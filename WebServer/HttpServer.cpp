#include "HttpServer.h"

HttpServer::HttpServer(int port,std::shared_ptr<EventLoop> main_reactor
        ,ThreadPool& sub_thread_pool)
            : listenfd_(BindAndListen(port)),main_reactor_(main_reactor),sub_thread_pool_(sub_thread_pool)
            ,listen_channel_(std::make_shared<Channel>(listenfd_,true))
{
    assert(listenfd_ != -1);
    port_ = port;
    SetNonBlocking(listenfd_);
}

void HttpServer::Start()
{
    /*对监听socket监听可读以及异常事件*/
    listen_channel_->SetEvents(EPOLLIN | EPOLLERR);
    listen_channel_->SetConnHandler([this] { NewConnHandler(); });
    listen_channel_->SetErrorHandler([this]{ ErrorHandler(); });

    /*将listen_channel加入事件循环*/
    main_reactor_->AddToEventChannelPool(listen_channel_);

    /*构造SubReactor并开启事件循环*/
    auto sub_reactor_num = sub_thread_pool_.size();
    for (decltype(sub_reactor_num) i = 0; i < sub_reactor_num; ++i)
    {
        sub_reactors_.emplace_back(std::make_shared<EventLoop>());
        sub_thread_pool_.AddTaskToPool([this,i](){this->sub_reactors_[i]->StartLoop();});
    }
}

void HttpServer::NewConnHandler()
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof client_addr;
    int connfd = accept(listenfd_,reinterpret_cast<sockaddr*>(&client_addr),&client_addr_len);
    if(connfd < 0)
    {
        std::cout<<"accept error: "<<errno<<std::endl;
        return;
    }
    /*限制服务器的最大并发连接数*/
    if(current_user_num >= kMaxUserNum)
    {
        std::cout<<"max user number limit"<<std::endl;
        close(connfd);
        return;
    }
    ++current_user_num;
    SetNonBlocking(connfd);
    /*将连接socket分发给SubReactor*/
    auto connfd_channel = std::make_shared<Channel>(connfd,false);

    /*Http server的连接sokcet需要监听可读、可写、断开连接以及错误事件*/
    connfd_channel->SetEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR);

    /*将连接socket分发给事件最少的SubReactor*/
    //main_reactor_->AddToEventChannelPool(connfd_channel);
    //auto s = new HttpData(main_reactor_,connfd_channel);
    auto target_sub_reactor = sub_reactors_[0];
    for (int i = 1; i < sub_reactors_.size(); ++i)
    {
        if(sub_reactors_[i]->EventSize() < target_sub_reactor->EventSize())
            target_sub_reactor = sub_reactors_[i];
    }
    auto s = new HttpData(target_sub_reactor,connfd_channel);
    target_sub_reactor->AddToEventChannelPool(connfd_channel);
}

void HttpServer::ErrorHandler()
{
    std::cout<<"get an error form listen socket: "<<errno<<std::endl;
}