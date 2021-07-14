#ifndef WEBSERVER_HTTPDATA_H
#define WEBSERVER_HTTPDATA_H

/*Linux system APIs*/

/*STD Headers*/
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
/*User-define Headers*/

/*前向声明*/
class EventLoop;
class Channel;
class Timer;

/*！
@Author: DJJ
@Description: HttpData类

 用于处理http数据。包含连接socket可读、可写、异常以及断开连接的回调函数。

@Date: 2021/6/15 下午8:09
*/

//TODO 引入HTTP
class HttpData {
private:
    Channel* p_connfd_channel_;                   //连接socket对应的Channel对象的智能指针
    EventLoop* p_sub_reactor_;                    //connfd_channel_属于的SubReactor
    Timer* p_timer_;                              //挂靠的定时器
    std::string read_in_buffer;
    std::string write_out_buffer;
public:
    HttpData() = default;
    HttpData(EventLoop* sub_reactor,Channel* connfd_channel);
    ~HttpData();

    void LinkTimer(Timer* p_timer);              //挂靠定时器
    Timer* GetTimer() {return p_timer_;}         //获取挂靠的定时器
private:
    void ReadHandler();                                             //从连接socket读数据
    void WriteHandler();                                            //向连接socket写数据
    void DisConndHandler();                                         //连接socket断开连接
    void ErrorHandler(int fd,int error_num,std::string msg);        //错误处理
    void ExpiredHandler();                                          //连接socket的超时处理
};

/*！
@Author: DJJ
@Description: Mime类

 保存文件后缀与文件类型的映射。

@Date: 2021/7/14 下午7:25
*/
class MimeType{
private:
    static void Init();                                             //初始化mime_对象，必须保证只能调用一次
    static std::unordered_map<std::string,std::string> mime_;       //文件后缀与文件类型的映射
    MimeType() = default;
public:
    MimeType(const MimeType&) = delete;
    MimeType& operator=(const MimeType&) = delete;

    static std::string GetMime(const std::string& suffix);          //根据后缀获取文件类型
private:
    static std::once_flag flag_;                                    //once_flag这里必须为static
};

#endif //WEBSERVER_HTTPDATA_H
