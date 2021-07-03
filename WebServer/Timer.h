#ifndef WEBSERVER_TIMER_H
#define WEBSERVER_TIMER_H

/*Linux system APIs*/

/*STD Headers*/
#include <memory>
#include <chrono>
#include <queue>
#include <set>
#include <algorithm>
/*User-define Headers*/
#include "HttpData.h"

/*！
@Author: DJJ
@Description: 定时器类

 每个SubReactor都有一个定时器，用来处理超时请求以及长期不活跃的连接。
 也就是说，每一个连接都需要和一个定时器挂靠。

@Date: 2021/7/3 下午2:43
*/

using SysTimePoint = std::chrono::system_clock::time_point;

class Timer {
private:
    std::weak_ptr<HttpData> wp_httpdata_;                       //与定时器挂靠的http连接
    SysTimePoint expired_timepoint_;                            //一个绝对时间，超过该时刻即认为超时
public:
    Timer(std::weak_ptr<HttpData> wp_httpdata,std::chrono::seconds timeout);
    ~Timer();

    SysTimePoint GetExpiredTimePoint() const {return expired_timepoint_;}
    /*当前时刻超过了expired_timepoint_则认为超时*/
    bool Expired() {return std::chrono::system_clock::now() > expired_timepoint_;};
};

bool operator < (const Timer& lhs,const Timer& rhs)
{
    return lhs.GetExpiredTimePoint() < rhs.GetExpiredTimePoint();
}

/*！
@Author: DJJ
@Description: Timer管理类

 用于管理所有的Timer对象

@Date: 2021/7/3 下午3:05
*/
class TimerManager{
private:
    std::set<Timer,std::less<>> timer_set_;      //Timer对象集合，按照超时时刻的远近排序且Timer的生命周期完全由其决定
public:
    void HandleExpired();       //处理所有超时连接
    void AddTimer();            //添加新的定时器
};
#endif //WEBSERVER_TIMER_H
