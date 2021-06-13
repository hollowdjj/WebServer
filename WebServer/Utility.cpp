#include "Utility.h"

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    assert(fcntl(fd,F_SETFL,new_option) != -1);
    return old_option;
}

int BindAndListen(int port)
{
    if(port<0 || port > 65535) return -1;
    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    if(listenfd == -1) return -1;

    auto close_return = [&listenfd](){ close(listenfd);return -1;};

    /*设置地址重用，实现端口复用，一般服务器都需要设置*/
    int reuse = 1;
    int res = setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse);
    if(res == -1) close_return();

    /*绑定地址*/
    sockaddr_in server_addr;
    bzero(&server_addr,sizeof server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    /*!
        INADDR_ANY泛指本机的意思。主要是考虑到主机具有多个网卡的情况。
        不管数据从哪个网卡过来，只要是绑定的端口号过来的数据，都可以接收。
     */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    res = bind(listenfd,reinterpret_cast<sockaddr*>(&server_addr),sizeof server_addr);
    if(res == -1) close_return();
    
    /*监听队列长度为2048*/
    res = listen(listenfd,2048);
    if(res == -1) close_return();

    return listenfd;
}