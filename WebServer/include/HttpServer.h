/*！
@Author: DJJ
@Date: 2021/6/12 下午4:29
*/
#ifndef WEBSERVER_HTTPSERVER_H
#define WEBSERVER_HTTPSERVER_H

/*Linux system APIS*/
#include <netinet/in.h>

/*STD Headers*/
#include <memory>
#include <unordered_map>
#include <algorithm>

/*User-define Headers*/
#include "NonCopyable.h"

/*前向声明*/
class EventLoop;
class ThreadPool;
class Channel;

/*!
@brief server类
*/
class HttpServer : private NonCopyable{
private:
    int listenfd_;                                            //监听socket
    int port_;                                                //端口号
    Channel* p_listen_channel_;                               //监听socket的Channel
    EventLoop* p_main_reactor_;                               //MainReactor
    std::vector<std::shared_ptr<EventLoop>> sub_reactors_;    //SubReactors
    ThreadPool* p_sub_thread_pool_;                           //管理子线程的线程池
public:
    std::vector<int> tickfds_{};                              //所有SubReactor的tickfd的写端文件描述符
public:
    /*!
    @brief 限制对象数量并禁止复制和赋值
    */
    friend HttpServer* CreateHttpServer(int port, EventLoop* main_reactor, ThreadPool* sub_thread_pool);
    ~HttpServer();

    /*!
    @brief 启动服务器
    */
    void Start();
    
    /*!
    @brief 关闭服务器
    */
    void Quit();
private:
    /*!
    @brief 私有构造函数以限制对象的数量
    */
    HttpServer(int port, EventLoop* main_reactor,ThreadPool* sub_thread_pool);

    /*!
    @brief 监听socket的EPOLLIN回调函数
    */
    void NewConnHandler();

    /*!
    @brief 监听socket的EPOLLERR回调函数
    */
    void ErrorHandler();
};

inline
HttpServer* CreateHttpServer(int port, EventLoop* main_reactor,ThreadPool* sub_thread_pool)
{
    static auto server = new HttpServer(port,main_reactor,sub_thread_pool);
    return server;
}

#endif //WEBSERVER_HTTPSERVER_H
