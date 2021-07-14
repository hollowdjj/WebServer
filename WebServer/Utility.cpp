#include "Utility.h"

///////////////////////////
//   Global    Variables //
///////////////////////////
std::chrono::seconds GlobalVar::slot_interval = std::chrono::seconds(1); /* NOLINT */
std::chrono::seconds GlobalVar::timer_timeout = std::chrono::seconds(5); /* NOLINT */
int GlobalVar::slot_num = 60;
const int kMaxBufferSize = 4096;

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
    sockaddr_in server_addr{};
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

ssize_t ReadData(int fd, char* dest, size_t n)
{
    char* pos = dest;       //本次存放读取的数据的首地址
    size_t num = n;         //本次期望读取num个字节的数据
    ssize_t read_sum = 0;   //一共读取的字节数
    ssize_t read_once = 0;  //本次read函数调用成功读取的字节数
    while(num > 0)
    {
        read_once = read(fd, pos, num);
        if(read_once < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) return read_sum;  //缓冲区中的所有数据已读出，返回
            else if(errno == EINTR) read_once = 0;                        //非阻塞read被系统中断打断，read_once为0
            else return -1;                                               //否则表示发生了错误
        }
        else if(read_once == 0) break;                                    //读取完毕，退出循环
        /*更新*/
        pos +=read_once;
        num -= read_once;
        read_sum+= read_once;
    }

    return read_sum;
}

ssize_t ReadData(int fd,std::string& buffer,bool& disconnect)
{
    ssize_t read_once = 0;   //本次读取的字节数
    ssize_t read_sum = 0;    //读取的总字节数
    while(true)
    {
        char temp[kMaxBufferSize];
        memset(temp,'\0',4096);
        read_once = recv(fd,temp,kMaxBufferSize,0);
        if(read_once < 0)
        {
            if(errno == EINTR) continue;                                      //被系统中断就再重新读一次
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                /*!
                  ET模式下，连接socket的可读事件就绪，表明缓冲区中有数据可读。所以，不用考虑
                  那种由于一开始就没有数据可读从而返回EWOULDBLOCK的情况。当错误代码为EAGAIN
                  或EWOULDBLOCK时，我们就认为数据已经读取完毕。
                 */
                return read_sum;
            }
            printf("error on reading data from connfd: %s", strerror(errno)); //否则数据读取出错
            return -1;
        }
        else if(read_once == 0)
        {
             disconnect = true;             //一般情况下，recv返回0是由于客户端关闭连接导致的。
             break;
        }
        read_sum +=read_once;
        buffer += std::string(std::begin(temp),std::begin(temp)+read_once);  //拼接数据
    }

    return read_sum;
}

ssize_t WriteData(int fd, const char* source, size_t n)
{
    const char* pos = source;      //本次要写入的数据的首地址
    size_t num = n;                //本次期望写入num个字节的数据
    ssize_t write_sum = 0;         //一共写入的字节数
    ssize_t write_once = 0;        //本次write函数调用成功读取的字节数
    while(num > 0)
    {
        write_once = write(fd,pos,num);
        if(write_once < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) return write_sum;    //缓冲区已经写满了，返回
            else if(errno == EINTR) write_once = 0;                          //被系统中断打断
            else return -1;                                                  //否则出错
        }
        pos+=write_once;
        num-=write_once;
        write_sum+=write_once;
    }
    return write_sum;
}

ssize_t WriteData(int fd,std::string& buffer)
{
    auto num = buffer.size();      //期望写出的字节数
    ssize_t write_once = 0;        //本次写出的字节数
    ssize_t write_sum = 0;         //写出的总字节数
    const char* ptr = buffer.c_str();
    
    while(num > 0)
    {
        write_once = send(fd, ptr, num, 0);
        if(write_once < 0)
        {
            if(errno == EAGAIN || errno==EWOULDBLOCK)  return write_sum;  //缓冲区已经写满了，返回
            else if(errno == EINTR)
            {
                write_once = 0;
                continue;                                                 //被系统中断打断时重新再写一次
            }
            return -1;                                                    //否则写数据出错
        }
        num-=write_once;
        write_sum+=write_once;
        ptr+=write_once;
    }
    /*从buffer中删除已经写出的数据*/
    if(write_sum == buffer.size()) buffer.clear();
    else buffer = buffer.substr(write_sum);
    
    return write_sum;
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