#pragma once

#include <sys/epoll.h>
#include <string>
#include <cstdint>
#include <vector>

enum class IOEventMode {Read, Write, ReadWrite};
struct IOEvent
{
    int fd;
    bool readable;
    bool writable;
};

class EpollEventLoop
{
private:
    int epoll_fd;
    int maxEvents_;
    std::vector<epoll_event> events;
    std::uint32_t toEpollFlags(IOEventMode mode);

public:
    explicit EpollEventLoop(int maxEvents);
    ~EpollEventLoop() = default;
    int init();
    int registerFD(int fd, IOEventMode mode);
    void changeFdMode(int fd, IOEventMode mode);
    void removeFD(int fd);
    int getEventCount();
    std::vector<IOEvent> getEvents();
};
