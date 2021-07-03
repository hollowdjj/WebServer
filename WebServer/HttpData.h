#ifndef WEBSERVER_HTTPDATA_H
#define WEBSERVER_HTTPDATA_H

/*Linux system APIs*/

/*STD Headers*/
#include <string>

/*User-define Headers*/

class EventLoop;
class Channel;

/*！
@Author: DJJ
@Description: HttpData类

 用于处理http数据。包含连接socket可读、可写、异常以及断开连接的回调函数。

@Date: 2021/6/15 下午8:09
*/

//TODO 引入HTTP
class HttpData {
private:
    Channel* connfd_channel_;                   //连接socket对应的Channel对象的智能指针
    EventLoop* sub_reactor_;                    //connfd_channel_属于的SubReactor
    std::string read_in_buffer;
    std::string write_out_buffer;
public:
    HttpData() = default;
    HttpData(EventLoop* sub_reactor,Channel* connfd_channel);
    ~HttpData();

    void HandleDisConn();                                           //调用断开连接函数
private:
    void ReadHandler();                                             //从连接socket读数据
    void WriteHandler();                                            //向连接socket写数据
    void DisConndHandler();                                         //连接socket断开连接
    void ErrorHandler(int fd,int error_num,std::string msg);        //错误处理
};


#endif //WEBSERVER_HTTPDATA_H
