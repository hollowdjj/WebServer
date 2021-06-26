#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <assert.h>
#include <errno.h>
#include <sys/poll.h>
#include <unistd.h>
#include <string.h>
#include <string>
#include <iostream>
/*！
@Author: DJJ
@Description: 服务器测试代码，使用非阻塞connect
@Date: 2021/6/18 上午11:13
*/

//TODO 连接经常超时，存在bug
int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int NonBlockConnect(const char* ip,int port,int timeout)
{
    /*创建服务器监听socket的地址*/
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&server_addr.sin_addr);
    
    int sockfd = socket(PF_INET,SOCK_STREAM,0);
    assert( sockfd != -1);
    int old_option = SetNonBlocking(sockfd);    //设置成非阻塞才能同时发起多个连接

    int ret = connect(sockfd,reinterpret_cast<sockaddr*>(&server_addr),sizeof server_addr);
    if(ret == 0)
    {
        /*连接成功建立，返回*/
        printf("connection established through socket %d\n",sockfd);
        fcntl(sockfd,F_SETFL,old_option);
        return sockfd;
    }
    else if(errno != EINPROGRESS)
    {
        /*返回-1且错误代码为EINPROHRES时才表示连接正在进行当中*/
        printf("unblock connection unsupport\n");
        return -1;
    }

    /*连接正在进行中时，使用poll监听该socket，并在连接成功建立后清除错误代码*/
    pollfd fds[1];
    fds[0].events = POLLOUT;fds[0].revents = 0;
    int res = poll(fds,1, timeout);
    if(res<0)
    {
        /*出错*/
        printf("connection error: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    else if(res == 0)
    {
        /*超时*/
        printf("connection time out\n");
        close(sockfd);
        return -1;
    }
    /*获取socket上的错误代码，错误号为0才表示连接成功*/
    int error = 0;
    socklen_t len = sizeof error;
    if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&error,&len)<0)
    {
        printf("get socket option failed\n");
        close(sockfd);
        return -1;
    }
    if(error !=0)
    {
        printf("connection failed after poll with error: %s\n", strerror(errno));
        close(sockfd);
        return -1;
    }
    /*连接成功*/
    printf("connection success after poll with the socket: %d\n",sockfd);
    fcntl(sockfd,F_SETFL,old_option);
    return sockfd;
}
int main(int argc,char* argv[])
{
    if(argc <=2)
    {
        printf("usage: %s ip_address port_number\n",basename(argv[0]));
        return 1;
    }
    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int conn_num = 0;
    while(true)
    {
        if(conn_num >=5) break;

        int temp = NonBlockConnect(ip,port,200000);
        if(temp > 0)
        {
            const char* buf = "hello motherfucker!";
            std::cout<<"total "<<strlen(buf)<<" bytes"<<std::endl;
            int res = send(temp,buf,strlen(buf),0);
            std::cout<<"send "<<res<<" bytes"<<std::endl;
            ++conn_num;
        }
    }
    return 0;
}



