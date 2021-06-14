#include "Epoller.h"


///////////////////////////
//   Global    Variables //
///////////////////////////
const int kEpollTimeOut = 10000;             //epoll超时事件
const int kMaxActiveEventNum = 4096;         //最多监听4096个事件

Epoller::Epoller() : epollfd_(epoll_create1(EPOLL_CLOEXEC))
{
    /*!
        注意，这里没有使用epoll_create。这是因为，epoll_create函数的size参数只是一个参考
        实际上，内核epoll事件表是会动态增长的，因此没有必要使用epoll_create了
     */
    epollfd_ = epoll_create1(EPOLL_CLOEXEC);
    assert(epollfd_ > 0);
    active_events_.resize(kMaxActiveEventNum);
}

void Epoller::AddEpollEvent(std::shared_ptr<Channel> event_channel)
{
    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = (event_channel->GetEvents() | EPOLLET);

    if(epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&event) < 0)
    {
        std::cout<<"epoll add error: "<<errno<<std::endl;
        return;
    }
    /*向内核epoll事件表添加事件成功后才能将事件添加到事件池中*/
    events_channel_pool_[event.data.fd] = event_channel;
}

void Epoller::ModEpollEvent(std::shared_ptr<Channel> event_channel)
{
    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = (event_channel->GetEvents() | EPOLLET);
    
    if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&event) < 0)
    {
        std::cout<<"epoll mod error: "<<errno<<std::endl;
    }
    /*只有修改内核epoll事件表成功后才能修改事件池*/
    //events_channel_poll_[fd] = event_channel;
}

void Epoller::DelEpollEvent(std::shared_ptr<Channel> event_channel)
{
    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = (event_channel->GetEvents() | EPOLLET);

    if(epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,&event)<0)
    {
        std::cout<<"epoll del error: "<<errno<<std::endl;
        return;
    }
    events_channel_pool_[fd].reset();
}

std::vector<std::shared_ptr<Channel>> Epoller::GetActiveEvents()
{
    while(true)
    {
        int active_event_num = epoll_wait(epollfd_,&active_events_[0],kMaxActiveEventNum,kEpollTimeOut);
        if(active_event_num < 0)
        {
            std::cout<<"epoll_wait error: "<<errno<<std::endl;
            return {};
        }
        else if(active_event_num == 0)
        {
            /*epoll_wait超时，这里暂时的处理方式是继续循环*/
            std::cout<<"epoll_wait timeout"<<std::endl;
            continue;
        }

        /*根据就绪事件，找到事件池中相应的Channel并修改其revents_属性*/
        std::vector<std::shared_ptr<Channel>> ret(active_event_num);
        for (int i = 0; i < active_event_num; ++i)
        {
            int fd = active_events_[i].data.fd;
            auto channel = events_channel_pool_[fd];

            if(channel)
            {
                channel->SetRevents(active_events_[i].events);
                ret[i] = std::move(channel);
            }
            else
            {
                std::cout<<"modification on emtpy Channel Object"<<std::endl;
            }
        }
        return std::move(ret);
    }
}