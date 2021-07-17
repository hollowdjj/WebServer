#include "HttpData.h"
#include "Channel.h"
#include "EventLoop.h"
#include <ctime>
#include <iomanip>

/*-----------------------HttpData类-------------------------*/

HttpData::HttpData(EventLoop* sub_reactor,Channel* connfd_channel)
                        :p_sub_reactor_(sub_reactor),
                         p_connfd_channel_(connfd_channel),
                         p_timer_(nullptr),
                         http_method_(HttpMethod::kEmpty)
{
    if(p_connfd_channel_)
    {
        /*设置回调函数*/
        p_connfd_channel_->SetReadHandler([this](){ReadHandler();});
        p_connfd_channel_->SetWriteHandler([this](){WriteHandler();});
    }
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
        //TODO 对于由服务端读取socket数据或向socket写数据时发送错误的情况，最好先发送一个error信息给客户端\
               再关闭连接。而对于超时，则也是先发送一个error告诉客户端超时了，然后再关闭连接
        /*read_num < 0读取数据错误可能是socket连接出了问题，这个时候最好由服务端主动断开连接*/
        DisConndHandler();
        return;
    }

    /*解析Http请求报文*/
    RequestLineParseState flag = ParseRequestLine();
    if(flag == RequestLineParseState::kParseError) ErrorHandler(fd,400,"Bad Request");
    printf("get content: %s from socket %d\n", read_in_buffer_.c_str(), fd);
    //TODO 解析首部行
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
    
    /*重新计时*/
    p_sub_reactor_->AdjustTimer(p_timer_,GlobalVar::timer_timeout);
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
}

void HttpData::ExpiredHandler()
{
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
    if(pos == std::string::npos) return RequestLineParseState::kParseError;

    /*从read_in_buffer中截取出请求行*/
    auto request_line = read_in_buffer_.substr(0,pos);
    read_in_buffer_.erase(0,pos+2);

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

void HttpData::MutexRegInOrOut(bool epollin)
{
    /*epollin为true时，表示注册EPOLLIN而不注册EPOLLOUT，反之。*/
    __uint32_t old_option = p_connfd_channel_->GetEvents();
    __uint32_t new_option;
    if(epollin) new_option = old_option | EPOLLIN | ~EPOLLOUT;
    else new_option = old_option | ~EPOLLIN | EPOLLOUT;
    p_connfd_channel_->SetEvents(new_option);
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