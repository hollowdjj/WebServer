/*！
@Author: DJJ
@Date: 2021/6/15 下午8:09
*/
#ifndef WEBSERVER_HTTPDATA_H
#define WEBSERVER_HTTPDATA_H

/*STD Headers*/
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <map>
#include <functional>

/*!
@brief 表示请求报文解析状态的枚举。
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
@brief 表示http请求报文中请求行解析状态的枚举。
*/
enum class RequestLineParseState{
    kParseAgain,
    kParseError,
    kParseSuccess,
};

/*!
@brief 表示http请求报文中首部行解析状态的枚举。
*/
enum class HeaderLinesParseState{
    kParseAgain,
    kParseError,
    kParseSuccess,
};

/*!
@brief 表示请求报文分析状态的枚举。
*/
enum class RequestMsgAnalysisState{
    kAnalysisError,
    kAnalysisSuccess,
};

/*!
@brief 用于处理http数据的类。
*/

/*前向声明*/
class EventLoop;
class Channel;
class Timer;

class HttpData {
private:
    Channel* p_connfd_channel_;                            //连接socket对应的Channel对象的智能指针
    EventLoop* p_sub_reactor_;                             //connfd_channel_属于的SubReactor
    Timer* p_timer_{};                                     //挂靠的定时器

    std::string read_in_buffer_{};                         //http请求报文
    std::string write_out_buffer_{};                       //http响应报文
    RequestMsgParseState request_msg_parse_state_;         //表示请求报文的解析状态
    std::map<std::string,std::string> fields_values_{};    //请求报文字段与其对应的值
    std::map<std::string,std::function<RequestMsgAnalysisState()>> method_proc_func_;  //请求报文中方法字段与业务处理函数的映射
public:
    HttpData() = default;
    HttpData(EventLoop* sub_reactor,Channel* connfd_channel);
    ~HttpData();

    /*!
    @brief 挂靠定时器并设置超时回调函数
    */
    void LinkTimer(Timer* p_timer);

    /*!
    @brief 获取挂靠的定时器，不要delete返回的指针
    */
    Timer* GetTimer() {return p_timer_;}
private:
    ///////////////////////////
    //       CallBacks       //
    ///////////////////////////

    /*!
    @brief EPOLLIN的回调函数。

    从连接socket读取http响应报文，并根据数据读取状态以及报文的解析状态一层
    层的推进，最终完成响应报文的编写和发送。可能需要多次调用才能完成数据的读
    取或请求报文的解析。
    */
    void ReadHandler();

    /*!
    @brief EPOLLOUT的回调函数。

    向连接socket写数据，即向客户端发送http响应报文。可能需要多次调用才能完
    成响应报文所有数据的发送。
    */
    void WriteHandler();

    /*!
    @brief EPOLLRDHUP的回调函数。

    EPOLLRDHUP信号的产生是由于客户端关闭了连接，此时服务端也对等关闭连接即可。
    */
    void DisConndHandler();

    /*!
    @brief 连接socket的超时回调函数。

    向客户端发送408 request time-out然后断开连接。
    */
    void ExpiredHandler();

    /*!
    @brief EPOLLERR的回调函数。

    连接socket出现错误时采取的策略为断开连接。
    */
    void ErrorHandler();

    //////////////////////////////
    // Http Msg Parase&Analysis //
    //////////////////////////////

    /*!
    @brief 解析http请求报文的请求行。

    请求行解析成功后，其方法字段、URI以及http协议版本会被存放在fields_values_中，
    对应的key值分别为method URI以及version。
    @return RequestLineParseState::kParseAgain   请求行数据不完整。
    @return RequestLineParseState::kParseError   请求行数据完整但有语法错误。
    @return RequestLineParseState::kParseSuccess 请求行解析成功。
    */
    RequestLineParseState ParseRequestLine();

    /*!
    @brief 解析http请求报文的首部行。

    首部行解析完成后，字段以及字段值将会存储在fields_values_中，索引即为字段本身
    此函数只会判断首部行数据是否完整以及首部航是否存在格式错误。不对字段是否有效以
    及字段的拼写是否正确做出判断。
    @return HeaderLinesParseState::kParseAgain  首部行数据不完整。
    @return HeaderLinesParseState::kParseError  首部行数据完整但格式不正确。
    @return HeaderLinesParseState::kParseSucess 首部行解析成功。
    */
    HeaderLinesParseState ParseHeaderLines();

    /*!
    @brief 分析请求报文并编写相应的响应报文。

    这里只响应GET POST以及HEAD方法。
    @return
    */
    RequestMsgAnalysisState AnalysisRequest();

    ///////////////////////////
    //  Http response proc   //
    ///////////////////////////

    /*!
    @brief 处理客户端的GET或HEAD请求。
    */
    RequestMsgAnalysisState ProcessGETorHEAD();
    
    /*!
    @brief 处理客户端的POST请求。
    */
    RequestMsgAnalysisState ProcessPOST();

    ///////////////////////////
    //        Tools          //
    ///////////////////////////

    /*!
    @brief 编写带错误代码和错误信息的响应报文。

    @param[in] fd         连接socket的文件描述符。
    @param[in] error_num  http错误代码。
    @param[in] msg        错误信息。
    */
    void SetHttpErrorMsg(int fd, int error_num, std::string msg);

    /*!
    @brief 互斥注册EPOLLIN和EPOLLOUT。

    对于连接soket，同时只能注册EPOLLIN，EPOLLOUT其中之一。
    @param[in] epollin true表示注册EPOLLIN删除EPOLLOUT，false则相反。
    */
    void MutexRegInOrOut(bool epollin);

    /*!
    @brief 还原http连接信息。

    清除连接信息并根据keep-alive设置超时时间。
    */
    void Reset();

    /*!
    @brief 编写响应报文中和请求报文中的方法字段无关的内容。
    */
    void FillPartOfResponseMsg();
};

/*!
@brief 保存文件后缀与文件类型的映射。
*/
class SourceMap{
private:
    static void Init();                                             //初始化mime_对象，必须保证只能调用一次
    SourceMap() = default;
public:
    SourceMap(const SourceMap&) = delete;
    SourceMap& operator=(const SourceMap&) = delete;

    static std::string GetMime(const std::string& suffix);          //根据后缀获取文件类型
private:
    static std::once_flag flag_;                                    //once_flag这里必须为static
    static std::unordered_map<std::string,std::string> mime_;       //文件后缀与文件类型的映射
};

#endif //WEBSERVER_HTTPDATA_H
