/*！
@Author: DJJ
@Date: 2021/8/4 下午9:39
*/
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <chrono>
#include <vector>
#include <string>

std::vector<int> clients;

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

void AddFd(int epoll_fd,int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epoll_fd,EPOLL_CTL_ADD,fd,&event);
    SetNonBlocking(fd);
}

bool Write_N_Bytes(int sockfd,const char* buffer,int len)
{
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n",len,sockfd);
    while(true)
    {
        bytes_write = send(sockfd,buffer,len,0);
        if(bytes_write == -1) return false;
        else if(bytes_write == 0) return false;

        len -= bytes_write;
        buffer = buffer + bytes_write;
        if(len <= 0) return true;
    }
}

bool Read_Once(int sockfd,char* buffer,int len)
{
    int bytes_read = 0;
    memset(buffer,'\0',len);
    bytes_read = recv(sockfd,buffer,len,0);
    if(bytes_read == -1) return false;
    else if(bytes_read == 0) return false;

    printf("read in %d bytes from socket %d with content: \n",bytes_read,sockfd);
    return true;
}

void Start_Conn(int epoll_fd,const char* ip,int port,int num)
{
    sockaddr_in server_address;
    server_address.sin_port = htons(port);
    server_address.sin_family = AF_INET;
    inet_pton(AF_INET,ip,&server_address.sin_addr);

    for (int i = 0; i < num; ++i)
    {
        int sockfd = socket(PF_INET,SOCK_STREAM,0);
        if(sockfd < 0) continue;
        if(connect(sockfd,(sockaddr*)&server_address,sizeof server_address) == 0)
        {
            printf("build connection %d\n",i);
            AddFd(epoll_fd,sockfd);
            clients.push_back(sockfd);
        }
    }
}

void Close_Conn(int epoll_fd,int sockfd)
{
    epoll_ctl(epoll_fd,EPOLL_CTL_DEL,sockfd,0);
    printf("Close connection %d\n",sockfd);
    close(sockfd);
}

/*!
@brief usage: ./stress ip_addr port_number client_number last_time keep_alive
*/
int main(int argc,char* argv[])
{
    signal(SIGPIPE, SIG_IGN);
    bool keep_alive = atoi(argv[5]);
    std::string connection = keep_alive ? "keep-alive\r\n" : "close\r\n";
    std::string request = "GET http://localhost/hello HTTP/1.1\r\nConnection: " + connection + "\r\n";

    int epoll_fd = epoll_create(100);
    Start_Conn(epoll_fd, argv[1],atoi(argv[2]), atoi(argv[3]));
    epoll_event events[10000];
    char buffer[2048];
    int last_time = atoi(argv[4]);
    auto start_time = std::chrono::steady_clock::now();
    while(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - start_time).count() <=last_time)
    {
        int res = epoll_wait(epoll_fd,events,10000,2000);
        for (int i = 0; i < res; ++i)
        {
            int sockfd = events[i].data.fd;
            if(events[i].events & EPOLLIN)
            {
                if(!Read_Once(sockfd,buffer,2048)) Close_Conn(epoll_fd,sockfd);
                if(keep_alive)
                {
                    epoll_event event;
                    event.events = EPOLLET | EPOLLOUT | EPOLLERR | EPOLLRDHUP;
                    event.data.fd = sockfd;
                    if(epoll_ctl(epoll_fd,EPOLL_CTL_MOD,sockfd,&event) < 0)
                    {
                        printf("epoll_ctl error: %s\n", strerror(errno));
                        Close_Conn(epoll_fd,sockfd);
                    }
                }
                else
                {
                    Close_Conn(epoll_fd,sockfd);
                    Start_Conn(epoll_fd, argv[1],atoi(argv[2]), 1);
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                if(!Write_N_Bytes(sockfd,request.c_str(),strlen(request.c_str()))) Close_Conn(epoll_fd,sockfd);
                epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR | EPOLLRDHUP;
                event.data.fd = sockfd;
                if(epoll_ctl(epoll_fd,EPOLL_CTL_MOD,sockfd,&event) < 0)
                {
                    printf("epoll_ctl error: %s\n", strerror(errno));
                    Close_Conn(epoll_fd,sockfd);
                }
                Start_Conn(epoll_fd, argv[1],atoi(argv[2]), 1);
            }
            else if(events[i].events & EPOLLERR) Close_Conn(epoll_fd,sockfd);
            //else if(events[i].events & EPOLLRDHUP) Close_Conn(epoll_fd,sockfd);
        }
    }
    if(keep_alive)
    {
        for (const auto& item : clients)
        {
            Close_Conn(epoll_fd,item);
        }
    }
    return 0;
}

