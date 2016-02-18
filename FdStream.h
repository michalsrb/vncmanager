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

#ifndef FDSTREAM_H
#define FDSTREAM_H

#include "Stream.h"


/**
 * @brief implementation of Stream that reads and writes to/from file descriptor.
 *
 * The class takes ownership of the file descriptor and closes it when destroyed unless it's moved to another instance or taken away with takeFd method.
 *
 * @remark This class is not thread-safe and requires external synchronization if shared between threads.
 */
class FdStream : public Stream
{
public:
    /**
     * Create FdStream without associated file descriptor.
     */
    FdStream();

    /**
     * @brief Create FdStream from given file descriptor.
     *
     * @param fd Opened file descriptor that can be read from and written to.
     */
    FdStream(int fd);

    FdStream(FdStream &) = delete;

    /**
     * Move underlying file descriptor from one instance to another.
     */
    FdStream(FdStream && another);

    virtual ~FdStream();

    FdStream &operator=(FdStream &) = delete;

    virtual void send(const void *buf, std::size_t len);

    virtual void recv(void *buf, std::size_t len);

    virtual int fd() const { return m_fd; }

    virtual int takeFd();

private:
    int m_fd = -1;
};

#endif // FDSTREAM_H
