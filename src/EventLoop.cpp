/*User-define Headers*/
#include "EventLoop.h"
#include "Channel.h"
#include "HttpData.h"

EventLoop::EventLoop(bool is_main_reactor /*false*/)
                    : epollfd_(epoll_create1(EPOLL_CLOEXEC)),
                      is_main_reactor_(is_main_reactor)
{
    /*!
        注意，这里没有使用epoll_create。这是因为，epoll_create函数的size参数只是一个参考
        实际上，内核epoll事件表是会动态增长的，因此没有必要使用epoll_create了。并且只有Sub
        Reactor才需要监听管道的读端
     */
    if(!AddEpollEvent(timewheel_.GetTickfdChannel()) || epollfd_ == -1)
    {
        ::GetLogger()->critical("epoll_create1 error: {}", strerror(errno));
        exit(-1);
    }
}

EventLoop::~EventLoop()
{
    close(epollfd_);
}

bool EventLoop::AddEpollEvent(Channel* event_channel, std::chrono::seconds timeout)
{
    /*每一个连接socket都必须设置一个表示HttpData对象的holder*/
    if(!event_channel || (event_channel->IsConnfd() && event_channel->GetHolder()== nullptr)) return false;

    /*向epoll内核事件表添加事件*/
    int fd = event_channel->GetFd();
    epoll_event event;
    bzero(&event,sizeof event);
    event.data.fd = fd;
    event.events = (event_channel->GetEvents() | EPOLLET);
    if(epoll_ctl(epollfd_,EPOLL_CTL_ADD,fd,&event) < 0)
    {
        ::GetLogger()->error("epoll add error: {}", strerror(errno));
        return false;
    }
    
    /*添加事件成功后才能将事件添加到事件池中。只有连接socket才需设置holder和timer*/
    events_channel_pool_[event.data.fd] = std::unique_ptr<Channel>(event_channel);
    if(event_channel->GetHolder())
    {
        /*有holder的连接socket才需要将holder保存在事件池中并设置timer。监听socket以及tickfd均不用设置*/
        http_data_pool_[event.data.fd] = std::unique_ptr<HttpData>(event_channel->GetHolder());
        auto p_timer = timewheel_.AddTimer(timeout);              //设置timer
        event_channel->GetHolder()->LinkTimer(p_timer);
        /*连接数加1*/
        {
            std::unique_lock<std::mutex> locker(mutex_for_conn_num_);
            ++connection_num_;
        }
    }

    return true;
}

bool EventLoop::ModEpollEvent(Channel* event_channel)
{
    if(!event_channel) return false;

    /*修改注册的事件*/
    int fd = event_channel->GetFd();
    if(!event_channel->EqualAndUpdateLastEvents())
    {
        epoll_event event{};
        event.data.fd = fd;
        event.events = (event_channel->GetEvents() | EPOLLET);
        if(epoll_ctl(epollfd_,EPOLL_CTL_MOD,fd,&event) < 0)
        {
            ::GetLogger()->error("epoll mod error: {}", strerror(errno));
            return false;
        }
    }

    return true;
}

bool EventLoop::DelEpollEvent(Channel* event_channel)
{
    if(!event_channel) return false;

    /*从epoll内核事件表中删除事件*/
    int fd = event_channel->GetFd();
    epoll_event event;
    event.data.fd = fd;
    event.events = 0;
    if(epoll_ctl(epollfd_,EPOLL_CTL_DEL,fd,&event)<0)
    {
        ::GetLogger()->error("epoll del error: {}", strerror(errno));
        return false;
    }

    /*仅连接socket需要删除定时器以及holder*/
    if(event_channel->GetHolder())
    {
        timewheel_.DelTimer(event_channel->GetHolder()->GetTimer());
        http_data_pool_[fd].reset(nullptr);
        /*连接数减一*/
        {
            std::unique_lock<std::mutex> locker(mutex_for_conn_num_);
            --connection_num_;
        }
    }
    events_channel_pool_[fd].reset(nullptr);

    return true;
}

void EventLoop::StartLoop()
{
    /*监听*/
    while(!stop_)
    {
        /*获取事件池中的就绪事件并调用相应的回调函数*/
        GetActiveEventsAndProc();
    }
    stop_ = true;
}

void EventLoop::QuitLoop()
{
    stop_ = true;
    for (auto& i : events_channel_pool_)
    {
        if(i) DelEpollEvent(i.get());    //断开所有连接
    }
}

int EventLoop::GetConnectionNum()
{
    std::unique_lock<std::mutex> locker(mutex_for_conn_num_);
    return connection_num_;
}

void EventLoop::GetActiveEventsAndProc()
{
    while(!stop_)
    {
        int active_event_num = epoll_wait(epollfd_,active_events_,kMaxActiveEventNum,kEpollTimeOut);
        if(active_event_num < 0 && errno != EINTR)
        {
            /*这里不对系统中断信号作出处理，程序照常运行*/
            ::GetLogger()->error("eopll wait error: {}", strerror(errno));
            return;
        }
        else if(active_event_num == 0)
        {
            /*epoll_wait超时，这里的处理方式是继续循环*/
            continue;
        }
        /*根据就绪事件，找到事件池中相应的Channel修改其revents_属性后再调用相应的回调函数*/
        for (int i = 0; i < active_event_num; ++i)
        {
            int fd = active_events_[i].data.fd;
            auto& channel = events_channel_pool_[fd];

            if(channel)
            {
                channel->SetRevents(active_events_[i].events);
                channel->CallReventsHandlers();
            }
            else
            {
                ::GetLogger()->warn("Set revents on emtpy Channel Object");
            }
        }
    }
}