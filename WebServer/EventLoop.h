#ifndef WEBSERVER_EVENTLOOP_H
#define WEBSERVER_EVENTLOOP_H

/*Linux system APIS*/

/*STD Headers*/

/*User-defien Headers*/
#include "Epoller.h"

/*！
@Author: DJJ
@Description: EventLoop类

 EventLoop即SubReactor。每个线程只能有一个EventLoop对象

@Date: 2021/6/14 下午4:19
*/

class EventLoop {
private:
    std::mutex mutex_;                         //互斥锁
    std::condition_variable cond_;             //条件变量
    bool stop_ = true;                         //指示SubReactor是否工作，默认为停止工作
    std::shared_ptr<Epoller> event_pool_;      //每个EventLoop都有一个事件池
public:
    EventLoop();
    ~EventLoop();

    /*循环调用GetActiveEvent以获取就绪事件。随后调用这些事件的回调函数*/
    void StartLoop();

    /*增　改　删*/
    bool AddToEventChannelPool(std::shared_ptr<Channel> event_channel)
    {
        /*添加了事件成功后就需唤醒此SubReactor*/
        if(event_pool_->AddEpollEvent(event_channel))
        {
            cond_.notify_one();
            return true;
        }
        else
        {
            return false;
        }
    }
    bool ModEventChannelPool(std::shared_ptr<Channel> event_channel)
    {
         return event_pool_->ModEpollEvent(event_channel);
    }
    bool DelFromEventChannePool(std::shared_ptr<Channel> event_channel)
    {
        return event_pool_->DelEpollEvent(event_channel);
    }
};


#endif //WEBSERVER_EVENTLOOP_H
