#include "Timer.h"


Timer::Timer(std::weak_ptr<HttpData> wp_httpdata, std::chrono::seconds timeout)
            : wp_httpdata_(wp_httpdata),expired_timepoint_(std::chrono::system_clock::now() + timeout)
{

}

Timer::~Timer()
{
    /*析构时，讲与该Timer对象挂靠的http连接关闭*/
    if(auto  p = wp_httpdata_.lock())
    {
        //p->HandleDisConn();
    }
}

void TimerManager::HandleExpired()
{
    /*找到timer_set_中第一个超时时刻在当前时刻之后的Timer*/
    auto target = std::find_if(timer_set_.begin(),timer_set_.end(),[](const Timer& time)
    {
        return time.GetExpiredTimePoint() > std::chrono::system_clock::now();
    });
    /*删除超时的Timer*/
    timer_set_.erase(timer_set_.begin(),target);
}