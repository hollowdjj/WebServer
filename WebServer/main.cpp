#include <iostream>


#include "HttpServer.h"

int main()
{
    int port = 80;
    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::thread::hardware_concurrency() - 1);
    auto main_reactor = std::make_shared<EventLoop>();
    auto server = CreateMainReactor(port,main_reactor);
    server->Start();
    main_reactor->StartLoop();
    return 0;
}
