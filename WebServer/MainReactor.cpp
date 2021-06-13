#include "MainReactor.h"

MainReactor::MainReactor(int port) : listenfd_(BindAndListen(port))
{
    assert(listenfd_ != -1);
    port_ = port;
    listen_channel_ = std::make_shared<Channel>(listenfd_,true);
    SetNonBlocking(listenfd_);
}

void MainReactor::Start()
{
    /*对监听socket监听可读以及异常事件*/
    listen_channel_->SetEvents(EPOLLIN | EPOLLERR);
    listen_channel_->SetConnHandler([this] { NewConnHandler(); });
    listen_channel_->SetErrorHandler([this]{ ErrorHandler(); });
    /*将listen_channel加入事件循环中*/
}

void MainReactor::NewConnHandler()
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
    SetNonBlocking(connfd);
    /*将连接socket分发给SubReactor*/

}