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

HttpServer::~HttpServer()
{
    /*离开析构函数的函数体后，其余未被主动释放的成员变量才会被自动释放*/

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
        auto sub_reactor = std::make_shared<EventLoop>();
        sub_reactors_.emplace_back(std::make_pair(sub_reactor,0));
        sub_thread_pool_.AddTaskToPool([=](){sub_reactor->StartLoop();});
    }
}

void HttpServer::Quit()
{
    main_reactor_->Quit();
    for (auto& item : sub_reactors_)
    {
        item.first->Quit();
    }
}

void HttpServer::NewConnHandler()
{
    sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof client_addr;
    int connfd = accept(listenfd_,reinterpret_cast<sockaddr*>(&client_addr),&client_addr_len);
    if(connfd < 0)
    {
        printf("accept error: %s\n", strerror(errno));
        return;
    }
    /*限制服务器的最大并发连接数*/
    if(current_user_num >= kMaxUserNum)
    {
        printf("max user number limit\n");
        close(connfd);
        return;
    }
    ++current_user_num;
    SetNonBlocking(connfd);
    /*将连接socket分发给SubReactor*/
    auto connfd_channel = std::make_shared<Channel>(connfd,false);
    /*!
        Http server的连接sokcet需要监听可读、可写、断开连接以及错误事件
        但是需要注意的是，不要一开始就注册可写事件。因为只要connfd只要不是阻塞的它就是可写的
        因此，需要在完整读取了客户端的数据之后再注册可写事件，否则会一直触发可写事件
     */
    connfd_channel->SetEvents(EPOLLIN | EPOLLRDHUP | EPOLLERR);

    /*将连接socket分发给事件最少的SubReactor*/
    auto target_pair = sub_reactors_[0];
    int index = 0;
    for (int i = 0; i < sub_reactors_.size(); ++i)
    {
        printf("subreactor %d is handling %d connections\n",i,sub_reactors_[i].second);
        if(sub_reactors_[i].second < target_pair.second)
        {
            target_pair = sub_reactors_[i];
            index = i;
        }
    }
    auto s = new HttpData(target_pair.first,connfd_channel);
    if(target_pair.first->AddToEventChannelPool(connfd_channel))
    {
        printf("new connection established through socket %d and handled by subreactor %d\n",connfd,index);
        ++sub_reactors_[index].second;
    }
}

void HttpServer::ErrorHandler()
{
    printf("get an error form listen socket: %s", strerror(errno));
}