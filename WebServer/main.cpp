#include "HttpServer.h"
#include <signal.h>

///////////////////////////
//   Global    Variables //
///////////////////////////
HttpServer* server;

void CloseServer(int sig)
{
    server->Quit();
    delete server;
    exit(0);
}
void AlarmTick(int sig)
{

}

//TODO 获取子线程的管道，然后定时向这些管道写数据。内核会将信号交由进程号最小的那个线程进行处理
int main()
{
    /*设置服务器关闭的回调函数*/
    if(signal(SIGTERM,CloseServer) == SIG_ERR)
    {
        printf("set CloseServer handler failed\n");
        return -1;
    }
    int port = 6688;
    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::thread::hardware_concurrency() - 1);
    EventLoop main_reactor = EventLoop();
    server = CreateHttpServer(port, &main_reactor, &thread_pool);

    /*服务器开始运行*/
    server->Start();
    main_reactor.StartLoop();
    return 0;
}
