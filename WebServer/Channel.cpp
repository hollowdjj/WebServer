#include "Channel.h"


Channel::Channel(int fd,bool is_listenfd /*false*/) : fd_(fd),is_listenfd_(is_listenfd) {}

Channel::~Channel() {}

void Channel::CallReventsHandlers()
{
    if(is_listenfd_ && (revents_ & EPOLLIN))  CallConnHandler();
    else if(revents_ & EPOLLERR)
    {
        CallErrorHandler();
        return;
    }
    else if(revents_ & EPOLLOUT) CallWriteHandler();
    else if(revents_ & EPOLLIN)  CallReadHandler();
    else if(revents_ & EPOLLRDHUP) CallDisconnHandler();
}