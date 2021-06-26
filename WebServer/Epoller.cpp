#include "Epoller.h"


///////////////////////////
//   Global    Variables //
///////////////////////////
const int kEpollTimeOut = 10000;             //epoll超时时间(单位为毫秒)
const int kMaxActiveEventNum = 4096;         //最多监听4096个事件

Epoller::Epoller() : epollfd_(epoll_create1(EPOLL_CLOEXEC))
{
    /*!
        注意，这里没有使用epoll_create。这是因为，epoll_create函数的size参数只是一个参考
        实际上，内核epoll事件表是会动态增长的，因此没有必要使用epoll_create了
     */
    assert(epollfd_ != -1);
    active_events_.resize(kMaxActiveEventNum);
}

bool Epoller::AddEpollEvent(std::shared_ptr<Channel> event_channel)
{
    int fd = event_channel->GetFd();
    epoll_event event;
    bzero(&event,sizeof event);
    event.data.fd = fd;
    event.events = (event_channel->GetEvents() | EPOLLET);
    if(epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&event) < 0)
    {
        printf("epoll add error: %s", strerror(errno));
        return false;
    }
    else if(current_channel_num_ >= kMaxUserNum)
    {
        printf("add event to full SubReactor\n");
        return false;
    }
    /*向内核epoll事件表添加事件成功后才能将事件添加到事件池中*/
    events_channel_pool_[event.data.fd] = event_channel;
    ++current_channel_num_;
    return true;
}

bool Epoller::ModEpollEvent(std::shared_ptr<Channel> event_channel)
{
    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events |= (event_channel->GetEvents() | EPOLLET);

    if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&event) < 0)
    {
        printf("epoll mod error: %s\n", strerror(errno));
        return false;
    }
    /*只有修改内核epoll事件表成功后才能修改事件池*/
    //events_channel_poll_[fd] = event_channel;
    return true;
}

bool Epoller::DelEpollEvent(std::shared_ptr<Channel> event_channel)
{
    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = 0;

    if(epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,&event)<0)
    {
        printf("epoll del error: %s\n", strerror(errno));
        return false;
    }
    close(fd);
    events_channel_pool_[fd].reset();
    --current_channel_num_;
    return true;
}

std::vector<std::shared_ptr<Channel>> Epoller::GetActiveEvents()
{
    while(!stop_)
    {
        int active_event_num = epoll_wait(epollfd_,&active_events_[0],kMaxActiveEventNum,kEpollTimeOut);
        if(active_event_num < 0)
        {
            if(errno == EINTR) return {};
            printf("eopll wait error: %s\n", strerror(errno));
            return {};
        }
        else if(active_event_num == 0)
        {
            /*epoll_wait超时，这里暂时的处理方式是继续循环*/
            //printf("waiting for active events\n");
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
                printf("modification on emtpy Channel Object");
            }
        }
        return std::move(ret);
    }
    return {};
}

void Epoller::ClearEpoller()
{
    for (auto& i : events_channel_pool_)
    {
        if(i) DelEpollEvent(i);
    }
}