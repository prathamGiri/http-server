#pragma once

class Socket{
private:
    int fd;
public:
    Socket();
    ~Socket();
    int getFD() const;
};