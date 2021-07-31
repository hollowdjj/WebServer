#include "Channel.h"
#include "HttpServer.h"
#include "HttpData.h"
#include "Utility.h"
#include "EventLoop.h"

HttpServer::HttpServer(int port, EventLoop* main_reactor,ThreadPool* sub_thread_pool)
            : listenfd_(BindAndListen(port)), 
              p_main_reactor_(main_reactor), 
              p_sub_thread_pool_(sub_thread_pool),
              p_listen_channel_(new Channel(listenfd_, true, false))
{
    assert(listenfd_ != -1);
    port_ = port;
    SetNonBlocking(listenfd_);
}

HttpServer::~HttpServer()
{
    /*!
        离开析构函数的函数体后，其余未被主动释放的成员变量才会被自动释放。
        listen_channel_的生命周期交由main_reactor_管理
        main_reactor_的生命周期由main函数管理
        sub_thread_pool_的生命周期同样由main函数管理
        HttpServer只管理SubReactor的生命周期，这里使用了shared_ptr来自动释放资源
     */
}

void HttpServer::Start()
{
    /*对监听socket监听可读以及异常事件*/
    p_listen_channel_->SetEvents(EPOLLIN | EPOLLERR);
    p_listen_channel_->SetConnHandler([this] { NewConnHandler(); });
    p_listen_channel_->SetErrorHandler([this]{ ErrorHandler(); });

    /*将listen_channel加入MainReactor中进行监听*/
    p_main_reactor_->AddEpollEvent(p_listen_channel_);

    /*构造SubReactor并开启事件循环*/
    auto sub_reactor_num = p_sub_thread_pool_->size();
    for (decltype(sub_reactor_num) i = 0; i < sub_reactor_num; ++i)
    {
        /*HttpServer和ThreadPool需要共享SubReactor对象，故这里使用shared_ptr*/
        auto sub_reactor = std::make_shared<EventLoop>();
        tickfds_.emplace_back(sub_reactor->timewheel_.tick_fd_[1]);     //获取SubReactor的tickfd的写端文件描述符
        p_sub_thread_pool_->AddTaskToPool([=](){sub_reactor->StartLoop();});
        sub_reactors_.emplace_back(sub_reactor);
    }
}

void HttpServer::Quit()
{
    p_main_reactor_->QuitLoop();
    for (auto& sub_reactors : sub_reactors_)
    {
        sub_reactors->QuitLoop();
    }
}

void HttpServer::NewConnHandler()
{
    /*从监听队列中接受一个连接*/
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof client_addr;
    int connfd = accept(listenfd_,reinterpret_cast<sockaddr*>(&client_addr),&client_addr_len);
    if(connfd < 0)
    {
        ::GetLogger()->error("accept error: {}", strerror(errno));
        return;
    }
    //限制服务器的最大并发连接数
    if(GlobalVar::GetTotalUserNum() >= GlobalVar::kMaxUserNum)
    {
        ::GetLogger()->warn("max user number limit");
        close(connfd);
        return;
    }
    GlobalVar::IncTotalUserNum();
    SetNonBlocking(connfd);
    
    /*将连接socket分发给SubReactor*/
    auto connfd_channel = new Channel(connfd, false,true);
    /*!
        Http server的连接sokcet需要监听可读、可写、断开连接以及错误事件。
        但是需要注意的是，不要一开始就注册可写事件，因为只要connfd只要不是阻塞的它就是可写的。
        因此，需要在完整读取了客户端的数据之后再注册可写事件，否则会一直触发可写事件。
        这里connfd_channel的生命周期交由SubReactor管理。
     */
    connfd_channel->SetEvents(EPOLLIN | EPOLLRDHUP | EPOLLERR);

    //将连接socket分发给事件最少的SubReactor
    int smallest_num = sub_reactors_[0]->GetConnectionNum();
    unsigned long index = 0;
    std::vector<int> num_of_each;
    for (unsigned long i = 0; i < sub_reactors_.size(); ++i)
    {
        int num =  sub_reactors_[i]->GetConnectionNum();
        num_of_each.push_back(num);
        if( num < smallest_num)
        {
            smallest_num = num;
            index = i;
        }
    }
    //必须先设置Holder再将该连接socket加入到事件池中
    connfd_channel->SetHolder(new HttpData(sub_reactors_[index].get(),connfd_channel));
    if(sub_reactors_[index]->AddEpollEvent(connfd_channel))
    {
        ::GetLogger()->debug("new connection {} handled by subreactor {}",connfd,index);
        ++num_of_each[index];
    }

    /*打印当前每个SubReactor的连接数量*/
    for (int i = 0; i < num_of_each.size(); ++i)
    {
        ::GetLogger()->debug("Subreactor {} is handling {} connections",i,num_of_each[i]);
    }

    ::GetLogger()->info("New connection {}, current user number: {}",connfd,GlobalVar::GetTotalUserNum());
}

void HttpServer::ErrorHandler()
{
    ::GetLogger()->critical("Get an error from listen socket: {}", strerror(errno));
}