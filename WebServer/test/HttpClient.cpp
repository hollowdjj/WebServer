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
@Description: 服务器测试代码。由于实在同一台主机上测试，使用阻塞connect即可
@Date: 2021/6/18 上午11:13
*/

int NonBlockConnect(const char* ip,int port)
{
    /*创建服务器监听socket的地址*/
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&server_addr.sin_addr);
    
    int sockfd = socket(PF_INET,SOCK_STREAM,0);
    assert( sockfd != -1);

    int ret = connect(sockfd,reinterpret_cast<sockaddr*>(&server_addr),sizeof server_addr);
    if(ret == 0)
    {
        /*连接成功建立，返回*/
        printf("connection established through socket %d\n",sockfd);
        return sockfd;
    }

    printf("connection failed: %s\n", strerror(errno));
    return -1;
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
        if(conn_num >=5) continue;

        int temp = NonBlockConnect(ip,port);
        sleep(0.5);
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



