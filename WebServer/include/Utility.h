/*！
@Author: DJJ
@Date: 2021/6/13 上午10:32
*/
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
#include <optional>

/*Third-Party*/
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

/*!
@brief 全局变量
*/
struct GlobalVar{
    static const int kMaxUserNum = 100000;               //最大并发连接数量
    static int total_user_num_;                          //当前总连接数
    static int slot_num_;                                //时间轮的槽数
    static std::chrono::seconds slot_interval_;          //时间轮的槽间隔
    static std::chrono::seconds client_header_timeout_;  //tcp连接建立后,必须在该时间内接收到完整的请求行和首部行，否则超时
    static std::chrono::seconds client_body_timeout_;    //实体数据两相邻包到达的间隔时间不能超过该时间，否则超时
    static std::chrono::seconds keep_alive_timeout_;     //长连接的超时时间
    static char favicon[555];
    /*!
    @brief 总连接数加一。
    */
    static void IncTotalUserNum()
    {
        std::unique_lock locker(mutex_);
        ++total_user_num_;
    }
    /*!
    @brief 总连接数减一。
    */
    static void DecTotalUserNum()
    {
        std::unique_lock locker(mutex_);
        --total_user_num_;
    }
    /*!
    @brief 返回当前总连接数。
    */
    static int GetTotalUserNum()
    {
        std::unique_lock locker(mutex_);
        return total_user_num_;
    }
private:
    static std::mutex mutex_;    //total_user_num_需要互斥访问
};

/*!
@brief 将文件描述符fd设置为非阻塞模式。
*/
int SetNonBlocking(int fd);

/*!
@brief 绑定端口号并监听。成功时返回监听socket的文件描述符，否则返回-1。全连接队列长度设为2048。
*/
int BindAndListen(int port);

/*!
@brief 返回GMT时间。
*/
std::string GetTime();

/*!
@brief ET模式下从文件描述符(非socket)读n个字节的数据。

@param[in] fd     文件描述符。
@param[in] dest   数据存放的首地址。
@param[in] n      期望读取的字节数。
@return    成功读取的字节数，可能小于n。-1表示数据读取出错。
*/
ssize_t ReadData(int fd, char* dest, size_t n);

/*!
@brief ET模式下从连接socket读取数据。

@param[in] fd           连接socket的文件描述符。
@param[in] buffer       存放数据的string，赋值。
@param[in] disconnect   true表示客户端已断开连接，false表示客户端未断开连接，赋值。
@return    成功读取的字节数，可能小于n。-1表示数据读取出错。
*/
ssize_t ReadData(int fd,std::string& buffer,bool& disconnect);

/*!
@brief ET模式下向文件描述符(非socket)写n个字节的数据。

@param[in] fd      文件描述符。
@param[in] source  待写数据的首地址。
@param[in] n       待写数据的字节数。
@return    成功写出的字节数，可能小于n。-1表示写数据出错。
*/
ssize_t WriteData(int fd, const char* source, size_t n);

/*!
@brief ET模式下向连接socket写入数据并删除buffer中成功写出的数据。

@param[in] fd      连接socket的文件描述符。
@param[in] buffer  待写的数据。
@param[in] full    true表示写数据失败是由于输出缓冲区已满造成的。
@return    成功写出的字节数，可能小于n。-1表示写数据出错。
*/
ssize_t WriteData(int fd, std::string& buffer, bool& full);

/*!
@brief 用于存放SubReactors的线程池。From: https://github.com/progschj/ThreadPool
*/
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

/*!
@brief 生成一个全局唯一的log对象。
*/
std::shared_ptr<spdlog::logger> GetLogger(std::string path = "../temp/log.txt");


/*!
@brief

@param[in]
@return tuple.first  端口号
@return tuple.second subreactor数量
@return tuple.third  日志文件路径
*/
std::optional<std::tuple<int,size_t ,std::string>> ParaseCommand(int argc,char* argv[]);
#endif