/*
 * Copyright (c) 2016 Michal Srb <michalsrb@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <assert.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "FdStream.h"


FdStream::FdStream()
    : m_fd(-1)
{}

FdStream::FdStream(int fd)
    : m_fd(fd)
{}

FdStream::FdStream(FdStream && another)
{
    if (m_fd != -1) {
        close(m_fd);
    }

    m_fd = another.m_fd;
    another.m_fd = -1;
}

FdStream::~FdStream()
{
    if (m_fd != -1) {
        close(m_fd);
    }
}

void FdStream::recv(void *buf, std::size_t len)
{
    assert(m_fd != -1); // Using the stream before it was given FD or after it was taken is not allowed.

    char *ptr = (char *)buf;
    while (len > 0) {
        ssize_t ret = ::recv(m_fd, ptr, len, 0);
        if (ret == 0) {
            throw eof_exception();
        }
        if (ret < 0) {
            throw_errno();
        }

        ptr += ret;
        len -= ret;
    }
}

void FdStream::send(const void *buf, std::size_t len)
{
    assert(m_fd != -1); // Using the stream before it was given FD or after it was taken is not allowed.

    const char *ptr = (const char *)buf;
    while (len > 0) {
        ssize_t ret = ::send(m_fd, ptr, len, 0);
        if (ret <= 0) {
            throw_errno();
        }

        ptr += ret;
        len -= ret;
    }
}

int FdStream::takeFd()
{
    int fd = m_fd;
    m_fd = -1;
    return fd;
}
