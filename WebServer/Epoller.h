#ifndef WEBSERVER_EPOLLER_H
#define WEBSERVER_EPOLLER_H

/*Linux system APIS*/


/*STL Headers*/
#include <memory>
#include <vector>
#include <array>

/*User-define Headers*/
#include "Channel.h"
#include "Utility.h"

class HttpData;        //这里只是用智能指针来管理其对象的生存周期，不涉及对象的任何操作，故不需要引用头文件。

/*！
@Author: DJJ
@Description: Epoller类

 事件池,属于一个EventLoop对象。MainReactor产生的connfd会被分发给EventLoop对象中的事件池Epoller
 事件池负责调用epoll_wait函数并返回就绪事件。

@Date: 2021/6/13 下午10:21
*/
class Epoller {
private:
    int epollfd_;                                                 //epoll内核事件表
    int current_channel_num_ = 0;                                 //事件池中的事件数量
    std::vector<epoll_event> active_events_;                      //就绪事件
    std::shared_ptr<Channel> events_channel_pool_[kMaxUserNum];   //事件池
    std::shared_ptr<HttpData> http_data_pool_[kMaxUserNum];       //每一个连接socket的Channel都会对应一个HttpData对象
    bool stop_ = false;                                           //停止监听的标志
public:
    Epoller();

    bool AddEpollEvent(std::shared_ptr<Channel> event_channel);   //向事件池以及内核事件表中添加新的事件
    bool ModEpollEvent(std::shared_ptr<Channel> event_channel);   //修改内核事件表中注册的文件描述符想要监听的事件
    bool DelEpollEvent(std::shared_ptr<Channel> event_channel);   //删除内核事件表中注册的文件描述符

    std::vector<std::shared_ptr<Channel>> GetActiveEvents();      //返回就绪事件
    int GetEpollfd(){return epollfd_;}                            //返回epoll内核事件表
    bool empty() {return current_channel_num_ == 0;}              //判断事件池是否为空
    void ClearEpoller();                                          //断开所有连接
    void Stop() {stop_ = true;};                                  //停止监听
};


#endif //WEBSERVER_EPOLLER_H
