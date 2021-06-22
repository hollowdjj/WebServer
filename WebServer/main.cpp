#include "HttpServer.h"
#include <signal.h>

///////////////////////////
//   Global    Variables //
///////////////////////////
std::shared_ptr<EventLoop> main_reactor;
HttpServer* server;

//void CloseServer(int sig)
//{
//    server->Quit();
//    delete server;
//}

int main()
{
    /*设置服务器关闭的回调函数*/
//    if(signal(SIGTERM,CloseServer) == SIG_ERR)
//    {
//        printf("set CloseServer handler failed\n");
//        return -1;
//    }

    int port = 6688;
    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::thread::hardware_concurrency() - 1);
    main_reactor = std::make_shared<EventLoop>();
    server = CreateMainReactor(port,main_reactor,thread_pool);

    /*服务器开始运行*/
    server->Start();
    main_reactor->StartLoop();
    return 0;
}
