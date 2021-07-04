#ifndef WEBSERVER_EVENTLOOP_H
#define WEBSERVER_EVENTLOOP_H

/*Linux system APIS*/

/*STD Headers*/

/*User-defien Headers*/
#include "Epoller.h"

/*！
@Author: DJJ
@Description: EventLoop类

 EventLoop即SubReactor或MainReactor。每个线程只能有一个EventLoop对象

@Date: 2021/6/14 下午4:19
*/

class EventLoop {
private:
    std::mutex mutex_for_wakeup_;                 //互斥锁
    std::condition_variable cond_;                //条件变量

    bool stop_ = false;                           //指示Sub/Main-Reactor是否工作，默认为正在工作
    std::unique_ptr<Epoller> p_event_pool_;       //每个EventLoop都唯一拥有一个事件池

    std::mutex mutex_for_conn_num_;
    int connection_num_;                          //Sub/Main-Reactor管理的连接数量
public:
    EventLoop();
    ~EventLoop();

    /*返回Sub/Main-Reactor管理的连接数量*/
    int GetConnectionNum()
    {
        std::unique_lock<std::mutex> locker(mutex_for_conn_num_);
        return connection_num_;
    }

    /*循环调用GetActiveEvent以获取就绪事件。随后调用这些事件的回调函数*/
    void StartLoop();

    /*SubReactor停止工作*/
    void Quit()
    {
        /*事件池停止监听工作并清空事件池*/
        p_event_pool_->Stop();
        p_event_pool_->ClearEpoller();
        /*停止标志设置为true并唤醒休眠中的线程以退出while循环*/
        stop_ = true;
        cond_.notify_all();
    };

    /*增　改　删*/
    bool AddToEventChannelPool(Channel* event_channel)
    {
        /*添加了事件成功后就需唤醒此SubReactor*/
        if(p_event_pool_->AddEpollEvent(event_channel))
        {
            {
                std::unique_lock<std::mutex> locker(mutex_for_conn_num_);
                ++connection_num_;
            }
            cond_.notify_one();
            return true;
        }
        return false;
    }
    bool ModEventChannelPool(Channel* event_channel)
    {
         return p_event_pool_->ModEpollEvent(event_channel);
    }
    bool DelFromEventChannePool(Channel* event_channel)
    {
        if(p_event_pool_->DelEpollEvent(event_channel))
        {
            std::unique_lock<std::mutex> locker(mutex_for_conn_num_);
            --connection_num_;
            return true;
        }
        return  false;
    }
};


#endif //WEBSERVER_EVENTLOOP_H
