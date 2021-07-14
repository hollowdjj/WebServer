#include "HttpData.h"
#include "Channel.h"
#include "EventLoop.h"

/*-----------------------HttpData类-------------------------*/

HttpData::HttpData(EventLoop* sub_reactor,Channel* connfd_channel)
                        :p_sub_reactor_(sub_reactor),p_connfd_channel_(connfd_channel),p_timer_(nullptr)
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
    auto read_num = ReadData(fd,read_in_buffer,disconnect);
    if(read_num < 0 || disconnect) DisConndHandler();     //读取数据出错或者客户端关闭了连接时，关闭连接

    /*数据读完后，服务端准备向客户端写数据。此时，需要删除注册的EPOLLIN事件并注册EPOLLOUT事件*/
    __uint32_t old_option = p_connfd_channel_->GetEvents();
    __uint32_t new_option = old_option | EPOLLOUT | ~EPOLLIN;
    p_connfd_channel_->SetEvents(new_option);
    printf("get content: %s from socket %d\n",read_in_buffer.c_str(),fd);

    /*数据读取完毕后，还需要重新设置timer*/
    p_sub_reactor_->AdjustTimer(p_timer_,GlobalVar::timer_timeout);

    //TODO 解析Http请求报文
}

void HttpData::WriteHandler()
{
    /*向连接socket写数据*/
    int fd = p_connfd_channel_->GetFd();
    auto write_size = WriteData(fd,write_out_buffer);
    if(write_size < 0) DisConndHandler();    //若写数据出错，就关闭连接

    /*写完数据之后，需要删除注册的EPOLLOUT事件，并重新注册EPOLLIN事件*/
    __uint32_t old_option = p_connfd_channel_->GetEvents();
    __uint32_t new_option = old_option | EPOLLIN | ~EPOLLOUT;
    p_connfd_channel_->SetEvents(new_option);
}

void HttpData::DisConndHandler()
{
    printf("client %d disconnect\n",p_connfd_channel_->GetFd());
    /*客户端断开连接时，服务器端也断开连接。此时，需将连接socket从事件池中删除*/
    p_sub_reactor_->DelEpollEvent(p_connfd_channel_);
}

void HttpData::ErrorHandler(int fd,int error_num,std::string msg)
{
//    /*获取错误信息*/
//    printf("get an error from client: %d\n",fd);
//    char error[100];
//    socklen_t length = sizeof error;
//    memset(error,'\0',100);
//    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)<0) { printf("get socket error message failed\n");}
//
//    /*向客户端发送错误信息*/
//    send(fd,error,length,0);

      /*编写响应报文的entidy body*/
      std::string response_body;
      response_body += "";
      /*编写响应报文的header*/
      std::string response_header;
      response_header += "HTTP/1.1 " + std::to_string(error_num) + msg + "\r\n";   //状态行
      response_header += "Date: ";
      response_header += "Server: Hollow-Dai\r\n";
      response_header += "Content-Type: text/html\r\n";
      response_header += "Connection: Close\r\n";
      response_header += "Content-Length: " + std::to_string(response_body.size()) + "\r\n";
}

void HttpData::ExpiredHandler()
{
    printf("client %d is silent for a while, preparing to shut it down\n",p_connfd_channel_->GetFd());
    DisConndHandler();
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