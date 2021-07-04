#ifndef WEBSERVER_TIMER_H
#define WEBSERVER_TIMER_H

/*Linux system APIs*/

/*STD Headers*/
#include <memory>
#include <chrono>
#include <set>
#include <algorithm>
#include <functional>
#include <array>
#include <list>
/*User-define Headers*/


/*！
@Author: DJJ
@Description: 定时器类

 每个SubReactor都有一个定时器，用来处理超时请求以及长期不活跃的连接。
 也就是说，每一个连接都需要和一个定时器挂靠。

@Date: 2021/7/3 下午2:43
*/

/*前向声明*/
class HttpData;

class Timer {
private:
    using CallBack = std::function<void()>;
    CallBack ExpiredHandler_;                              //超时回调函数
    size_t cycles_;                                        //记录定时器在时间轮转多少圈后生效(不是转一次，而是一圈)
    size_t slot_index_;                                    //记录定时器属于时间轮上的哪个槽
public:
    /*-------------------------拷贝控制成员-------------------------*/
    Timer(size_t cycles, size_t slot_index);

    /*----------------------------接口-----------------------------*/
    size_t GetSlotIndex() const {return slot_index_;};    //返回Timer对象所在槽在时间轮中的编号
    void SetExpiredHandler(CallBack expired_handler) {ExpiredHandler_ = std::move(expired_handler);};   //注册超时回调函数
private:
    void CallExpiredHandler()
    {
        if(ExpiredHandler_) ExpiredHandler_();
        else printf("expired handler has not been registered yet\n");
    }
};

/*！
@Author: DJJ
@Description: 采用时间轮管理所有的timer
@Date: 2021/7/3 下午3:05
*/
class TimeWheel{
private:
    static const size_t slot_num_ = 60;                                     //时间轮的槽数
    std::chrono::seconds slot_interval_ = std::chrono::seconds(1);     //时间轮的槽间隔(1s)
    std::array<std::list<std::unique_ptr<Timer>>,slot_num_> slots;          //时间轮的槽
    size_t current_slot_ = 0;                                               //时间轮的当前槽
public:
    /*-------------------------拷贝控制成员--------------------------*/
    TimeWheel() = default;
    /*----------------------------接口------------------------------*/
    void AddTimer(std::chrono::seconds timeout);                            //添加新的定时器
    //void DelTimer(const Timer& timer);                                      //删除目标定时器
    void Tick();                                                            //slot_interval_时间到后，时间轮转动一次
    void HandleExpired();                                                   //处理所有超时连接

};
#endif //WEBSERVER_TIMER_H
