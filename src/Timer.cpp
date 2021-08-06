#include "Timer.h"
#include "Channel.h"
#include "Utility.h"

/*---------------------------------Timer类--------------------------------------*/
Timer::Timer(size_t trigger_cycles, size_t slot_index)
            : trigger_cycles_(trigger_cycles),
              slot_index_(slot_index) {}


void Timer::CallExpiredHandler()
{
    if(expired_sHandler_) expired_sHandler_();
    else ::GetLogger()->warn("expired handler has not been registered yet");
}

/*------------------------------TimerManager类----------------------------------*/
TimeWheel::TimeWheel()
{
    slots_.resize(GlobalVar::slot_num_);
    /*创建一个管道，通过监听tick_fd_[0]可读事件的形式定时让时间轮tick一下*/
    int res = pipe(tick_fd_);
    assert(res != -1);
    SetNonBlocking(tick_fd_[0]);
    SetNonBlocking(tick_fd_[1]);
    p_tickfd_channel_ = new Channel(tick_fd_[0], false);
    p_tickfd_channel_->SetEvents(EPOLLIN);
    p_tickfd_channel_->SetReadHandler([this](){Tick();});
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
     /*创建一个Timer并加入到时间轮的对应槽之中*/
     auto pos = CalPosInWheel(timeout);
     if(!pos) return nullptr;
     Timer* p_timer = new Timer(pos->first,pos->second);
     slots_[pos->second].push_back(p_timer);

     return p_timer;
}

void TimeWheel::DelTimer(Timer* timer)
{
    if(!timer) return;
    /*从时间轮中删除目标Timer并delete*/
    size_t index = timer->GetSlotIndex();
    slots_[index].remove(timer);
    delete timer;
}

void TimeWheel::AdjustTimer(Timer* timer, std::chrono::seconds timeout)
{
    auto pos = CalPosInWheel(timeout);
    if(!timer || !pos) return;

    /*先从时间轮中移除timer，但不要delete*/
    size_t index = timer->GetSlotIndex();
    slots_[index].remove(timer);
    /*修改timer的触发圈数以及所在的槽，并加入到时间轮中相应的槽*/
    timer->SetTriggerCycles(pos->first);
    timer->SetSlotIndex(pos->second);
    slots_[pos->second].push_back(timer);
}

void TimeWheel::Tick()
{
    /*从管道读出数据以防写满*/
    char msg[128];
    ReadData(tick_fd_[0],msg,128);

    /*值得注意的是，这里是在遍历容器的时候删除特定元素。*/
    for (auto it = slots_[current_slot_].begin();it != slots_[current_slot_].end();)
    {
        /*删除空指针以防万一*/
        Timer* p_timer = *it;
        if(!p_timer)
        {
            it = slots_[current_slot_].erase(it);
            continue;
        }
        
        /*trigger_cycles_大于0说明还未到触发时间*/
        if(p_timer->GetTriggerCycles() > 0)
        {
            p_timer->ReduceTriggerCyclesByOne();             //减一，表示经过了这个槽。
            ++it;
        }
        /*小于0则说明到触发时间了*/
        else
        {
            auto next = ++it;
            p_timer->CallExpiredHandler();                  //关闭连接 删除时间 删除定时器
            it = next;
        }
    }
    current_slot_ = ++current_slot_ % GlobalVar::slot_num_;  //指向下一个槽
}

std::optional<std::pair<size_t /*cycle*/, size_t /*index*/>> TimeWheel::CalPosInWheel(std::chrono::seconds timeout)
{
    if(timeout<std::chrono::seconds(0))
    {
        ::GetLogger()->warn("timeout can't be less than 0");
        return std::nullopt;
    }
    /*!
        根据超时值计算该计时器将在时间轮转动多少次（即多少个tick）之后被触发。如果timeout小于slot_interval_，tick折合为１
        否则向下折合为timeout/slot_interval_。然后，计算计时器的触发圈数以及应放入哪一个槽中。
     */
    size_t ticks = 0;
    if(timeout < GlobalVar::slot_interval_) ticks = 1;
    else ticks = timeout / GlobalVar::slot_interval_;
    size_t cycle = ticks / GlobalVar::slot_num_;                                           //timer将在时间轮转动cycle圈后被触发
    size_t index = (current_slot_ + (ticks % GlobalVar::slot_num_)) % GlobalVar::slot_num_; //timer被放置的槽的序号
    return std::make_pair(cycle,index);
}