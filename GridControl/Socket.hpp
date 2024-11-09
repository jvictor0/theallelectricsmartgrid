#pragma once
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/poll.h>
#include <vector>
#include <stdexcept>
#include <string>

struct Socket
{
    static constexpr size_t x_bufferSize = 4096;
    int m_fd;
    uint8_t m_buffer[x_bufferSize];
    size_t m_bufferHead;
    size_t m_bufferTail;
    std::vector<uint8_t> m_writeBuffer;

    Socket()
        : m_fd(-1)
        , m_bufferHead(0)
        , m_bufferTail(0)
    {
    }
    
    Socket(int fd)
        : m_fd(fd)
        , m_bufferHead(0)
        , m_bufferTail(0)
    {
        Setup();
    }

    ~Socket()
    {
        Close();
    }

    void Setup()
    {
        int flags = fcntl(m_fd, F_GETFL, 0);
        if (flags == -1 || fcntl(m_fd, F_SETFL, flags | O_NONBLOCK))
        {
            int error = errno;
            throw std::runtime_error("fcntl failed " + std::string(strerror(error)));
        }
    }

    void Close()
    {
        if (m_fd != -1)
        {
            close(m_fd);
            m_fd = -1;
        }
    }

    bool IsOpen() const
    {
        return m_fd != -1;
    }
    
    void Connect(const char* host, uint16_t port)
    {
        Close();
        m_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (m_fd == -1)
        {
            int error = errno;
            throw std::runtime_error("socket failed " + std::string(strerror(error)));
        }

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

        if (inet_pton(AF_INET, host, &addr.sin_addr) != 1)
        {
            int error = errno;
            throw std::runtime_error("inet_pton failed " + std::string(strerror(error)));
        }

        if (connect(m_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
        {
            int error = errno;
            throw std::runtime_error("connect failed " + std::string(strerror(error)));
        }

        Setup();
    }
    
    void CycleBuffer()
    {
        if (m_bufferHead == m_bufferTail)
        {
            m_bufferHead = 0;
            m_bufferTail = 0;
        }
        else
        {
            size_t size = m_bufferTail - m_bufferHead;
            memmove(m_buffer, m_buffer + m_bufferHead, size);
            m_bufferHead = 0;
            m_bufferTail = size;
        }
    }

    size_t BufferSize() const
    {
        return m_bufferTail - m_bufferHead;
    }

    size_t SpaceInBuffer() const
    {
        return x_bufferSize - m_bufferTail;
    }

    size_t Read(uint8_t* buffer, size_t size, bool block)
    {
        bool did = false;
        size_t totalBytesRead = 0;
        while (size > 0)
        {
            if (BufferSize() > 0)
            {
                size_t copySize = std::min(size, BufferSize());
                memcpy(buffer, m_buffer + m_bufferHead, copySize);
                m_bufferHead += copySize;
                buffer += copySize;
                size -= copySize;
                totalBytesRead += copySize;
            }
            else
            {
                if (!block && did)
                {
                    return totalBytesRead;
                }
                
                CycleBuffer();
                ssize_t bytesRead = read(m_fd, m_buffer + m_bufferTail, SpaceInBuffer());
                if (bytesRead <= 0)
                {
                    int error = errno;
                    if (error == EAGAIN || error == EWOULDBLOCK)
                    {
                        if (!block)
                        {
                            return totalBytesRead;
                        }

                        Poll(true);
                        continue;
                    }
                    else
                    {
                        throw std::runtime_error("read failed " + std::string(strerror(error)));
                    }
                }

                m_bufferTail += bytesRead;
                did = true;
            }
        }

        return totalBytesRead;
    }
                
    void Poll(bool read)
    {
        pollfd p = { m_fd, 0, 0 };
        
        if (read)
        {
            p.events |= POLLIN;
        }
        else
        {
            p.events |= POLLOUT;
        }

        int readyFds = poll(&p, 1, -1);
        if (readyFds == -1)
        {
            int error = errno;
            throw std::runtime_error("poll failed " + std::string(strerror(error)));
        }
    }

    void Write(const uint8_t* buffer, size_t size)
    {
        m_writeBuffer.insert(m_writeBuffer.end(), buffer, buffer + size);
    }

    void Flush()
    {
        size_t totalBytesWritten = 0;
        while (m_writeBuffer.size() > totalBytesWritten)
        {
            ssize_t bytesWritten = write(m_fd, m_writeBuffer.data() + totalBytesWritten, m_writeBuffer.size() - totalBytesWritten);
            if (bytesWritten <= 0)
            {
                int error = errno;
                if (error == EAGAIN || error == EWOULDBLOCK)
                {
                    Poll(false);
                    continue;
                }
                else
                {
                    throw std::runtime_error("write failed " + std::string(strerror(error)));
                }
            }

            totalBytesWritten += bytesWritten;
        }

        m_writeBuffer.clear();
    }
};
