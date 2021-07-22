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
#include <iostream>
#include <chrono>

/*！
@Author: DJJ
@Description: 定义工具函数以及工具类
@Date: 2021/6/13 上午10:32
*/

/*全局变量结构体*/
struct GlobalVar{
    static const int kMaxUserNum = 100000;                              //最大并发连接数量
    static std::chrono::seconds slot_interval;                          //时间轮的槽间隔
    static std::chrono::seconds default_timeout ;                       //tcp连接建立后，等待请求报文的超时时间
    static std::chrono::seconds keep_alive_timeout;                     //长连接的超时时间
    static int slot_num;                                                //时间轮的槽数
    static char favicon[555];
};

/*!
@brief 将文件描述符fd设置为非阻塞模式
*/
int SetNonBlocking(int fd);
/*!
@brief 绑定端口号并监听。成功时返回监听socket的文件描述符，否则返回-1
*/
int BindAndListen(int port);
/*!
@brief 返回GMT时间
*/
std::string GetTime();
/*!
@brief ET模式下从文件描述符(非socket)读n个字节的数据

@param[in] fd     文件描述符
@param[in] dest   存放数据的数组首地址
@param[in] n      期望读取的字节数
@return           成功读取的字节数。-1表示数据读取出粗
*/
ssize_t ReadData(int fd, char* dest, size_t n);

/*!
@brief ET模式下从连接socket读取数据

@param[in] fd           连接socket的文件描述符
@param[in] buffer       存放数据的string
@param[in] disconnect   true表示客户端已断开连接，false表示客户端未断开连接
@return                 成功读取的字节数。-1表示数据读取出错
*/
ssize_t  ReadData(int fd,std::string& buffer,bool& disconnect);

/*!
@brief ET模式下向文件描述符(非socket)写n个字节的数据

@param[in] fd      文件描述符
@param[in] source  待写数据的首地址
@param[in] n       待写数据的字节数
@return            成功写出的字节数。-1表示写数据出错。返回0不一定表示source的数据都写完了
*/
ssize_t WriteData(int fd, const char* source, size_t n);

/*!
@brief ET模式下向连接socket写入数据并删除buffer中成功写出的数据

@param[in] fd      连接socket的文件描述符
@param[in] buffer  待写的数据
@return            成功写出的字节数。-1表示写数据出错。返回0不一定表示buffer的所有数据都写完了
*/
ssize_t WriteData(int fd,std::string& buffer);

/*线程池*/
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

    /*返回线程池中的线程数量*/
    decltype(workers_.size()) size() {return workers_.size();};
};

/*模板成员函数的定义也必须写在头文件中*/
template<typename F,typename... Args>
auto ThreadPool::AddTaskToPool(F&& f,Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<return_type()>>
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
#endif