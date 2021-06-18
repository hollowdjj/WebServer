#include "HttpData.h"

using namespace std::placeholders;
HttpData::HttpData(std::shared_ptr<EventLoop> sub_reactor, std::shared_ptr<Channel> connfd_channel)
                        :sub_reactor_(std::move(sub_reactor)),connfd_channel_(std::move(connfd_channel))
{
    if(connfd_channel_)
    {
        /*设置回调函数*/
        connfd_channel_->SetReadHandler([this](){ReadHandler();});
        connfd_channel_->SetWriteHandler([this](){WriteHandler();});
        connfd_channel_->SetDisconnHandler([this](){DisConndHandler();});
        //connfd_channel_->SetErrorHandler(std::bind(&HttpData::ErrorHandler,this,10,10,"1"));
    }
}

void HttpData::ReadHandler()
{
    /*读取数据并显示。ET模式下需一次性把数据读完*/
    char buffer[4096];
    int fd = connfd_channel_->GetFd();
    while(true)
    {
        /*fd是非阻塞的那么recv就是非阻塞调用。缓冲区中无可读数据时，立即返回EWOULDBLOCK*/
        ssize_t ret = recv(fd,buffer,sizeof buffer,0);
        if(ret < 0)
        {
            /*错误代码为EAGAIN或EWOULDBLOCK时，表示数据已经读取完毕*/
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /*数据读完后，服务端准备向客户端写数据。此时，需要删除注册的EPOLLIN事件并注册EPOLLOUT事件*/
                __uint32_t old_option = connfd_channel_->GetEvents();
                __uint32_t new_option = old_option | EPOLLOUT | ~EPOLLIN;
                connfd_channel_->SetEvents(new_option);
                break;
            }
            /*否则，发生错误，此时关闭连接*/
            DisConndHandler();
            break;
        }
        /*一般情况下，recv返回0是由于客户端关闭连接导致的。对connfd注册了专门的关闭连接回调函数，所以这里什么都不做*/
        else if(ret ==0)
        {
            DisConndHandler();
        }
        else
        {
            std::cout<<"get "<<ret<<" bytes from " <<fd<<std::endl;
        }
    }
}

void HttpData::WriteHandler()
{
    std::cout<<"write data"<<std::endl;
    /*写完数据之后，需要删除注册的EPOLLOUT事件，并重新注册EPOLLIN事件*/
    __uint32_t old_option = connfd_channel_->GetEvents();
    __uint32_t new_option = old_option | EPOLLIN | ~EPOLLOUT;
    connfd_channel_->SetEvents(new_option);
}

void HttpData::DisConndHandler()
{
    std::cout<<"client disconnect"<<std::endl;
    /*客户端断开连接时，服务器端也断开连接。此时，需将连接socket从事件池中删除*/
    sub_reactor_->DelFromEventChannePool(connfd_channel_);
}

void HttpData::ErrorHandler(int fd,int error_num,std::string msg)
{
    /*获取错误信息*/
    std::cout<<"get an error from client "<<fd<<" "<<error_num<<" "<<msg<<std::endl;
    char error[100];
    socklen_t length = sizeof error;
    memset(error,'\0',100);
    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)<0) {std::cout<<"get socket error message failed"<<std::endl;}

    /*向客户端发送错误信息*/
    send(fd,error,length,0);
}