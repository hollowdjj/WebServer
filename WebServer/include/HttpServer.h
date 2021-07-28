#ifndef WEBSERVER_HTTPSERVER_H
#define WEBSERVER_HTTPSERVER_H

/*Linux system APIS*/
#include <netinet/in.h>


/*STD Headers*/
#include <memory>
#include <unordered_map>
#include <algorithm>

/*User-define Heades*/
#include "EventLoop.h"

/*！
@Author: DJJ
@Description: HttpServer类

 HttpServer类只能创建一个对象且是在主线程中创建。同时，主线程中创建的EventLoop对象表示的MainReactor用于监听客户端的连接请求

@Date: 2021/6/12 下午4:29
*/
class HttpServer : private NonCopyable{
private:
    int listenfd_;                                                        //监听socket
    int port_;                                                            //端口号
    Channel* p_listen_channel_;                                           //监听socket的Channel
    int current_user_num = 0;                                             //当前用户数量
    EventLoop* p_main_reactor_;                                           //MainReactor
    std::vector<std::shared_ptr<EventLoop>> sub_reactors_;                //SubReactors
    ThreadPool* p_sub_thread_pool_;                                       //管理子线程的线程池
public:
    std::vector<int> tickfds_{};                                          //所有SubReactor的tickfd的写端文件描述符
public:
    /*限制对象数量并禁止复制和赋值*/
    friend HttpServer* CreateHttpServer(int port, EventLoop* main_reactor, ThreadPool* sub_thread_pool);
    ~HttpServer();

    void Start();
    void Quit();
private:
    HttpServer(int port, EventLoop* main_reactor,ThreadPool* sub_thread_pool);    //私有构造函数以限制对象的数量
    void NewConnHandler();                                                        //监听socket可读事件就绪的回调函数
    void ErrorHandler();                                                          //监听socket错误处理的回调函数
};

inline
HttpServer* CreateHttpServer(int port, EventLoop* main_reactor,ThreadPool* sub_thread_pool)
{
    static auto server = new HttpServer(port,main_reactor,sub_thread_pool);
    return server;
}
#endif //WEBSERVER_HTTPSERVER_H
