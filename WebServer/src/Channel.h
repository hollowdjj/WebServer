#ifndef WEBSERVER_CHANNEL_H
#define WEBSERVER_CHANNEL_H

/*Linux system APIS*/
#include <sys/epoll.h>
#include <unistd.h>
#include <assert.h>

/*STD Headers*/
#include <functional>
#include <iostream>
#include <memory>

/*User-define Headers*/

/*！
@Author: DJJ
@Description: Channel类，即一个底层事件类

 1. 注册文件描述符、其上要监听的事件以及相应的回调函数
 2. 根据就绪事件调用相应的回调函数

 需要注意的是，客户端断开连接时，会同时触发可读事件。因此，最好在读数据的回调函数中根据recv函数返回0来判断
 客户端是否关闭。

@Date: 2021/6/12 下午5:34
*/

class HttpData;

class Channel {
private:
    int fd_;                       //需要监听事件的文件描述符
    bool is_listenfd_;             //表示fd_是否为监听socket
    bool is_connfd_;               //表示fd_是否为连接socket
    __uint32_t events_{};          //文件描述符fd_需要监听的事件
    __uint32_t revents_{};         //events_中的就绪事件
    __uint32_t last_events_{};     //修改之前注册的事件

    using CallBack = std::function<void()>;
    CallBack ReadHandler_;         //EPOLLIN的回调函数
    CallBack WriteHandler_;        //EPOLLOUT的回调函数
    CallBack ErrorHandler_;        //EPOLLERR的回调函数
    CallBack ConnHandler_;         //接受连接的回调函数
    CallBack DisConnHandler_;      //EPOLLRDHUP的回调函数

    HttpData* p_holder_{};           //只有连接socket才需要一个holder，监听socket不需要
public:
    Channel(int fd, bool is_listenfd, bool is_connfd);
    ~Channel();

    /*getters and setters*/
    void SetFd(int fd)                   {fd_ = fd;}
    int  GetFd()                         {return fd_;}

    void SetEvents(__uint32_t events)    {events_ = events;}
    __uint32_t  GetEvents()              {return events_;}

    void SetRevents(__uint32_t revents)  {revents_ = revents;}
    __uint32_t GetRevents()              {return revents_;}

    void SetIsListenfd(bool is_listenfd) {is_listenfd_ = is_listenfd;}
    bool IsListenfd()                    {return is_listenfd_;}

    void SetIsConnfd(bool is_connfd)     {is_connfd_ = is_connfd;}
    bool IsConnfd()                      {return is_connfd_;}

    void SetHolder(HttpData* holder) {p_holder_ = holder;}
    HttpData* GetHolder() {return p_holder_;}

    /*注册回调函数。std::function是cheap object，pass by value + move就可*/
    void SetReadHandler(CallBack read_handler)       {ReadHandler_ = std::move(read_handler);}
    void SetWriteHandler(CallBack write_handler)     {WriteHandler_ = std::move(write_handler);}
    void SetErrorHandler(CallBack error_handler)     {ErrorHandler_ = std::move(error_handler);}
    void SetConnHandler(CallBack conn_handler)       {ConnHandler_ = std::move(conn_handler);}
    void SetDisconnHandler(CallBack disconn_handler) {DisConnHandler_ = std::move(disconn_handler);}

    /*根据revents_调用相应的回调函数*/
    void CallReventsHandlers();
    
    /*判断channel的事件是否修改过并更新last_events*/
    bool EqualAndUpdateLastEvents();
private:
    /*回调函数的调用*/
    void CallReadHandler()
    {
        if(ReadHandler_) ReadHandler_();
        else printf("read handler has not been registered yet\n");
    }
    void CallWriteHandler()
    {
        if(WriteHandler_) WriteHandler_();
        else printf("write handler has not been registered yet\n");
    }
    void CallErrorHandler()
    {
        if(ErrorHandler_) ErrorHandler_();
        else printf("error handler has not been registered yet\n");
    }
    void CallConnHandler()
    {
        if(ConnHandler_) ConnHandler_();
        else printf("connect handler has not been registered yet\n");
    }
    void CallDisconnHandler()
    {
        if(DisConnHandler_) DisConnHandler_();
        else printf("disconnect handler has not been registered yet\n");
    }
};


#endif //WEBSERVER_CHANNEL_H
