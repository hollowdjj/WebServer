#include "Timer.h"
#include "Channel.h"
#include "Utility.h"

/*---------------------------------Timer类--------------------------------------*/
Timer::Timer(size_t trigger_cycles, size_t slot_index)
            : trigger_cycles_(trigger_cycles),
              slot_index_(slot_index) {}


/*------------------------------TimerManager类----------------------------------*/
TimeWheel::TimeWheel()
{
    slots_.resize(GlobalVar::slot_num);
    /*创建一个管道，通过监听tick_fd_[0]可读事件的形式定时让时间轮tick一下*/
    int res = pipe(tick_fd_);
    assert(res != -1);
    SetNonBlocking(tick_fd_[0]);
    SetNonBlocking(tick_fd_[1]);
    p_tickfd_channel = new Channel(tick_fd_[0],false,false);
    p_tickfd_channel->SetEvents(EPOLLIN);
    p_tickfd_channel->SetReadHandler([this](){Tick();});
}

TimeWheel::~TimeWheel()
{
    /*delete所有timer*/
    for (auto& slot : slots_)
    {
        for (auto& timer : slot)
        {
            delete timer;
        }
    }
    /*关闭管道*/
    close(tick_fd_[0]);
    close(tick_fd_[1]);
}

Timer* TimeWheel::AddTimer(std::chrono::seconds timeout)
{
    if(timeout<std::chrono::seconds(0))
    {
        printf("timeout can't be less than 0\n");
        return nullptr;
    }
    /*!
        根据超时值计算该计时器将在时间轮转动多少次（即多少个tick）之后被触发。如果timeout小于slot_interval_，tick折合为１
        否则向下折合为timeout/slot_interval_。然后，计算计时器的触发圈数以及应放入哪一个槽中。
     */
     size_t ticks = 0;
     if(timeout < GlobalVar::slot_interval) ticks = 1;
     else ticks = timeout / GlobalVar::slot_interval;
     size_t cycle = ticks / GlobalVar::slot_num;                                           //timer将在时间轮转动cycle圈后被触发
     size_t index = (current_slot_ + (ticks % GlobalVar::slot_num)) % GlobalVar::slot_num; //timer被放置的槽的序号
    /*创建一个Timer并加入到时间轮的对应槽之中*/
     Timer* p_timer = new Timer(cycle,index);
     slots_[index].push_back(p_timer);

     return p_timer;
}

void TimeWheel::DelTimer(Timer* timer)
{
    size_t index = timer->GetSlotIndex();
    slots_[index].remove(timer);
}

void TimeWheel::Tick()
{
    for (auto& timer : slots_[current_slot_])
    {
        /*trigger_cycles_大于0说明还未到触发时间*/
        if(timer->GetTriggerCycles() > 0)
        {
            timer->ReduceTriggerCyclesByOne();      //减一，表示经过了这个槽。
        }
        /*定时器到期，调用回调函数并删除定时器*/
        else
        {
            timer->CallExpiredHandler();
            slots_[current_slot_].remove(timer);
        }
        current_slot_ = ++current_slot_ % GlobalVar::slot_num;
    }
}