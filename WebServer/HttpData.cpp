#include "HttpData.h"
#include "Channel.h"
#include "EventLoop.h"

/*--------------------拷贝控制成员实现------------------*/
HttpData::HttpData(EventLoop* sub_reactor,Channel* connfd_channel)
                        :p_sub_reactor_(sub_reactor),p_connfd_channel_(connfd_channel)
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

/*---------------------接口实现-------------------------*/
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

/*-----------------private成员函数实现-------------------*/
void HttpData::ReadHandler()
{
    /*读取数据并显示。ET模式下需一次性把数据读完*/
    char buffer[4096];
    memset(buffer,'\0',4096);
    int fd = p_connfd_channel_->GetFd();
    while(true)
    {
        /*fd是非阻塞的那么recv就是非阻塞调用。*/
        ssize_t ret = recv(fd,buffer,sizeof buffer,0);
        if(ret < 0)
        {
            /*!
                ET模式下，连接socket的可读事件就绪，表明缓冲区中有数据可读。所以，不用考虑
                那种由于一开始就没有数据可读从而返回EWOULDBLOCK的情况。当错误代码为EAGAIN
                或EWOULDBLOCK时，我们就认为数据已经读取完毕。
             */
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /*数据读完后，服务端准备向客户端写数据。此时，需要删除注册的EPOLLIN事件并注册EPOLLOUT事件*/
                __uint32_t old_option = p_connfd_channel_->GetEvents();
                __uint32_t new_option = old_option | EPOLLOUT | ~EPOLLIN;
                p_connfd_channel_->SetEvents(new_option);
                printf("get content: %s from socket %d\n",buffer,fd);
                break;
            }
            /*否则，发生错误，此时关闭连接*/
            DisConndHandler();
            break;
        }
        else if(ret ==0)
        {
            /*!
                一般情况下，recv返回0是由于客户端关闭连接导致的。客户端断开连接时，会同时触发可读事件。
                因此，没有对连接socket注册连接断开的回调函数，而是在此处做关闭连接处理。
             */
            DisConndHandler();
            break;
        }
        else
        {
            //std::cout<<"get "<<ret<<" bytes from " <<fd<<" which is: "<<buffer<<std::endl;
        }
    }
}

void HttpData::WriteHandler()
{
    printf("write data\n");
    /*写完数据之后，需要删除注册的EPOLLOUT事件，并重新注册EPOLLIN事件*/
    __uint32_t old_option = p_connfd_channel_->GetEvents();
    __uint32_t new_option = old_option | EPOLLIN | ~EPOLLOUT;
    p_connfd_channel_->SetEvents(new_option);
}

void HttpData::DisConndHandler()
{
    printf("client %d disconnect\n",p_connfd_channel_->GetFd());
    /*客户端断开连接时，服务器端也断开连接。此时，需将连接socket从事件池中删除*/
    p_sub_reactor_->DelFromEventChannePool(p_connfd_channel_);
}

void HttpData::ErrorHandler(int fd,int error_num,std::string msg)
{
    /*获取错误信息*/
    printf("get an error from client: %d\n",fd);
    char error[100];
    socklen_t length = sizeof error;
    memset(error,'\0',100);
    if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&length)<0) { printf("get socket error message failed\n");}

    /*向客户端发送错误信息*/
    send(fd,error,length,0);
}

void HttpData::ExpiredHandler()
{
    printf("client %d is silent for a while, preparing to shut it down\n",p_connfd_channel_->GetFd());
    DisConndHandler();
}