/*！
@Author: DJJ
@Date: 2021/6/12 下午5:34
*/
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

/*!
@brief Channel类，即一个底层事件类。
*/
class HttpData;

class Channel {
private:
    int fd_;                       //需要监听事件的文件描述符
    bool is_connfd_;               //表示fd_是否为连接socket
    __uint32_t events_{};          //文件描述符fd_需要监听的事件
    __uint32_t revents_{};         //events_中的就绪事件
    __uint32_t last_events_{};     //mod之前注册的事件

    using CallBack = std::function<void()>;
    CallBack read_handler_;         //EPOLLIN的回调函数
    CallBack write_handler_;        //EPOLLOUT的回调函数
    CallBack error_handler_;        //EPOLLERR的回调函数
    CallBack disconn_handler_;      //EPOLLRDHUP的回调函数

    HttpData* p_holder_{};          //只有连接socket才需要一个holder，监听socket不需要
public:
    Channel(int fd, bool is_connfd);
    ~Channel();

    /*!
    @brief set and get fd_.
    */
    [[maybe_unused]]void SetFd(int fd) {fd_ = fd;}
    int GetFd()                        {return fd_;}

    /*!
    @brief set and get events_.
    */
    void SetEvents(__uint32_t events)  {events_ = events;}
    __uint32_t  GetEvents()            {return events_;}

    /*!
    @brief set and get revents_.
    */
    void SetRevents(__uint32_t revents)      {revents_ = revents;}
    [[maybe_unused]] __uint32_t GetRevents() {return revents_;}

    /*!
    @brief set and get is_connfd_.
    */
    [[maybe_unused]]void SetIsConnfd(bool is_connfd) {is_connfd_ = is_connfd;}
    bool IsConnfd()                                  {return is_connfd_;}

    /*!
    @brief set and get holder.
    */
    void SetHolder(HttpData* holder) {p_holder_ = holder;}
    HttpData* GetHolder()            {return p_holder_;}

    /*!
    @brief 注册回调函数，std::function是cheap object，pass by value + move就可。
    */
    void SetReadHandler(CallBack read_handler)       { read_handler_ = std::move(read_handler);}
    void SetWriteHandler(CallBack write_handler)     { write_handler_ = std::move(write_handler);}
    void SetErrorHandler(CallBack error_handler)     { error_handler_ = std::move(error_handler);}
    void SetDisconnHandler(CallBack disconn_handler) { disconn_handler_ = std::move(disconn_handler);}

    /*!
    @brief 根据revents_调用相应的回调函数。
    */
    void CallReventsHandlers();

    /*!
    @brief 判断channel的事件是否修改过并更新last_events。
    */
    bool EqualAndUpdateLastEvents();
private:
    /*!
    @brief 回调函数的调用。
    */
    void CallReadHandler();
    void CallWriteHandler();
    void CallErrorHandler();
    void CallDisconnHandler();
};

#endif //WEBSERVER_CHANNEL_H
