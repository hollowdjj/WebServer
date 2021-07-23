#include "HttpServer.h"
#include <chrono>
#include "Utility.h"
#include <stdlib.h>

///////////////////////////
//   Global    Variables //
///////////////////////////
HttpServer* server;

/*注意：一个进程中的所有线程共享一个信号处理函数*/
void SIGTERM_Handler(int sig)
{
    server->Quit();
    delete server;
    exit(0);
}

void SIGPIPE_Handler(int sig)
{
    //do nothing
}

void SIGALRM_Handler(int sig)
{
    /*向所有SubReactor的tick_fd[1]写一个字节的数据并重新定时*/
    const char* msg = "Tick";
    for (auto& tick_fd : server->tickfds_)
    {
        //TODO 可能没有写出数据
        WriteData(tick_fd,msg, strlen(msg));
    }
    alarm(std::chrono::duration_cast<std::chrono::seconds>(GlobalVar::slot_interval_).count());
}

int main()
{
    /*设置信号处理的回调函数*/
    if(signal(SIGTERM, SIGTERM_Handler) == SIG_ERR)
    {
        printf("set SIGTERM handler failed\n");
        return -1;
    }
    if(signal(SIGALRM, SIGALRM_Handler) == SIG_ERR)
    {
        printf("set SIGALRM handler failed\n");
        return -1;
    }
    if(signal(SIGPIPE, SIGPIPE_Handler) == SIG_ERR)
    {
        printf("set SIGPIPE handler failed\n");
        return -1;
    }

    int port = 6688;
    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::thread::hardware_concurrency() - 1);
    EventLoop main_reactor = EventLoop(true);
    server = CreateHttpServer(port, &main_reactor, &thread_pool);
    /*服务器开始运行*/
    server->Start();
    alarm(std::chrono::duration_cast<std::chrono::seconds>(GlobalVar::slot_interval_).count());
    main_reactor.StartLoop();

    return 0;
}
