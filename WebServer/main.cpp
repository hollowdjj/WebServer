#include <iostream>

#include "Utility.h"
int main()
{
    int port = 80;

    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::thread::hardware_concurrency() - 1);

    return 0;
}
