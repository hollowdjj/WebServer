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
    /*读取数据并显示.ET模式下需一次性把数据读完*/
    char buffer[4096];
    int fd = connfd_channel_->GetFd();
    while(true)
    {
        int ret = recv(fd,buffer,sizeof buffer,0);
        if(ret < 0)
        {
            /*错误代码为EAGAIN或EWOULDBLOCK时，表示数据已经读取完毕*/
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            else if(ret == 0) close(connfd_channel_->GetFd());
            else std::cout<<"get "<<ret<<" bytes from " <<fd<<std::endl;
        }
        else
        {
            std::cout<<"recieve data error"<<std::endl;
        }
    }
}

void HttpData::WriteHandler()
{
std::cout<<"write data"<<std::endl;
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