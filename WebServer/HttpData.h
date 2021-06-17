#ifndef WEBSERVER_HTTPDATA_H
#define WEBSERVER_HTTPDATA_H

/*Linux system APIs*/

/*STD Headers*/

/*User-define Headers*/
#include "EventLoop.h"

/*！
@Author: DJJ
@Description: HttpData类

 处理http数据。包含连接socket可读、可写、异常以及断开连接的回调函数
 （目前先不涉及http，先使用tcp测试一下）

@Date: 2021/6/15 下午8:09
*/
class HttpData {
private:
    std::shared_ptr<Channel> connfd_channel_;   //连接socket对应的Channel对象的智能指针
    std::shared_ptr<EventLoop> sub_reactor_;    //connfd_channel_属于的SubReactor
    std::string read_in_buffer;
    std::string write_out_buffer;
public:
    HttpData() = default;
    HttpData(std::shared_ptr<EventLoop> sub_reactor,std::shared_ptr<Channel> connfd_channel);

private:
    void ReadHandler();                                             //从连接socket读数据
    void WriteHandler();                                            //向连接socket写数据
    void DisConndHandler();                                         //连接socket断开连接
    void ErrorHandler(int fd,int error_num,std::string msg);        //错误处理
};


#endif //WEBSERVER_HTTPDATA_H
