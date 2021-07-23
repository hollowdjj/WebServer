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
@Description: 服务器测试代码。由于是在同一台主机上测试，使用阻塞connect即可

  这里只是简单测试一下服务端GET HEAD和POST方法有无明显的bug，并发测试使用webench完成。
@Date: 2021/6/18 上午11:13
*/

int CreatTcpConn(const char* ip, int port)
{
    /*创建服务器监听socket的地址*/
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&server_addr.sin_addr);
    
    int sockfd = socket(PF_INET,SOCK_STREAM,0);
    assert( sockfd != -1);

    int ret = connect(sockfd,reinterpret_cast<sockaddr*>(&server_addr),sizeof server_addr);
    if(ret == 0)
    {
        /*成功建立tcp连接*/
        printf("connection established through socket %d\n",sockfd);
        return sockfd;
    }
    else
    {
        printf("connection failed: %s\n", strerror(errno));
        return -1;
    }
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

    std::string get;
    get += "GET /favicon.ico HTTP/1.1\r\n";
    get += "Connection: keep-alive\r\n";
    get +="\r\n";

    std::string head;
    head += "HEAD /index.html HTTP/1.1\r\n";
    head += "Connection: close\r\n";
    head += "\r\n";

    std::string post; std::string body = "abcdefg";
    post += "POST / HTTP/1.1\r\n";
    post += "Connection: keep-alive\r\n";
    post += "Content-Type: text/plain\r\n";
    post += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    post +="\r\n";
    post += body;

    int conn_num = 0;
    int sockfd = CreatTcpConn(ip, port);
    sleep(0.5);
    if(sockfd > 0)
    {
        auto buf = head.c_str();
        std::cout<<"total "<<head.size()<<" bytes"<<std::endl;
        ssize_t res = send(sockfd, buf, strlen(buf), 0);
        std::cout<<"send "<<res<<" bytes"<<std::endl;
        ++conn_num;
    }

    while(true)
    {
        char buffer[4096];
        ssize_t num = recv(sockfd,buffer,4096,0);
        if(num > 0)
        {
            std::cout<<"----------------------recv------------------------"<<std::endl;
            std::cout<<buffer<<std::endl;
        }
        else if( num == 0)
        {
            close(sockfd);
            break;
        }
    }
    //close(sockfd);
    return 0;
}



