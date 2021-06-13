#ifndef WEBSERVER_EPOLLER_H
#define WEBSERVER_EPOLLER_H

/*！
@Author: DJJ
@Description: Epoller类

 事件池,属于一个EventLoop对象。MainReactor产生的connfd会被分发给EventLoop对象中的事件池Epoller
 事件池负责调用epoll_wait函数并存储就绪事件。

@Date: 2021/6/13 下午10:21
*/
class Epoller {

};


#endif //WEBSERVER_EPOLLER_H
