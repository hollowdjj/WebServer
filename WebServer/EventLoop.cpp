#include "EventLoop.h"
#include "Channel.h"

EventLoop::EventLoop() : event_pool_(std::make_unique<Epoller>())
{

}

EventLoop::~EventLoop()
{
    stop_ = true;
}

void EventLoop::StartLoop()
{
    std::vector<Channel*> ret;
    while(!stop_)
    {
        ret.clear();
        /*事件池为空时休眠*/
        {
            std::unique_lock<std::mutex> locker(mutex_for_wakeup_);
            cond_.wait(locker,[this](){return connection_num_ > 0;});
        }
        /*获取事件池中的就绪事件*/
        ret = event_pool_->GetActiveEvents();
        /*调用活跃事件的回调函数*/
        for (auto& item : ret) item->CallReventsHandlers();
    }
    stop_ = true;
}