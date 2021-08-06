/*！
@Author: DJJ
@Date: 2021/6/13 下午10:21
*/
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

/*前向声明*/
class Channel;
class HttpData;

class EventLoop {
private:
    std::mutex mutex_for_conn_num_;                               //避免访问连接数量造成竞争
    int connection_num_{};                                        //Reactor管理的http连接的数量

    bool stop_ = false;                                                     //指示Sub/Main-Reactor是否工作，默认为正在工作
    int epollfd_;                                                           //epoll内核事件表
    static const int kMaxActiveEventNum = 4096;
    static const int kEpollTimeOut = 10000;                                 //epoll超时时间10秒(单位为毫秒)
    epoll_event active_events_[kMaxActiveEventNum];                         //就绪事件池
    std::unique_ptr<Channel> events_channel_pool_[GlobalVar::kMaxUserNum];  //事件池
    std::unique_ptr<HttpData> http_data_pool_[GlobalVar::kMaxUserNum];      //每一个连接socket的Channel都会对应一个HttpData对象

    bool is_main_reactor_;                                        //指示是否为MainReactor
public:
    TimeWheel timewheel_;                                         //为了避免竞争，让每个事件池都拥有一个独立的时间轮
public:
    explicit EventLoop(bool is_main_reactor = false);
    ~EventLoop();

    /*!
    @brief 添加新的监听对象。

    如果添加的监听对象是连接socket，那么该函数会对其holder设置并挂靠一个定时器。
    定时器的超时时间默认为GlobalVar::client_header_timeout_。
    @param[in] event_channel 监听对象。
    @param[in] timeout       超时时间。
    @return    true添加成功，false添加失败。
    */
    bool AddEpollEvent(Channel* event_channel, std::chrono::seconds timeout = GlobalVar::client_header_timeout_);

    /*!
    @brief 修改监听对象所要监听的事件。

    @param[in] event_channel 监听对象。
    @return    true修改成功，false修改失败。
    */
    bool ModEpollEvent(Channel* event_channel);

    /*!
    @brief 删除监听对象。

    @param[in] event_channel 监听对象。
    @return    true删除成功，false删除失败。
    */
    bool DelEpollEvent(Channel* event_channel);

    /*!
    @brief 开始监听。
    */
    void StartLoop();

    /*!
    @brief 清空事件池停止运行。
    */
    void QuitLoop();

    /*!
    @brief 返回连接数量。
    */
    int GetConnectionNum();

    /*!
    @brief 返回epoll内核事件表文件描述符。
    */
    [[maybe_unused]]int GetEpollfd(){return epollfd_;}
private:
    /*!
    @brief 调用epoll_wait并根据事件调用其响应函数
    */
    void GetActiveEventsAndProc();
};

#endif //WEBSERVER_EVENTLOOP_H
