#include "EventLoop.h"

EventLoop::EventLoop() : event_pool_(std::make_shared<Epoller>())
{

}
EventLoop::~EventLoop()
{
    stop_ = true;
}

void EventLoop::StartLoop()
{
    std::vector<std::shared_ptr<Channel>> ret;
    while(!stop_)
    {
        ret.clear();
        /*事件池为空时休眠*/
        {
            std::unique_lock<std::mutex> locker(mutex_);
            cond_.wait(locker,[this](){return !this->event_pool_->empty();});
        }
        /*获取事件池中的就绪事件*/
        ret = event_pool_->GetActiveEvents();
        /*调用活跃事件的回调函数*/
        for (auto& item : ret) item->CallReventsHandlers();
    }
    stop_ = true;
}