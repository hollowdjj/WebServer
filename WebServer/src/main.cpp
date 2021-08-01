/*Linux System APIs*/
#include <getopt.h>
#include <libgen.h>

/*STD Headers*/
#include <chrono>
#include <regex>
#include <stdlib.h>

/*User-define Headers*/
#include "HttpServer.h"
#include "EventLoop.h"
#include "Utility.h"

///////////////////////////
//   Global    Variables //
///////////////////////////
HttpServer* server;

static void SigThread(void* arg)
{
    sigset_t* sigset = (sigset_t*)arg;
    bool stop = false;
    while(!stop)
    {
        int ret,sig;
        ret = sigwait(sigset, &sig);
        if(ret != 0)
        {
            ::GetLogger()->error("sigwait error: {}", strerror(ret));
            exit(EXIT_FAILURE);
        }
        /*SIGALRM信号处理*/
        else if(sig == SIGALRM)
        {
            const char* msg = "Tick";
            for (auto& tick_fd : server->tickfds_)
            {
                //TODO 可能没有写出数据 子线程屏蔽信号
                WriteData(tick_fd,msg, strlen(msg));
            }
            alarm(std::chrono::duration_cast<std::chrono::seconds>(GlobalVar::slot_interval_).count());
        }
        /*SIGTREM信号处理*/
        else if(sig == SIGTERM)
        {
            server->Quit();
            delete server;
            stop = true;
            exit(0);
        }
        /*SIGPIPE信号处理*/
        else if(sig == SIGPIPE)
        {
            //do nothing
        }
    }
}

int main(int argc,char* argv[])
{
    /*解析命令行参数*/
    auto res = ParaseCommand(argc,argv);
    if(!res)
    {
        printf("usage: %s [-p port_number] [-s subreactor_number ] [-l log_file_path(start with .)]",basename(argv[0]));
        return 0;
    }

    /*开启日志*/
    if(!GetLogger(std::get<2>(*res))) return 0;
    ::GetLogger()->set_pattern("[%Y-%m-%d %H:%M:%S] [thread %t] [%l] %v");

    /*!
       在Linux当中，进程的所有线程共享信号。线程可以通过设置信号掩码来屏蔽掉某些信号。
       然而，在这里子线程是通过线程池创建的，不太好添加信号掩码，况且在每个线程中单独
       设置信号掩码也很容易导致逻辑错误。因此，最好的方法是专门定义一个线程去处理所有
       信号。首先需要在所有子线程创建之前，在主线程中设置好信号掩码。随后创建的子线程
       会自动继承这个信号掩码。这样做了之后，所有线程都不会响应被屏蔽的信号。因此，需
       要再单独创建一个线程，并通过调用sigwait函数来等待信号并处理。
     */
    sigset_t sigset;
    sigemptyset(&sigset);
    sigaddset(&sigset,SIGALRM);
    sigaddset(&sigset,SIGTERM);
    sigaddset(&sigset,SIGPIPE);
    assert(pthread_sigmask(SIG_BLOCK, &sigset,nullptr) == 0);

    /*创建一个线程池。注意主线程不在线程池中*/
    ThreadPool thread_pool(std::get<1>(*res));
    EventLoop main_reactor = EventLoop(true);
    server = CreateHttpServer(std::get<0>(*res), &main_reactor, &thread_pool);

    /*开启一个后台信号处理函数*/
    std::thread sig_thread(SigThread,(void*)&sigset);
    sig_thread.detach();

    /*服务器开始运行*/
    server->Start();
    alarm(std::chrono::duration_cast<std::chrono::seconds>(GlobalVar::slot_interval_).count());
    main_reactor.StartLoop();

    return 0;
}
