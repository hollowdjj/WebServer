/*！
@Author: DJJ
@Date: 2021/6/15 下午8:09
*/

#ifndef WEBSERVER_HTTPDATA_H
#define WEBSERVER_HTTPDATA_H
/*Linux system APIs*/

/*STD Headers*/
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
/*User-define Headers*/

/*!
@brief 表示http请求报文中请求行解析状态的枚举
*/
enum class RequestLineParseState{
    kParseError,
    kParseSuccess
};

/*!
@brief 表示http请求报文中首部行解析状态的枚举
*/
enum class HeaderLinesParseState{
    kParseError,
    kParseSuccess
};
/*!
@brief 表示http请求是GET POST还是HEAD
*/
enum HttpMethod{
    kEmpty,
    kGet,
    kPost,
    kHead
};

/*!
@brief Http协议版本
*/
enum HttpVersion{
    kHttp10,    //http/1.0
    kHttp11     //http/1.1
};

/*!
@brief 用于处理http数据的类
*/

/*前向声明*/
class EventLoop;
class Channel;
class Timer;

//TODO 引入HTTP
class HttpData {
private:
    Channel* p_connfd_channel_;                   //连接socket对应的Channel对象的智能指针
    EventLoop* p_sub_reactor_;                    //connfd_channel_属于的SubReactor
    Timer* p_timer_;                              //挂靠的定时器

    std::string read_in_buffer_;                  //读取的Http响应报文
    std::string write_out_buffer_;                //http响应报文
    HttpMethod http_method_;                      //表示为GET POST还是HEAD
    std::string filename_;                        //客户端请求的资源文件名
    HttpVersion http_version_;                    //http协议版本号
public:
    HttpData() = default;
    HttpData(EventLoop* sub_reactor,Channel* connfd_channel);
    ~HttpData();

    void LinkTimer(Timer* p_timer);              //挂靠定时器
    Timer* GetTimer() {return p_timer_;}         //获取挂靠的定时器
private:
    /*回调函数*/
    void ReadHandler();                                             //从连接socket读数据
    void WriteHandler();                                            //向连接socket写数据
    void DisConndHandler();                                         //连接socket断开连接
    void ErrorHandler(int fd,int error_num,std::string msg);        //错误处理
    void ExpiredHandler();                                          //连接socket的超时处理
    /*解析http数据的函数*/
    RequestLineParseState ParseRequestLine();                       //解析http请求报文的请求行
    HeaderLinesParseState ParseHeaderLines();                       //解析http请求报文的首部行
    
    /*工具函数*/
    void MutexRegInOrOut(bool epollin);                             //对于连接soket，同时只能注册EPOLLIN，EPOLLOUT其中之一

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
