#include <curses.h>
#include "Timer.h"
#include "HttpData.h"

/*Timer类*/
Timer::Timer(size_t cycles, size_t slot_index)
            : cycles_(cycles),
              slot_index_(slot_index) {}


/*TimerManager类*/

void TimeWheel::HandleExpired()
{
    /*找到timer_set_中第一个超时时刻在当前时刻之后的Timer*/
}

void TimeWheel::AddTimer(std::chrono::seconds timeout)
{
    if(timeout<std::chrono::seconds(0))
    {
        printf("timeout can't be less than 0\n");
        return;
    }
    /*!
        根据超时值计算该计时器将在时间轮转动多少次（即多少个tick）之后被触发。如果timeout小于slot_interval_，tick折合为１
        否则向下折合为timeout/slot_interval_。然后，计算计时器的触发圈数以及应放入哪一个槽中。
     */
     size_t ticks = 0;
     if(timeout < slot_interval_) ticks = 1;
     else ticks = timeout / slot_interval_;
     size_t cycle = ticks / slot_num_;                                  //timer将在时间轮转动cycle圈后被触发
     size_t index = (current_slot_ + (ticks % slot_num_)) % slot_num_;  //timer被放置的槽的序号
    /*创建一个Timer并加入到时间轮的对应槽之中*/
    Timer timer = Timer(cycle,index);
    //slots[index].emplace_back(std::move(timer));
}

//void TimeWheel::DelTimer(Timer* timer)
//{
//    size_t index = timer.GetSlotIndex();
//    slots[index].remove(timer);
//}