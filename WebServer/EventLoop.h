#ifndef WEBSERVER_EVENTLOOP_H
#define WEBSERVER_EVENTLOOP_H

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
@Description: EventLoop类

 事件池，唯一属于一个EventLoop对象。MainReactor产生的connfd会被分发给EventLoop表示的SubReactor中的事件池Epoller
 事件池负责调用epoll_wait函数并返回就绪事件，并且所有连接socket、监听socket以及HttpData对象的生命周期都由Epoller
 唯一管理。

@Date: 2021/6/13 下午10:21
*/
class EventLoop {
private:
    std::mutex mutex_for_wakeup_;                                 //用于休眠线程的互斥锁
    std::condition_variable cond_;                                //条件变量
    std::mutex mutex_for_conn_num_;                               //避免访问连接数量造成竞争
    int connection_num_;                                          //Sub/Main-Reactor管理的连接数量

    bool stop_ = false;                                           //指示Sub/Main-Reactor是否工作，默认为正在工作
    int epollfd_;                                                 //epoll内核事件表
    std::vector<epoll_event> active_events_;                      //就绪事件池
    std::unique_ptr<Channel> events_channel_pool_[kMaxUserNum];   //事件池
    std::unique_ptr<HttpData> http_data_pool_[kMaxUserNum];       //每一个连接socket的Channel都会对应一个HttpData对象

    TimeWheel timewheel_;                                         //每个事件池都有一个TimerManager对象
public:
    EventLoop();
    ~EventLoop();

    bool AddEpollEvent(Channel* event_channel);                    //向事件池以及内核事件表中添加新的事件
    bool ModEpollEvent(Channel* event_channel);                    //修改内核事件表中注册的文件描述符想要监听的事件
    bool DelEpollEvent(Channel* event_channel);                    //删除内核事件表中注册的文件描述符
    void StartLoop();                                              //开始监听事件
    void QuitLoop();                                               //清空事件池停止运行

    int GetConnectionNum();                                        //返回连接数量
    void GetActiveEventsAndProc();                                 //返回就绪事件
    int GetEpollfd(){return epollfd_;}                             //返回epoll内核事件表
};


#endif //WEBSERVER_EVENTLOOP_H
