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

#include <memory>

#include "StreamFormatter.h"


StreamFormatter::StreamFormatter(Stream *stream)
    : m_stream(stream)
{}

void StreamFormatter::send_raw(const void *buf, std::size_t len)
{
    m_stream->send(buf, len);
}

void StreamFormatter::recv_raw(void *buf, std::size_t len)
{
    if (bufferSize > 0) {
        std::size_t buffered = std::min(len, bufferSize);
        memcpy(buf, buffer, buffered);
        bufferSize -= buffered;
        len -= buffered;
        buf = (uint8_t *)buf + buffered;
    }

    if (len > 0) {
        m_stream->recv(buf, len);
    }
}

std::string StreamFormatter::recv_string(std::size_t length)
{
    std::unique_ptr<char[]> buffer(new char[length]);
    recv_raw(buffer.get(), length);
    return std::string(buffer.get(), buffer.get() + length);
}

void StreamFormatter::forward_raw(Stream &output, void *buf, std::size_t len)
{
    if (bufferSize > 0) {
        recv_raw(buf, len);
        output.send(buf, len);
    } else {
        m_stream->forward(output, buf, len);
    }
}

void StreamFormatter::forward_directly(Stream &output, std::size_t len)
{
    if (bufferSize > 0) {
        std::size_t buffered = std::min(len, bufferSize);
        output.send(buffer, std::min(len, bufferSize));
        bufferSize -= buffered;
        len -= buffered;
    }

    if (len > 0) {
        m_stream->forward_directly(output, len);
    }
}
