/*User-define Headers*/
#include "Epoller.h"
#include "Channel.h"
#include "HttpData.h"

///////////////////////////
//   Global    Variables //
///////////////////////////
const int kEpollTimeOut = 10000;             //epoll超时时间10秒(单位为毫秒)
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

Epoller::~Epoller()
{
    close(epollfd_);
}

bool Epoller::AddEpollEvent(Channel* event_channel)
{
    /*每一个连接socket都必须设置一个表示HttpData对象的holder*/
    if(!event_channel) return false;
    auto holder = event_channel->GetHolder();
    if(!event_channel->IsListenfd() && holder == nullptr) return false;

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
    /*向内核epoll事件表添加事件成功后才能将事件添加到事件池中并设置定时器*/
    events_channel_pool_[event.data.fd] = std::unique_ptr<Channel>(event_channel);
    http_data_pool_[event.data.fd] = std::unique_ptr<HttpData>(holder);
    auto p_timer = timewheel_.AddTimer(std::chrono::seconds(5));
    event_channel->GetHolder()->LinkTimer(p_timer);
    return true;
}

bool Epoller::ModEpollEvent(Channel* event_channel)
{
    if(!event_channel) return false;

    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events |= (event_channel->GetEvents() | EPOLLET);

    if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&event) < 0)
    {
        printf("epoll mod error: %s\n", strerror(errno));
        return false;
    }
    return true;
}

bool Epoller::DelEpollEvent(Channel* event_channel)
{
    if(!event_channel) return false;

    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = 0;

    if(epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,&event)<0)
    {
        printf("epoll del error: %s\n", strerror(errno));
        return false;
    }
    /*释放资源以关闭连接*/
    events_channel_pool_[fd].reset(nullptr);
    http_data_pool_[fd].reset(nullptr);

    return true;
}

std::vector<Channel*> Epoller::GetActiveEvents()
{
    while(!stop_)
    {
        int active_event_num = epoll_wait(epollfd_,&active_events_[0],kMaxActiveEventNum,kEpollTimeOut);
        if(active_event_num < 0)
        {
            if(errno != EINTR) printf("eopll wait error: %s\n", strerror(errno));
            return {};
        }
        else if(active_event_num == 0)
        {
            /*epoll_wait超时，这里的处理方式是继续循环*/
            continue;
        }

        /*根据就绪事件，找到事件池中相应的Channel并修改其revents_属性*/
        std::vector<Channel*> ret(active_event_num);
        for (int i = 0; i < active_event_num; ++i)
        {
            int fd = active_events_[i].data.fd;
            auto& channel = events_channel_pool_[fd];

            if(channel)
            {
                channel->SetRevents(active_events_[i].events);
                ret[i] = channel.get();
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
        if(i) DelEpollEvent(i.get());
    }
}