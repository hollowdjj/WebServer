#include "Utility.h"

int SetNonBlocking(int fd)
{
    int old_option = fcntl(fd,F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd,F_SETFL,new_option);
    return old_option;
}

int BindAndListen(int port)
{
    if(port<0 || port > 65535) return -1;
    int listenfd = socket(PF_INET,SOCK_STREAM,0);
    if(listenfd == -1)
    {
        printf("create socket error: %s\n", strerror(errno));
        close(listenfd);
        return -1;
    }

    /*设置地址重用，实现端口复用，一般服务器都需要设置*/
    int reuse = 1;
    int res = setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof reuse);
    if(res == -1)
    {
        printf("set socketpot error: %s\n",strerror(errno));
        close(listenfd);
        return -1;
    }

    /*绑定地址*/
    sockaddr_in server_addr;
    bzero(&server_addr,sizeof server_addr);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    /*!
        INADDR_ANY泛指本机的意思。主要是考虑到主机具有多个网卡的情况。
        不管数据从哪个网卡过来，只要是绑定的端口号过来的数据，都可以接收。
     */
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    res = bind(listenfd,reinterpret_cast<sockaddr*>(&server_addr),sizeof server_addr);
    if(res == -1)
    {
        printf("bind error: %s\n",strerror(errno));
        close(listenfd);
        return -1;
    }
    
    /*监听队列长度为2048*/
    res = listen(listenfd,2048);
    if(res == -1)
    {
        printf("listen error: %s\n",strerror(errno));
        close(listenfd);
        return -1;
    }

    return listenfd;
}

ThreadPool::ThreadPool(size_t thread_num) : stop_(false)
{
    for (int i = 0; i < thread_num; ++i)
    {
        workers_.emplace_back([this]()
                              {
                                  while(true)
                                  {
                                      std::function<void()> task;
                                      /*从任务队列中取出一项任务并执行，若任务队列为空则休眠线程*/
                                      {
                                          std::unique_lock<std::mutex> locker(this->mutex_);
                                          /*!
                                              只要stop_ == true即线程池已经停止使用了就一直休眠
                                              当stop_ == false，即线程池开启时，还要任务队列不为空才会唤醒是，否则休眠
                                           */
                                          this->cond_.wait(locker,[this](){return this->stop_ || !this->tasks_.empty();});
                                          /*当stop_为true且任务队列为空时，线程会被唤醒，此时退出*/
                                          if(this->stop_ && this->tasks_.empty()) return;

                                          task = this->tasks_.front();
                                          this->tasks_.pop();
                                      }
                                      task();
                                  }
                              });
    }
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> locker(mutex_);
        stop_ = true;
    }
    for (auto& item : workers_) item.join();
}