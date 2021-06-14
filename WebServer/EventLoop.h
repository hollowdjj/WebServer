#ifndef WEBSERVER_EVENTLOOP_H
#define WEBSERVER_EVENTLOOP_H

/*Linux system APIS*/

/*STD Headers*/

/*User-defien Headers*/
#include "Epoller.h"

/*！
@Author: DJJ
@Description: EventLoop类

 在我的理解中一个EventLoop对象就是一个SubReactor

@Date: 2021/6/14 下午4:19
*/

class EventLoop {
private:
    std::shared_ptr<Epoller> event_pool_;
public:
    EventLoop();
    ~EventLoop();

    /*向事件池中添加事件*/
    void AddToEventChannelPool(std::shared_ptr<Channel> event_channel) {event_pool_->AddEpollEvent(event_channel);}
};


#endif //WEBSERVER_EVENTLOOP_H
