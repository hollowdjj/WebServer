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
#include <map>
#include <functional>

/*User-define Headers*/

/*!
@brief 表示请求报文解析状态的枚举
*/
enum class RequestMsgParseState{
    kStart,
    kRequestLineOK,
    kHeaderLinesOK,
    kCheckBody,
    kAnalysisRequest,
    kFinish,
};
/*!
@brief 表示http请求报文中请求行解析状态的枚举
*/
enum class RequestLineParseState{
    kParseAgain,
    kParseError,
    kParseSuccess,
};
/*!
@brief 表示http请求报文中首部行解析状态的枚举
*/
enum class HeaderLinesParseState{
    kParseAgain,
    kParseError,
    kParseSuccess,
};
/*!
@brief 表示请求报文分析状态的枚举
*/
enum class RequestMsgAnalysisState{
    kAnalysisError,
    kAnalysisSuccess,
};
/*!
@brief 表示http请求是GET POST还是HEAD
*/
enum class HttpMethod{
    kEmpty,
    kGet,
    kPost,
    kHead,
};
/*!
@brief Http协议版本
*/
enum class HttpVersion{
    kEmpty,
    kHttp10,    //http/1.0
    kHttp11,    //http/1.1
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
    Channel* p_connfd_channel_;                                     //连接socket对应的Channel对象的智能指针
    EventLoop* p_sub_reactor_;                                      //connfd_channel_属于的SubReactor
    Timer* p_timer_{};                                              //挂靠的定时器

    std::string read_in_buffer_{};                                    //读取的Http响应报文
    std::string write_out_buffer_{};                                  //http响应报文
    std::string filename_{};                                          //客户端请求的资源文件名
    //bool keep_alive_ = false;                                         //true表示长连接，false表示短连接
    std::map<std::string,std::string> fields_values_{};              //首部字段与其对应的值

    RequestMsgParseState request_msg_parse_state_;                  //表示请求报文的解析状态
    HttpMethod http_method_;                                        //表示为GET POST还是HEAD
    HttpVersion http_version_;                                      //http协议版本号

    std::map<HttpMethod,std::function<RequestMsgAnalysisState()>> method_proc_func_;  //请求报文中方法字段与业务处理函数的映射
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
    void ExpiredHandler();                                          //连接socket的超时处理
    void ErrorHandler(int fd,int error_num,std::string msg);        //错误处理

    /*解析http数据的函数*/
    RequestLineParseState ParseRequestLine();                       //解析http请求报文的请求行，判断是否有语法错误
    HeaderLinesParseState ParseHeaderLines();                       //解析http请求报文的首部行，判断是否有语法错误
    RequestMsgAnalysisState AnalysisRequest();                      //分析请求报文并编写相应的响应报文

    /*GET POST HEAD方法对应的业务处理函数*/
    RequestMsgAnalysisState ProcessGETorHEAD();
    RequestMsgAnalysisState ProcessPOST();

    
    /*工具函数*/
    void MutexRegInOrOut(bool epollin);                             //对于连接soket，同时只能注册EPOLLIN，EPOLLOUT其中之一
    void Reset();                                                   //还原
    void FillPartOfResponseMsg();                                   //编写响应报文中和请求报文中的方法字段无关的内容
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
