#ifndef WEBSERVER_TIMER_H
#define WEBSERVER_TIMER_H

/*Linux system APIs*/
#include <unistd.h>
#include <signal.h>

/*STD Headers*/
#include <memory>
#include <chrono>
#include <functional>
#include <array>
#include <list>
#include <optional>
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
class Channel;
class TimeWheel;

class Timer {
private:
    using CallBack = std::function<void()>;
    CallBack expired_sHandler_;                            //超时回调函数
    size_t trigger_cycles_;                                //记录定时器在时间轮转多少圈后生效(不是转一次，而是一圈)
    size_t slot_index_;                                    //记录定时器属于时间轮上的哪个槽
public:
    friend class TimeWheel;
    Timer(size_t cycles, size_t slot_index);

    size_t GetSlotIndex() const {return slot_index_;};           //返回Timer对象所在槽在时间轮中的编号
    size_t GetTriggerCycles() const {return trigger_cycles_;}    //返回定时器的触发圈数
    void ReduceTriggerCyclesByOne() {--trigger_cycles_;}         //定时器的触发圈减1
    void SetExpiredHandler(CallBack expired_handler) {expired_sHandler_ = std::move(expired_handler);};   //注册超时回调函数
    void CallExpiredHandler()
    {
        if(expired_sHandler_) expired_sHandler_();
        else printf("expired handler has not been registered yet\n");
    }
private:
    void SetTriggerCycles(size_t trigger_cycles) { trigger_cycles_ = trigger_cycles;};   //修改定时器的触发圈数
    void SetSlotIndex(size_t slot_index) {slot_index_ = slot_index;}                     //修改定时器所在的槽
};

/*
@Author: DJJ
@Description: 采用时间轮管理所有的timer
@Date: 2021/7/3 下午3:05
*/
class TimeWheel{
private:
    std::vector<std::list<Timer*>> slots_;                                  //时间轮的槽(使用裸指针管理Timer的生命周期)
    size_t current_slot_ = 0;                                               //时间轮的当前槽

    Channel* p_tickfd_channel;                                              //tick_fd_[0]对应的Channel
public:
    /*-------------------------拷贝控制成员--------------------------*/
    TimeWheel();
    ~TimeWheel();
    TimeWheel(const TimeWheel& rhs) = delete;
    TimeWheel& operator=(const TimeWheel& rhs) = delete;
    /*----------------------------接口------------------------------*/
    Timer* AddTimer(std::chrono::seconds timeout);                          //添加新的定时器
    void DelTimer(Timer* timer);                                            //删除目标定时器
    void AdjustTimer(Timer* timer, std::chrono::seconds timeout);           //目标timer延迟一个timeout时间
    [[nodiscard]]Channel* GetTickfdChannel() {return p_tickfd_channel;}     //返回tick_fd_[0]对应的channel，其生命周期由reactor管理。
public:
    int tick_fd_[2]{};                                                      //用于通知时间轮tick一下的管道
private:
    void Tick();                                                            //slot_interval_时间到后，时间轮转动一次
    std::optional<std::pair<size_t, size_t>> CalPosInWheel(std::chrono::seconds timeout); //计算timeout时间后触发的定时器因放置在时间轮的什么位置
};


#endif //WEBSERVER_TIMER_H
