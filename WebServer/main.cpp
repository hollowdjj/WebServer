#include "HttpServer.h"
#include <chrono>
#include "Utility.h"
#include <stdlib.h>

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
    /*向所有SubReactor的tick_fd[1]写一个字节的数据并重新定时*/
    const char* msg = "Tick";
    for (auto& tick_fd : server->tickfds_)
    {
        WriteData(tick_fd,msg, strlen(msg));
//        assert(WriteData(tick_fd,msg, strlen(msg)!= strlen(msg)));
        //write(tick_fd,reinterpret_cast<char*>(&msg),1);
    }
    alarm(std::chrono::duration_cast<std::chrono::seconds>(GlobalVar::slot_interval).count());
}

int main()
{
    /*设置服务器关闭的回调函数*/
    if(signal(SIGTERM,CloseServer) == SIG_ERR)
    {
        printf("set SIGTERM handler failed\n");
        return -1;
    }
    if(signal(SIGALRM,AlarmTick) == SIG_ERR)
    {
        printf("set SIGALRM handler failed\n");
    }

    int port = 6688;
    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::thread::hardware_concurrency() - 1);
    EventLoop main_reactor = EventLoop(true);
    server = CreateHttpServer(port, &main_reactor, &thread_pool);
    /*服务器开始运行*/
    server->Start();
    alarm(std::chrono::duration_cast<std::chrono::seconds>(GlobalVar::slot_interval).count());
    main_reactor.StartLoop();

    return 0;
}
