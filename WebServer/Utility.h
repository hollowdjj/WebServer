#ifndef WEBSERVER_UTILITY_H
#define WEBSERVER_UTILITY_H

/*Linux system APIS*/
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <assert.h>

/*STD Headers*/
#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
/*！
@Author: DJJ
@Description: 定义工具函数以及工具类
@Date: 2021/6/13 上午10:32
*/

/*!
@brief: 将文件描述符fd设置为非阻塞模式
*/
int SetNonBlocking(int fd);

/*!
@brief: 绑定端口号并监听。成功时返回监听socket，否则返回-1
*/
int BindAndListen(int port);


class ThreadPool{
private:
    std::vector<std::thread> workers_;           //线程池
    std::queue<std::function<void()>> tasks_;    //任务队列
    std::mutex mutex_;                           //互斥锁
    std::condition_variable cond_;               //条件变量
    bool stop_;                                  //表示线程池是否启用
public:
    explicit ThreadPool(size_t thread_num);
    ~ThreadPool();

    /*向线程池中添加任务*/
    template<typename F,typename... Args>
    auto AddTaskToPool(F&& f,Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;
};

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
                    cond_.wait(locker,[this](){return this->stop_ || !this->tasks_.empty();});
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

template<typename F,typename... Args>
auto ThreadPool::AddTaskToPool(F&& f,Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type>>
            (std::bind(std::forward<F>(f),std::forward<Args>(args)...));
    std::future<return_type> res = task->get_future();

    {
        std::unique_lock<std::mutex> locker(mutex_);
        if(stop_)
            throw std::runtime_error("add task to a stoped threadpool");

        tasks_.emplace([task](){(*task)();});
    }

    cond_.notify_one();
    return res;
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> locker(mutex_);
        stop_ = true;
    }
    for (auto& item : workers_) item.join();
}
#endif