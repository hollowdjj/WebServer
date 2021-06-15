#include "HttpServer.h"

HttpServer::HttpServer(int port,std::shared_ptr<EventLoop> main_reactor)
            : listenfd_(BindAndListen(port)),main_reactor_(main_reactor)
{
    assert(listenfd_ != -1);
    port_ = port;
    listen_channel_ = std::make_shared<Channel>(listenfd_,true);
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
    auto sub_reactor_num = sub_thread_pool->size();
    for (decltype(sub_reactor_num) i = 0; i < sub_reactor_num; ++i)
    {
        sub_reactors_.emplace_back(std::make_shared<EventLoop>());
        sub_thread_pool->AddTaskToPool([this,i](){this->sub_reactors_[i]->StartLoop();});
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

    //需要新建一个处理HTTP数据的类。然后在里面绑定相应的回调函数
    /*Http server的连接sokcet需要监听可读、可写、断开连接以及错误事件*/
    connfd_channel->SetEvents(EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLERR);
    
    /*这里还要考虑一下如何分发连接socket*/

}

void HttpServer::ErrorHandler()
{
    std::cout<<"get an error form listen socket: "<<errno<<std::endl;
}