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

#ifndef STREAM_H
#define STREAM_H

#include <cstddef>
#include <vector>

#include "helper.h"

#include "ReadSelector.h"


/**
 * @brief a simple stream that can read, write and forward data to another stream.
 *
 * It assumes that every implementation has underlying file descriptor that can be used in a select.
 *
 * @remark This class is not thread-safe and requires external synchronization if shared between threads.
 */
class Stream
{
public:
    virtual ~Stream() {}

    /**
     * @brief Synchronously write data to the stream.
     *
     * Throws exception on failure.
     */
    virtual void send(const void *buf, std::size_t len) = 0;

    /**
     * @brief Synchronously read data from the stream.
     *
     * Throws exception on failure.
     */
    virtual void recv(void *buf, std::size_t len) = 0;

    /**
     * Read data from this stream to the buffer and write them to the output stream.
     *
     * Used when the data are needed but can be immediately forwarded without modifications.
     */
    void forward(Stream &output, void *buf, std::size_t len) {
        recv(buf, len);
        output.send(buf, len);
    }

    /**
     * Read given amount of data from this stream and write them to the output stream without saving them to any explicit buffer.
     *
     * Used when the data are not needed and only has to be forwarded without modifications.
     */
    void forward_directly(Stream &output, std::size_t len);

    /**
     * Underlying file descriptor.
     */
    virtual int fd() const = 0;

    /**
     * The underlying file descriptor is taken away from this FdStream and returned.
     * No more reading or writing is possible after calling this method.
     */
    virtual int takeFd() = 0;
};

#endif // STREAM_H
