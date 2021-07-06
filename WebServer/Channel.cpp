#include "Channel.h"

Channel::Channel(int fd, bool is_listenfd, bool is_connfd)
                : fd_(fd),
                  is_listenfd_(is_listenfd),
                  is_connfd_(is_connfd)
{
    /*不能同时为监听socket和连接socket*/
    if(is_listenfd==is_connfd && is_connfd)
    {
        assert(-1);
    }
}

Channel::~Channel()
{
    close(fd_);
}

void Channel::CallReventsHandlers()
{
    if(is_listenfd_ && (revents_ & EPOLLIN))  CallConnHandler();
    else if(revents_ & EPOLLERR)
    {
        CallErrorHandler();
        return;
    }
    else if(revents_ & EPOLLOUT) CallWriteHandler();
    else if(revents_ & (EPOLLIN | EPOLLRDHUP))  CallReadHandler();
}