#include "Timer.h"
#include "HttpData.h"
#include "Utility.h"

/*---------------------------------Timer类--------------------------------------*/
Timer::Timer(size_t trigger_cycles, size_t slot_index)
            : trigger_cycles_(trigger_cycles),
              slot_index_(slot_index) {}


/*------------------------------TimerManager类----------------------------------*/
TimeWheel::TimeWheel()
{
    /*创建一个管道，通过监听tick_fd_[0]可读事件的形式定时让时间轮tick一下*/
    int res = pipe(tick_fd_);
    assert(res != -1);
    SetNonBlocking(tick_fd_[0]);
    SetNonBlocking(tick_fd_[1]);
}

TimeWheel::~TimeWheel()
{
    /*delete所有timer*/
    for (auto& slot : slots)
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
     if(timeout < slot_interval_) ticks = 1;
     else ticks = timeout / slot_interval_;
     size_t cycle = ticks / slot_num_;                                  //timer将在时间轮转动cycle圈后被触发
     size_t index = (current_slot_ + (ticks % slot_num_)) % slot_num_;  //timer被放置的槽的序号
    /*创建一个Timer并加入到时间轮的对应槽之中*/
     Timer* p_timer = new Timer(cycle,index);
     slots[index].push_back(p_timer);

     return p_timer;
}

void TimeWheel::DelTimer(Timer* timer)
{
    size_t index = timer->GetSlotIndex();
    slots[index].remove(timer);
}

void TimeWheel::TickAndAlarm()
{
    Tick();
    alarm(std::chrono::duration_cast<std::chrono::seconds>(slot_interval_).count());
}

void TimeWheel::Tick()
{
    for (auto& timer : slots[current_slot_])
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
            slots[current_slot_].remove(timer);
        }
        current_slot_ = ++current_slot_ % slot_num_;
    }
}

void SigHandler(int sig)
{
    /*向tick_fd[1]写一个字节的数据*/
    int msg = sig;
    auto tick_fd = *CreatePipe();
    send(tick_fd[1],reinterpret_cast<char*>(&msg),1,0);
}

//auto CreatePipe() -> int(*)[2]
//{
//    /*每个线程只会有一个tick_fd*/
//    static thread_local int tick_fd[2];
//    return &tick_fd;
//}

