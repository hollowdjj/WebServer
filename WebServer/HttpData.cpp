#include "HttpData.h"
#include "Channel.h"
#include "EventLoop.h"
#include <ctime>
#include <iomanip>

/*-----------------------HttpData类-------------------------*/

HttpData::HttpData(EventLoop* sub_reactor,Channel* connfd_channel)
                        :p_sub_reactor_(sub_reactor),
                         p_connfd_channel_(connfd_channel),
                         http_method_(HttpMethod::kEmpty),
                         http_version_(HttpVersion::kEmpty),
                         request_msg_parse_state_(RequestMsgParseState::kStart)
{
    if(p_connfd_channel_)
    {
        /*设置回调函数*/
        p_connfd_channel_->SetReadHandler([this](){ReadHandler();});
        p_connfd_channel_->SetWriteHandler([this](){WriteHandler();});
    }
    /*填充请求报文中方法字段与相应业务处理函数的映射*/
    method_proc_func_[HttpMethod::kGet]  = [this](){return ProcessGETorHEAD();};
    method_proc_func_[HttpMethod::kHead] = [this](){return ProcessGETorHEAD();};
    method_proc_func_[HttpMethod::kPost] = [this](){return ProcessPOST();};
}

HttpData::~HttpData()
{
    /*do nothing 成员变量中的raw pointers不负责管理其所指向对象的生命周期*/
}

void HttpData::LinkTimer(Timer* p_timer)
{
    if(!p_timer)
    {
        printf("can't link an empty timer\n");
        return;
    }
    p_timer_ = p_timer;
    p_timer_->SetExpiredHandler([this](){ExpiredHandler();});
}

void HttpData::ReadHandler()
{
    /*调整定时器以延迟该连接被关闭的时间*/
    p_sub_reactor_->AdjustTimer(p_timer_,GlobalVar::timer_timeout);

    /*从连接socket读取数据*/
    int fd = p_connfd_channel_->GetFd();
    bool disconnect = false;
    auto read_num = ReadData(fd, read_in_buffer_, disconnect);
    printf("client %d Request:\n%s\n",fd,read_in_buffer_.c_str());
    if(read_num < 0 || disconnect)
    {
        /*read_num < 0读取数据错误可能是socket连接出了问题，这个时候最好由服务端主动断开连接*/
        DisConndHandler();
        return;
    }

    /*解析http请求报文*/
    bool finish = false;
    while(!finish)
    {
        switch (request_msg_parse_state_) {
            /*State1: 解析请求报文的请求行*/
            case RequestMsgParseState::kStart:{
                RequestLineParseState flag = ParseRequestLine();
                switch (flag) {
                    case RequestLineParseState::kParseAgain:                //未接收到完整的请求行，返回，等待下一波数据的到来
                        return;
                    case RequestLineParseState::kParseError:                //请求行语法错误，向客户端发送错误代码400并重置
                        ErrorHandler(fd, 400, "Bad Request: Request line has syntax error");
                        return;
                    case RequestLineParseState::kParseSuccess:              //成功解析了请求行
                        request_msg_parse_state_ = RequestMsgParseState::kRequestLineOK;
                        break;
                }
            }break;
            /*State2: 解析请求报文的首部行*/
            case RequestMsgParseState::kRequestLineOK:{
                HeaderLinesParseState flag = ParseHeaderLines();
                switch (flag) {
                    case HeaderLinesParseState::kParseAgain:                //首部行数据不完整，返回，等待下一波数据到来
                        return;
                    case HeaderLinesParseState::kParseError:                //首部行语法错误，向客户端发送错误代码400并重置
                        ErrorHandler(fd,400,"Bad Request: Header lines have syntax error");
                        return;
                    case HeaderLinesParseState::kParseSuccess:              //成功解析了首部行
                        request_msg_parse_state_ = RequestMsgParseState::kHeaderLinesOK;
                        break;
                }
            }break;
            /*State3: 对于POST请求，服务端要检查请求报文中的实体数据是否完整，而GET和HEAD则不用*/
            case RequestMsgParseState::kHeaderLinesOK:{
                if(http_method_ == HttpMethod::kPost)
                    request_msg_parse_state_ = RequestMsgParseState::kCheckBody;
                else
                    request_msg_parse_state_ = RequestMsgParseState::kAnalysisRequest;
            }break;
            /*State4: 查询实体数据大小并判断实体数据是否全部读到了*/
            case RequestMsgParseState::kCheckBody:{
                if(headers_values_.find("Content-Length") != headers_values_.end())
                {
                    int content_length = std::stoi(headers_values_["Content-Length"]);
                    if(content_length + 2 != read_in_buffer_.size()) return;     //请求报文数据未全部接收，返回，等待数据到来
                }
                else
                {
                    //请求报文首部行中有语法错误，发送错误代码和信息并重置
                    ErrorHandler(fd,400,"Bad Request: Lack of argument (Content-Length)");
                    return;
                }
                request_msg_parse_state_ = RequestMsgParseState::kAnalysisRequest;
            }break;
            /*State5: 分析客户端请求*/
            case RequestMsgParseState::kAnalysisRequest:{
                RequestMsgAnalysisState flag = AnalysisRequest();
                switch (flag) {
                    case RequestMsgAnalysisState::kAnalysisError:       //发生错误，错误代码机信息的发送在业务函数中完成。
                        return;
                    case RequestMsgAnalysisState::kAnalysisSuccess:     //成功。相应操作，如数据发送也在业务函数中完成。
                        request_msg_parse_state_ = RequestMsgParseState::kFinish;
                        break;
                }
            }break;
            /*State6: 请求报文全部解析完成，结束while循环*/
            case RequestMsgParseState::kFinish:
                finish = true;
                break;
        }
    }
}

void HttpData::WriteHandler()
{
    /*调整定时器以延迟该连接被关闭的时间*/
    p_sub_reactor_->AdjustTimer(p_timer_,GlobalVar::timer_timeout);

    /*向服务端发送响应报文时，需要删除注册的EPOLLIN事件并注册EPOLLOUT事件*/
    MutexRegInOrOut(false);

    /*向连接socket写数据*/
    int fd = p_connfd_channel_->GetFd();
    while(true)
    {
        /*由于是正常的响应报文，所以一定要把数据完全写出*/
        auto ret = WriteData(fd, write_out_buffer_);
        if(ret < 0)
        {
            DisConndHandler();
            break;
        }
        else if(ret < write_out_buffer_.size()) continue;
    }

    /*发送完数据后，需删除EPOLLOUT事件并重新注册EPOLLIN事件*/
    MutexRegInOrOut(true);
}

void HttpData::DisConndHandler()
{
    printf("client %d disconnect\n",p_connfd_channel_->GetFd());
    /*客户端断开连接时，服务器端也断开连接。此时，需将连接socket从事件池中删除*/
    p_sub_reactor_->DelEpollEvent(p_connfd_channel_);
}

void HttpData::ErrorHandler(int fd,int error_num,std::string msg)
{
    /*调整定时器以延迟该连接被关闭的时间*/
    p_sub_reactor_->AdjustTimer(p_timer_,GlobalVar::timer_timeout);

    /*向服务端发送响应报文时，需要删除注册的EPOLLIN事件并注册EPOLLOUT事件*/
    MutexRegInOrOut(false);

    /*编写响应报文的entidy body*/
    std::string response_body;
    response_body += "<html><title>错误</title>";
    response_body += "<body bgcolor=\"ffffff\">";
    response_body += std::to_string(error_num) + msg;
    response_body += "<hr><em> Hollow-Dai Server</em>\n</body></html>";

    /*编写响应报文的header*/
    std::string response_header;
    std::time_t t = std::time(nullptr);
    auto time = std::ctime(&t);
    response_header += "HTTP/1.1 " + std::to_string(error_num) + msg + "\r\n";
    response_header += "Date: " + std::string(time) + " GMT\r\n";
    response_header += "Server: Hollow-Dai\r\n";
    response_header += "Content-Type: text/html\r\n";
    response_header += "Connection: close\r\n";
    response_header += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
    response_header += "\r\n";
    
    /*向客户端发送响应报文*/
    std::string response_buffer = response_header + response_body;
    while(WriteData(fd,response_buffer));       //尽可能地向客户端发送数据，没写完就算了，不进行错误处理。
    
    /*发送完数据后，需删除EPOLLOUT事件并重新注册EPOLLIN事件*/
    MutexRegInOrOut(true);

    /*重置*/
    Reset();
}

void HttpData::ExpiredHandler()
{
    //TODO 告知客户端已超时
    printf("client %d is silent for a while, preparing to shut it down\n",p_connfd_channel_->GetFd());
    DisConndHandler();
}

RequestLineParseState HttpData::ParseRequestLine()
{
    /*!
        Http请求报文的请求行的格式为：
        请求方法|空格|URL|空格|协议版本|回车符|换行符。其中URL以‘/’开始。例如：
        GET /index.html HTTP/1.1\r\n
     */

    /*必须要接收到完整的请求行才能开始解析*/
    auto pos = read_in_buffer_.find("\r\n");
    if(pos == std::string::npos) return RequestLineParseState::kParseAgain;

    /*从read_in_buffer中截取出请求行*/
    auto request_line = read_in_buffer_.substr(0,pos);
    read_in_buffer_.erase(0,pos);        //不要把\r\n也截取了，这样解析首部行时会方便一点

    /*判断方法字段是GET POST还是HEAD*/
    decltype(pos) pos_method;
    std::string method;
    if((pos_method = request_line.find("GET")) == 0)
    {
        method = "GET";
        http_method_ = HttpMethod::kGet;
    }
    else if((pos_method = request_line.find("POST")) == 0)
    {
        method = "POST";
        http_method_ = HttpMethod::kPost;
    }
    else if((pos_method = request_line.find("HEAD")) == 0)
    {
        method = "HEAD";
        http_method_ = HttpMethod::kHead;
    }
    else return RequestLineParseState::kParseError;

    /*解析URL*/
    decltype(pos_method) pos_space,pos_slash,pos_http;
    if((pos_space = request_line.find(' ')) == std::string::npos
       || pos_space != method.size()                                //方法字段后必须有个空格
       || (pos_slash = request_line.find('/',pos_space)) == std::string::npos
       || pos_slash != pos_space + 1                                //URL必须紧接着方法字段后面的那个空格且以斜杠开头
       || (pos_http = request_line.rfind("HTTP/",pos)) == std::string::npos
       || pos_http + 8 != pos)                                      //HTTP字段必须满足HTTP/0.0\r\n的格式
    {
        return RequestLineParseState::kParseError;
    }
    else
    {
        filename_ = request_line.substr(pos_slash+1,pos_http-pos_slash-2);
    }
    /*解析http协议的版本号*/
    std::string version;
    version = request_line.substr(pos-3,pos);
    if(version == "1.0") http_version_ = HttpVersion::kHttp10;
    else if(version == "1.1") http_version_ = HttpVersion::kHttp11;
    else return RequestLineParseState::kParseError;

    return RequestLineParseState::kParseSuccess;
}

HeaderLinesParseState HttpData::ParseHeaderLines()
{
    /*!
        只检查每一行的格式。首部行的格式必须为"字段名：|空格|字段值|cr|lf"
        对首部字段是否正确，字段值是否正确均不做判断。
     */
    auto FormatCheck = [this](std::string& target) -> bool
    {
        auto pos_colon = target.find(':');
        if(islower(target[0])                     //字段名的首字母必须大写
          ||pos_colon == std::string::npos        //字段名后必须有冒号
          || target[pos_colon+1] != ' '           //冒号后面必须紧跟一个空格
          || pos_colon+1 == target.size()-1)      //空格后面必须要有值
            return false;

        /*保存首部字段和对应的值*/
        std::string header = target.substr(0,pos_colon);
        std::string value = target.substr(pos_colon+2,target.size()-1-pos_colon-2+1);
        headers_values_[header] = value;
        return true;
    };

    /*判断每个首部行的格式是否正确*/
    decltype(read_in_buffer_.size()) pos_cr = 0,old_pos = 0;
    while(pos_cr != std::string::npos)
    {
        old_pos = pos_cr;
        //接收到完整的首部行才开始解析
        if((pos_cr = read_in_buffer_.find("\r\n",pos_cr + 2)) == std::string::npos) break;
        if(pos_cr == old_pos + 2)
        {
            //解析到空行了，则说明首部行格式没问题且数据完整。此时清除已解析的header
            read_in_buffer_ = read_in_buffer_.substr(pos_cr);
            return HeaderLinesParseState::kParseSuccess;
        }

        std::string header_line = read_in_buffer_.substr(old_pos+2,pos_cr-old_pos-2);
        if(!FormatCheck(header_line)) return HeaderLinesParseState::kParseError;
    } //跳出循环时，old_pos为最后一个完整首部行\r的索引或者0

    /*没有解析到空行，且目前已解析了的首部行格式均正确，说明请求报文中的后续数据还在传输中*/
    read_in_buffer_ = read_in_buffer_.substr(old_pos);  //清除已经解析了的完整首部行
    return HeaderLinesParseState::kParseAgain;
}

RequestMsgAnalysisState HttpData::AnalysisRequest()
{
    return method_proc_func_[http_method_]();
}

RequestMsgAnalysisState HttpData::ProcessGETorHEAD()
{
    /*!
        HEAD方法与GET方法一样，只是不返回报文主体部分。
        一般用来确认URI的有效性以及资源更新的日期时间等。
     */

    /*响应报文的状态行*/
    std::string version;
    if(http_version_ == HttpVersion::kHttp10)      version = "HTTP/1.0";
    else if(http_version_ == HttpVersion::kHttp11) version = "HTTP/1.1";
    std::string status_line = version + " 200 OK\r\n";

    /*响应报文的首部行*/
    std::string header_lines;
    if(headers_values_["Connection"] == "keep-alive")        //keep-alive字段
    {
        keep_alive_ = true;
        header_lines += "Connection: keep-alive\r\n" + std::string("Keep-Alive: timeout=")
                + std::to_string(GlobalVar::keep_alive_timeout.count()) + "\r\n";
    }
    else
    {
        keep_alive_ = false;
        header_lines +="Connection: close\r\n";
    }
    std::string::size_type pos_dot = filename_.find('.');
    std::string file_type = pos_dot == std::string::npos ?
                            MimeType::GetMime("default") :  MimeType::GetMime(filename_.substr(pos_dot));  //文件类型

    //echo test
    if(filename_ == "hello")
    {
        header_lines += "Content-type: text/plain\r\n\r\nHello World";
        return RequestMsgAnalysisState::kAnalysisSuccess;
    }
    else if(filename_ == "favicon.ico")
    {
        header_lines += "Content-Type: image/png\r\n";
        header_lines += "Content-Length: " + std::to_string(strlen(GlobalVar::favicon));
    }

}

RequestMsgAnalysisState HttpData::ProcessPOST()
{

}
void HttpData::MutexRegInOrOut(bool epollin)
{
    /*epollin为true时，表示注册EPOLLIN而不注册EPOLLOUT，反之。*/
    __uint32_t old_option = p_connfd_channel_->GetEvents();
    __uint32_t new_option;
    if(epollin) new_option = old_option | EPOLLIN | ~EPOLLOUT;
    else new_option = old_option | ~EPOLLIN | EPOLLOUT;
    p_connfd_channel_->SetEvents(new_option);
}

void HttpData::Reset()
{
    read_in_buffer_.clear();
    write_out_buffer_.clear();
    filename_.clear();
    headers_values_.clear();
    request_msg_parse_state_ = RequestMsgParseState::kStart;
    http_method_ = HttpMethod::kEmpty;
    http_version_ = HttpVersion::kEmpty;
    p_sub_reactor_->AdjustTimer(p_timer_,GlobalVar::timer_timeout);
}

/*-----------------------MimeType类-------------------------*/
std::unordered_map<std::string,std::string> MimeType::mime_{};
std::once_flag MimeType::flag_{};

void MimeType::Init()
{
    mime_[".html"] = "text/html";
    mime_[".avi"] = "video/x-msvideo";
    mime_[".bmp"] = "image/bmp";
    mime_[".c"] = "text/plain";
    mime_[".doc"] = "application/msword";
    mime_[".gif"] = "image/gif";
    mime_[".gz"] = "application/x-gzip";
    mime_[".htm"] = "text/html";
    mime_[".ico"] = "image/x-icon";
    mime_[".jpg"] = "image/jpeg";
    mime_[".png"] = "image/png";
    mime_[".txt"] = "text/plain";
    mime_[".mp3"] = "audio/mp3";
    mime_["default"] = "text/html";
}

std::string MimeType::GetMime(const std::string& suffix)
{
    std::call_once(MimeType::flag_,MimeType::Init);     //Init函数只会调用一次
    return mime_.find(suffix) == mime_.end() ? mime_["default"] : mime_[suffix];
}