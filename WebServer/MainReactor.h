#ifndef WEBSERVER_MAINREACTOR_H
#define WEBSERVER_MAINREACTOR_H

/*Linux system APIS*/
#include <netinet/in.h>

/*STD Headers*/
#include <memory>

/*User-define Heades*/
#include "Channel.h"
#include "Utility.h"

/*！
@Author: DJJ
@Description: MainReactor类

 负责监听客户端的连接请求，事件触发后，交由acceptor对象以建立连接。
 随后，MainReactor对象再将连接socket分发给SubReactor

@Date: 2021/6/12 下午4:29
*/

class MainReactor {
private:
    int listenfd_;                              //监听socket
    int port_;                                  //端口号
    std::shared_ptr<Channel> listen_channel_;   //监听socket的Channel
    const int kMaxUserNum = 100000;             //最大并发连接数量
    int current_user_num = 0;                   //当前用户数量

public:
    friend MainReactor& CreateMainReactor(int port);
    MainReactor(const MainReactor&) = delete;
    MainReactor& operator=(const MainReactor&) = delete;

    void Start();
private:
    explicit MainReactor(int port);                              //私有构造函数以限制对象的数量
    void NewConnHandler();                                       //监听socket可读事件就绪的回调函数
    void ErrorHandler();                                         //监听socket错误处理的回调函数
};


inline
MainReactor& CreateMainReactor(int port)
{
    static MainReactor main_reactor(port);
    return main_reactor;
}
#endif //WEBSERVER_MAINREACTOR_H
