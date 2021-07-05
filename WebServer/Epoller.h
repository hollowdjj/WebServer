#ifndef WEBSERVER_EPOLLER_H
#define WEBSERVER_EPOLLER_H

/*Linux system APIS*/
#include <sys/epoll.h>

/*STL Headers*/
#include <memory>
#include <vector>
#include <array>

/*User-define Headers*/
#include "Utility.h"
#include "Timer.h"

class Channel;
class HttpData;

/*！
@Author: DJJ
@Description: Epoller类

 事件池，唯一属于一个EventLoop对象。MainReactor产生的connfd会被分发给EventLoop表示的SubReactor中的事件池Epoller
 事件池负责调用epoll_wait函数并返回就绪事件，并且所有连接socket、监听socket以及HttpData对象的生命周期都由Epoller
 唯一管理。

@Date: 2021/6/13 下午10:21
*/
class Epoller {
private:
    int epollfd_;                                                 //epoll内核事件表
    std::vector<epoll_event> active_events_;                      //就绪事件
    std::unique_ptr<Channel> events_channel_pool_[kMaxUserNum];   //事件池
    std::unique_ptr<HttpData> http_data_pool_[kMaxUserNum];       //每一个连接socket的Channel都会对应一个HttpData对象
    bool stop_ = false;                                           //停止监听的标志
    TimeWheel timewheel_;                                         //每个事件池都有一个TimerManager对象
public:
    Epoller();
    ~Epoller();

    bool AddEpollEvent(Channel* event_channel);                    //向事件池以及内核事件表中添加新的事件
    bool ModEpollEvent(Channel* event_channel);                    //修改内核事件表中注册的文件描述符想要监听的事件
    bool DelEpollEvent(Channel* event_channel);                    //删除内核事件表中注册的文件描述符

    std::vector<Channel*> GetActiveEvents();                       //返回就绪事件
    int GetEpollfd(){return epollfd_;}                             //返回epoll内核事件表
    void ClearEpoller();                                           //断开所有连接
    void Stop() {stop_ = true;};                                   //停止监听
};


#endif //WEBSERVER_EPOLLER_H
