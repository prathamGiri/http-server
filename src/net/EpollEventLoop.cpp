#include "net/EpollEventLoop.hpp"

#include <iostream>

std::uint32_t EpollEventLoop::toEpollFlags(IOEventMode mode){
    switch (mode)
    {
        case IOEventMode::Read: return EPOLLIN;
        case IOEventMode::Write: return EPOLLOUT;
        case IOEventMode::ReadWrite: return EPOLLIN | EPOLLOUT;
    }
    return EPOLLIN;
}

int EpollEventLoop::init(){
    this->epoll_fd = epoll_create1(0);
    if (this->epoll_fd == -1)
    {
        std::cerr << "Failed to create epoll\n";
        return 1;
    }
    return 0;
}

int EpollEventLoop::registerFD(int fd, IOEventMode mode){
    epoll_event serverEvent{};
    serverEvent.events = toEpollFlags(mode);
    serverEvent.data.fd = fd;

    if (epoll_ctl(
            this->epoll_fd,
            EPOLL_CTL_ADD,
            fd,
            &serverEvent
        ) == -1)
    {
        std::cerr << "Failed to add fd to epoll\n";
        return 1;
    }
    return 0;
}

void EpollEventLoop::removeFD(int fd){
    epoll_ctl(
        this->epoll_fd,
        EPOLL_CTL_DEL,
        fd,
        nullptr
    );
}

void EpollEventLoop::changeFdMode(int fd, IOEventMode mode){
    epoll_event event{};
    event.events = toEpollFlags(mode);
    event.data.fd = fd;
    epoll_ctl(
            this->epoll_fd,
            EPOLL_CTL_MOD,
            fd,
            &event
        );
}

int EpollEventLoop::getEventCount(){
    int event_count = epoll_wait(
        this->epoll_fd,
        this->events,
        10,
        -1
    );

    if (event_count == -1)
    {
        std::cerr << "epoll_wait failed\n";
        return -1;
    }
    return event_count;
}

epoll_event EpollEventLoop::getEvents(int i){
    return this->events[i];
}