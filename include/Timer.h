/*！
@Author: DJJ
@Date: 2021/7/3 下午2:43
*/
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

/*前向声明*/
class HttpData;
class Channel;
class TimeWheel;

/*!
@brief 定时器
*/
class Timer {
private:
    using CallBack = std::function<void()>;
    CallBack expired_sHandler_;                //超时回调函数
    size_t trigger_cycles_;                    //记录定时器在时间轮转多少圈后生效(不是转一次，而是一圈)
    size_t slot_index_;                        //记录定时器属于时间轮上的哪个槽
public:
    friend class TimeWheel;
    Timer(size_t cycles, size_t slot_index);

    /*!
    @brief 返回定时器所在槽在时间轮中的编号。
    */
    size_t GetSlotIndex() const {return slot_index_;};

    /*!
    @brief 返回定时器的触发圈数。
    */
    size_t GetTriggerCycles() const {return trigger_cycles_;}

    /*!
    @brief 定时器的触发圈减1。
    */
    void ReduceTriggerCyclesByOne() {--trigger_cycles_;}

    /*!
    @brief 注册超时回调函数。
    */
    void SetExpiredHandler(CallBack expired_handler) {expired_sHandler_ = std::move(expired_handler);};
private:
    /*!
    @brief 调用超时回调函数。
    */
    void CallExpiredHandler();

    /*!
    @brief 修改定时器的触发圈数。
    */
    void SetTriggerCycles(size_t trigger_cycles) { trigger_cycles_ = trigger_cycles;};

    /*!
    @brief 修改定时器在时间轮中的槽。
    */
    void SetSlotIndex(size_t slot_index) {slot_index_ = slot_index;}
};

/*!
@brief 时间轮
*/
class TimeWheel{
private:
    std::vector<std::list<Timer*>> slots_;       //时间轮的槽(使用裸指针管理Timer的生命周期)
    size_t current_slot_ = 0;                    //时间轮的当前槽

    Channel* p_tickfd_channel_;                  //tick_fd_[0]对应的Channel
public:
    TimeWheel();
    ~TimeWheel();
    TimeWheel(const TimeWheel& rhs) = delete;
    TimeWheel& operator=(const TimeWheel& rhs) = delete;

    /*!
    @brief 向时间轮中添加新的定时器。

    @param[in] timeout 定时器的超时时间。
    @return    添加的定时器对象的指针。注意，不要delete此指针。
    */
    Timer* AddTimer(std::chrono::seconds timeout);

    /*!
    @brief 从时间轮中删除目标定时器并释放资源。
    */
    void DelTimer(Timer* timer);

    /*!
    @brief 修改目标定时器的超时时间。

    此函数是以当前时间为基准重新设置一个值为timeout的超时时间而非叠加超时时间。
    @param[in] timer   目标定时器
    @param[in] timeout 新的超时时间
    */
    void AdjustTimer(Timer* timer, std::chrono::seconds timeout);

    /*!
    @brief 返回tick_fd_[0]对应的channel，其生命周期由时间轮对象所在的SubReactor管理。
    */
    [[nodiscard]]Channel* GetTickfdChannel() {return p_tickfd_channel_;}
public:
    int tick_fd_[2]{};             //用于通知时间轮tick一下的管道
private:
    /*!
    @brief GlobalVar::slot_interval_时间到后，时间轮转动一次
    */
    void Tick();

    /*!
    @brief 计算超时时间为timeout的定时器应放置在时间轮的什么位置

    @param[in] timeout 超时时间
    @return    pair.first为触发圈数，pair.second为定时器所在槽的索引
    */
    std::optional<std::pair<size_t, size_t>> CalPosInWheel(std::chrono::seconds timeout);
};

#endif //WEBSERVER_TIMER_H
