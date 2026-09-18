#pragma once

#include <sys/epoll.h>
#include <string>
#include <cstdint>

enum class IOEventMode {Read, Write, ReadWrite};

class EpollEventLoop
{
private:
    int epoll_fd;
    epoll_event events[10];
    std::uint32_t toEpollFlags(IOEventMode mode);

public:
    int init();
    int registerFD(int fd, IOEventMode mode);
    void changeFdMode(int fd, IOEventMode mode);
    void removeFD(int fd);
    int getEventCount();
    epoll_event getEvents(int i);
};
