#include "Channel.h"
#include "Utility.h"

Channel::Channel(int fd, bool is_connfd)
                : fd_(fd),
                  is_connfd_(is_connfd)
{

}

Channel::~Channel()
{
    close(fd_);
}

void Channel::CallReadHandler()
{
    if(read_handler_) read_handler_();
    else ::GetLogger()->warn("read handler has not been registered yet");
}

void Channel::CallWriteHandler()
{
    if(write_handler_) write_handler_();
    else ::GetLogger()->warn("write handler has not been registered yet");
}

void Channel::CallErrorHandler()
{
    if(error_handler_) error_handler_();
    else ::GetLogger()->warn("error handler has not been registered yet");
}

void Channel::CallDisconnHandler()
{
    if(disconn_handler_) disconn_handler_();
    else ::GetLogger()->warn("disconnect handler has not been registered yet");
}

void Channel::CallReventsHandlers()
{
    if(revents_ & EPOLLERR)
    {
        CallErrorHandler();
        return;
    }
    else if(revents_ & EPOLLOUT)
        CallWriteHandler();
    else if(revents_ & EPOLLRDHUP)
        CallDisconnHandler();
    else if(revents_ & EPOLLIN)
        CallReadHandler();
}

bool Channel::EqualAndUpdateLastEvents()
{
    bool ret = (events_ == last_events_);
    last_events_ = events_;
    return ret;
}