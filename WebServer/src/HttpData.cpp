#include "HttpData.h"
#include "Channel.h"
#include "EventLoop.h"
#include <iomanip>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <regex>
/*-----------------------HttpData类-------------------------*/
HttpData::HttpData(EventLoop* sub_reactor,Channel* connfd_channel)
                        :p_sub_reactor_(sub_reactor),
                         p_connfd_channel_(connfd_channel),
                         request_msg_parse_state_(RequestMsgParseState::kStart)
{
    if(p_connfd_channel_)
    {
        /*设置回调函数*/
        p_connfd_channel_->SetReadHandler([this](){ReadHandler();});
        p_connfd_channel_->SetWriteHandler([this](){WriteHandler();});
        p_connfd_channel_->SetErrorHandler([this](){ErrorHandler();});
        p_connfd_channel_->SetDisconnHandler([this](){DisConndHandler();});
    }
    /*填充请求报文中方法字段与相应业务处理函数的映射*/
    method_proc_func_["GET"]  = [this](){return ProcessGETorHEAD();};
    method_proc_func_["HEAD"] = [this](){return ProcessGETorHEAD();};
    method_proc_func_["POST"] = [this](){return ProcessPOST();};
}

HttpData::~HttpData()
{
    /*do nothing 成员变量中的raw pointers不负责管理其所指向对象的生命周期*/
}

void HttpData::LinkTimer(Timer* p_timer)
{
    if(!p_timer)
    {
        ::GetLogger("../temp/log.txt")->error("can't link an empty timer");
        return;
    }
    p_timer_ = p_timer;
    p_timer_->SetExpiredHandler([this](){ExpiredHandler();});
}

void HttpData::ReadHandler()
{
    /*从连接socket读取数据*/
    int fd = p_connfd_channel_->GetFd();
    bool disconnect = false;
    auto read_num = ReadData(fd, read_in_buffer_, disconnect);
    if(read_num > 0) ::GetLogger("../temp/log.txt")->debug("client {} Request:\n{}\n", fd, read_in_buffer_.c_str());
    else if(read_num < 0 || disconnect)
    {
        /*read_num < 0读取数据错误可能是socket连接出了问题，这个时候最好由服务端主动断开连接*/
        DisConndHandler();
        return;
    }

    /*解析http请求报文*/
    bool finish = false;
    bool error = false;
    while(!finish && !error)      //finish和error同为false时才进入循环
    {
        switch (request_msg_parse_state_) {
            /*State1: 解析请求报文的请求行*/
            case RequestMsgParseState::kStart:{
                RequestLineParseState flag = ParseRequestLine();
                switch (flag) {
                    case RequestLineParseState::kParseAgain:                //未接收到完整的请求行，返回，等待下一波数据的到来
                        return;
                    case RequestLineParseState::kParseError:                //请求行语法错误，向客户端发送错误代码400并重置
                        SetHttpErrorMsg(fd, 400, "Bad Request: Request line has syntax error");
                        error = true;
                        break;
                    case RequestLineParseState::kParseSuccess:              //成功解析了请求行
                        request_msg_parse_state_ = RequestMsgParseState::kRequestLineOK;
                        break;                 //此时read_in_buffer_为：\r\n首部行 + 空行 + 实体
                }
            }break;
            /*State2: 解析请求报文的首部行*/
            case RequestMsgParseState::kRequestLineOK:{
                HeaderLinesParseState flag = ParseHeaderLines();
                switch (flag) {
                    case HeaderLinesParseState::kParseAgain:                //首部行数据不完整，返回，等待下一波数据到来
                        return;
                    case HeaderLinesParseState::kParseError:                //首部行语法错误，向客户端发送错误代码400并重置
                        SetHttpErrorMsg(fd, 400, "Bad Request: Header lines have syntax error");
                        error = true;
                        break;
                    case HeaderLinesParseState::kParseSuccess:              //成功解析了首部行
                        request_msg_parse_state_ = RequestMsgParseState::kHeaderLinesOK;
                        break;                //此时read_in_buffer_为：空行 + 实体(字节数应为Content-length + 2)
                }
            }break;
            /*State3: 对于POST请求，服务端要检查请求报文中的实体数据是否完整，而GET和HEAD则不用*/
            case RequestMsgParseState::kHeaderLinesOK:{
                if(fields_values_["method"] == "POST")
                    request_msg_parse_state_ = RequestMsgParseState::kCheckBody;
                else
                    request_msg_parse_state_ = RequestMsgParseState::kAnalysisRequest;
            }break;
            /*State4: 查询实体数据大小并判断实体数据是否全部读到了*/
            case RequestMsgParseState::kCheckBody:{
                //body的两相邻报文到达的间隔不能超过client_body_timeout_，否则超时。
                p_sub_reactor_->timewheel_.AdjustTimer(p_timer_,GlobalVar::client_body_timeout_);
                if(fields_values_.find("Content-Length") != fields_values_.end())
                {
                    int content_length = std::stoi(fields_values_["Content-Length"]);
                    if(content_length + 2 != read_in_buffer_.size())  return; //请求报文数据未全部接收，返回。
                }
                else
                {
                    //请求报文首部行中有语法错误，发送错误代码和信息并重置
                    SetHttpErrorMsg(fd, 400, "Bad Request: Lack of argument (Content-Length)");
                    return;
                }
                request_msg_parse_state_ = RequestMsgParseState::kAnalysisRequest;
            }break;
            /*State5: 分析客户端请求*/
            case RequestMsgParseState::kAnalysisRequest:{
                RequestMsgAnalysisState flag = AnalysisRequest();
                switch (flag) {
                    case RequestMsgAnalysisState::kAnalysisError:       //发生错误
                        error = true;
                        break;
                    case RequestMsgAnalysisState::kAnalysisSuccess:     //成功
                        request_msg_parse_state_ = RequestMsgParseState::kFinish;
                        break;
                }
            }break;
            /*State6: 请求报文全部解析完成，分析并写好了响应报文。结束while循环并向客户端发送响应报文*/
            case RequestMsgParseState::kFinish:
                finish = true;
                break;
        }
    }
    /*发送http响应报文*/
    WriteHandler();
}

void HttpData::WriteHandler()
{
    /*向连接socket写数据*/
    int fd = p_connfd_channel_->GetFd();
    auto total_num = write_out_buffer_.size() + 1; //调用c_str函数后，会最末尾加上\0，所以发送的数据总数要加1
    ssize_t write_sum = 0;
    while(true)
    {
        bool full;
        auto ret = WriteData(fd, write_out_buffer_,full);
        if(ret < 0)                       //写数据出错，断开连接
        {
            DisConndHandler();
            return;
        }
        write_sum += ret;
        if(full && write_sum < total_num) //发送缓冲区已写满，但数据还未全部发送完，则注册EPOLLOUT并返回等待epoll_wait返回再回调
        {
            MutexRegInOrOut(false);
            p_sub_reactor_->timewheel_.DelTimer(p_timer_);
            p_timer_ = nullptr;          //这里需要取消timer，避免因为发送缓冲区已满造成连接超时
            return;
        }
        if(write_sum == total_num) break;
    }

    /*发送完数据后，需删除EPOLLOUT事件并重新注册EPOLLIN事件*/
    MutexRegInOrOut(true);

    /*重置*/
    Reset();
}

void HttpData::DisConndHandler()
{
    int fd = p_connfd_channel_->GetFd();
    /*此时，需将连接socket从事件池中删除*/
    if(p_sub_reactor_->DelEpollEvent(p_connfd_channel_))
    {
        GlobalVar::DecTotalUserNum();
        ::GetLogger("../temp/log.txt")->info("Client {} disconnect, current user number: {}", fd, GlobalVar::GetTotalUserNum());
    }
}

void HttpData::ErrorHandler()
{
    ::GetLogger("../temp/log.txt")->error("Get an error form connect socket: {}", strerror(errno));
    DisConndHandler();
}

void HttpData::SetHttpErrorMsg(int fd, int error_num, std::string msg)
{
    ::GetLogger("../temp/log.txt")->debug("Client {} http error: {} {}", fd, error_num, msg.c_str());

    /*编写响应报文的entidy body*/
    std::string response_body;
    response_body += "<html><title>错误</title>";
    response_body += "<body bgcolor=\"ffffff\">";
    response_body += std::to_string(error_num) + msg;
    response_body += "<hr><em> Hollow-Dai Server</em>\n</body></html>";

    /*编写响应报文的header*/
    std::string response_header;
    response_header += "HTTP/1.1 " + std::to_string(error_num) + " "+ msg + "\r\n";
    response_header += "Date: " + GetTime() + "\r\n";
    response_header += "Server: Hollow-Dai\r\n";
    response_header += "Content-Type: text/html\r\n";
    response_header += "Connection: close\r\n";
    response_header += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
    response_header += "\r\n";
    write_out_buffer_ = response_header + response_body;
}

void HttpData::ExpiredHandler()
{
    int fd = p_connfd_channel_->GetFd();
    ::GetLogger("../temp/log.txt")->debug("client {} timeout, shut it down", fd);
    SetHttpErrorMsg(fd, 408, "Request Time-out");
    WriteHandler();
}

RequestLineParseState HttpData::ParseRequestLine()
{
    /*!
        Http请求报文的请求行的格式为：
        请求方法|空格|URI|空格|协议版本|回车符|换行符。其中URI以‘/’开始。例如：
        GET /index.html HTTP/1.1\r\n
     */

    /*必须要接收到完整的请求行才能开始解析*/
    auto pos = read_in_buffer_.find("\r\n");
    if(pos == std::string::npos) return RequestLineParseState::kParseAgain;

    /*从read_in_buffer中截取出请求行*/
    auto request_line = read_in_buffer_.substr(0,pos);
    read_in_buffer_.erase(0,pos);        //不要把\r\n也截取了，这样解析首部行时会方便一点

    /*使用正则表达式解析http请求报文的request line*/
    std::regex r(R"(^(GET|HEAD|POST)\s(\S*)\s(HTTP\/1\.[0|1])$)");
    std::smatch results;
    std::regex_match(request_line,results,r);
    if(results.empty()) return RequestLineParseState::kParseError;

    /*保存method URI以及version*/
    fields_values_["method"] = results[1];
    fields_values_["URI"] = results[2];
    fields_values_["version"] = results[3];
    
    return RequestLineParseState::kParseSuccess;
}

HeaderLinesParseState HttpData::ParseHeaderLines()
{
    /*!
        只检查每一行的格式。首部行的格式必须为"字段名：|空格|字段值|cr|lf"
        对首部字段是否正确，字段值是否正确均不做判断。
        另外，此时的read_in_buffer_为：\r\n首部行 + 空行 + 实体
     */
    auto FormatCheck = [this](std::string& target) -> bool
    {
        std::regex r(R"(^([A-Z]\S*)\:\s(.+)$)");
        std::smatch results;
        std::regex_match(target,results,r);
        if(results.empty()) return false;
        /*保存首部字段和对应的值*/
        fields_values_[results[1]] = results[2];
        return true;
    };

    /*判断每个首部行的格式是否正确*/
    decltype(read_in_buffer_.size()) pos_cr = 0,old_pos = 0;
    while(true)
    {
        old_pos = pos_cr;
        pos_cr = read_in_buffer_.find("\r\n",pos_cr + 2);
        if(pos_cr == std::string::npos) return HeaderLinesParseState::kParseAgain;   //接收到完整的首部行后才开始解析
        else if(pos_cr == old_pos + 2)
        {
            //解析到首部行和实体之间的空行了，则说明首部行格式没问题且数据完整。此时清除已解析的header(保留空行和实体)
            read_in_buffer_ = read_in_buffer_.substr(pos_cr);
            break;
        }
        std::string header_line = read_in_buffer_.substr(old_pos+2,pos_cr-old_pos-2); //检查首部行格式
        if(!FormatCheck(header_line)) return HeaderLinesParseState::kParseError;
    }

    return HeaderLinesParseState::kParseSuccess;
}

RequestMsgAnalysisState HttpData::AnalysisRequest()
{
    return method_proc_func_[fields_values_["method"]]();
}

RequestMsgAnalysisState HttpData::ProcessGETorHEAD()
{
    /*HEAD方法与GET方法一样，只是不返回报文主体部分。一般用来确认URI的有效性以及资源更新的日期时间等。*/
    
    FillPartOfResponseMsg();  //编写响应报文中和请求报文中的方法字段无关的内容

    /*解析客户端请求的资源名*/
    std::string file_name = fields_values_["URI"].substr(fields_values_["URI"].find_last_of('/')+1);
    
    /*echo test*/
    if(file_name == "hello")
    {
        std::string body = "Hello World";
        write_out_buffer_ += std::string("Content-type: text/plain\r\n");
        write_out_buffer_ += std::string("Content-Length: ") + std::to_string(body.size()) + "\r\n\r\n" + body;
        return RequestMsgAnalysisState::kAnalysisSuccess;
    }
    else if(file_name == "favicon.ico")
    {
        write_out_buffer_ += "Content-Type: image/png\r\n";
        write_out_buffer_ += "Content-Length: " + std::to_string(sizeof(GlobalVar::favicon)) + "\r\n";
        write_out_buffer_ += "\r\n" + std::string(GlobalVar::favicon);
        return RequestMsgAnalysisState::kAnalysisSuccess;
    }

    /*首部行的Content-Type字段*/
    std::string::size_type pos_dot = file_name.find('.');
    std::string file_type = (pos_dot == std::string::npos ?
                            SourceMap::GetMime("default") : SourceMap::GetMime(file_name.substr(pos_dot)));  //文件类型

    write_out_buffer_ += "Content-Type: " + file_type + "\r\n";
    /*首部行的Content-Length字段*/
    std::string dir = "../resource/" + file_name;
    int fd = p_connfd_channel_->GetFd();
    struct stat file{};
    if(stat(dir.c_str(),&file) < 0)
    {
        SetHttpErrorMsg(fd, 404, "Not Found!");
        return RequestMsgAnalysisState::kAnalysisError;
    }
    write_out_buffer_ += "Content-Length: " + std::to_string(file.st_size) + "\r\n";
    /*首部行结束*/

    /*HEAD方法不需要实体*/
    if(fields_values_["method"] == "HEAD")
    {
        write_out_buffer_ += "\r\n";
        return RequestMsgAnalysisState::kAnalysisSuccess;
    }
    
    /*对GET方法，打开并读取文件，然后填充报文实体*/
    int file_fd = open(dir.c_str(),O_RDONLY,0);
    if(file_fd < 0)
    {
        SetHttpErrorMsg(fd, 404, "Not Found!");
        return RequestMsgAnalysisState::kAnalysisError;
    }
    /*读取文件*/
    void* mmap_ret = mmap(nullptr,file.st_size,PROT_READ,MAP_PRIVATE,file_fd,0);  //使用mmap避免拷贝
    close(file_fd);
    if(mmap_ret == (void*)-1) //读取文件出错
    {
        munmap(mmap_ret,file.st_size);
        SetHttpErrorMsg(fd, 404, "Not Found!");
        return RequestMsgAnalysisState::kAnalysisError;
    }
    char* file_buffer = static_cast<char*>(mmap_ret);
    write_out_buffer_ += "\r\n" + std::string(file_buffer,file_buffer + file.st_size);
    munmap(mmap_ret,file.st_size);
    return RequestMsgAnalysisState::kAnalysisSuccess;
}

RequestMsgAnalysisState HttpData::ProcessPOST()
{
    /*!
        POST方法用于客户端向服务端提交数据。这里简单将请求报文实体中的字符串
        全部转换成大写，然后发送回客户端。
     */
     auto body = read_in_buffer_.substr(2);
     for (auto& item : body)
     {
         item = static_cast<char>(std::toupper(static_cast<unsigned char>(item)));
     }
     FillPartOfResponseMsg();
     write_out_buffer_ += std::string("Content-Type: text/plain\r\n") + "Content-Length: " + std::to_string(body.size()) + "\r\n";
     write_out_buffer_ += "\r\n" + body;

     return RequestMsgAnalysisState::kAnalysisSuccess;
}

void HttpData::MutexRegInOrOut(bool epollin)
{
    /*!
     epollin为true时，表示注册EPOLLIN而不注册EPOLLOUT，反之。同时，在使用EPOLL_CTL_MOD时，events必须全部重写，不能采
     用取反然后或等于的形式。比如：
     __uint32_t old_option = p_connfd_channel_->GetEvents();
     old_option |= ~EPOLLIN; old_option |= EPOLLOUT
     p_connfd_channel_->SetEvents(old_option);
     采取上述这种写法会报invalid argument错误。然而poll是可以这样写的。至于epoll为什么不能，还有待查证。
     */
    __uint32_t events;
    if(epollin) events = EPOLLIN | EPOLLRDHUP | EPOLLERR;
    else events = EPOLLOUT | EPOLLRDHUP | EPOLLERR;
    p_connfd_channel_->SetEvents(events);
    p_sub_reactor_->ModEpollEvent(p_connfd_channel_);
}

void HttpData::Reset()
{
    /*长连接则重置超时时间，短连接则关闭连接*/
    if(fields_values_["Connection"] == "keep-alive" || fields_values_["Connection"] == "Keep-Alive")
    {
        auto timeout = GlobalVar::keep_alive_timeout_;
        if(!p_timer_) p_sub_reactor_->timewheel_.AddTimer(timeout);
        else p_sub_reactor_->timewheel_.AdjustTimer(p_timer_,timeout);
    }
    else
    {
        DisConndHandler();
        return;
    }
    /*重置连接信息*/
    read_in_buffer_.clear();
    write_out_buffer_.clear();
    fields_values_.clear();
    request_msg_parse_state_ = RequestMsgParseState::kStart;
}

void HttpData::FillPartOfResponseMsg()
{
    /*状态行*/
    std::string status_line = fields_values_["version"] + " 200 OK\r\n";

    /*首部行的Date字段*/
    std::string header_lines;
    header_lines += "Date: " + GetTime() + "\r\n";
    /*首部行的Server字段*/
    header_lines += "Server: Hollow-Dai\r\n";
    /*首部行的Connection字段*/
    if(fields_values_["Connection"] == "keep-alive" || fields_values_["Connection"] == "Keep-Alive")
    {
        header_lines += "Connection: keep-alive\r\n" + std::string("Keep-Alive: timeout=")
                        + std::to_string(GlobalVar::keep_alive_timeout_.count()) + "\r\n";
    }
    else
    {
        header_lines +="Connection: close\r\n";
    }

    write_out_buffer_ = status_line + header_lines;
}

/*-----------------------SourceMap类-------------------------*/
std::unordered_map<std::string,std::string> SourceMap::mime_{};
std::once_flag SourceMap::flag_{};

void SourceMap::Init()
{
    /*文件类型*/
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

std::string SourceMap::GetMime(const std::string& suffix)
{
    std::call_once(SourceMap::flag_,SourceMap::Init);     //Init函数只会调用一次
    return mime_.find(suffix) == mime_.end() ? mime_["default"] : mime_[suffix];
}