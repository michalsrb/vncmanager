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

#include <sys/select.h>

#include "helper.h"
#include "ReadSelector.h"
#include "Stream.h"


void ReadSelector::clear()
{
    m_fds.clear();
}

void ReadSelector::addFD(int fd, ReadSelector::Handler handler)
{
    [[gnu::unused]] auto iter = m_fds.insert(std::make_pair(fd, handler));

    assert(iter.second); // It would fail to insert if it was already tehre. Calling this method with fd that was already in is illegal.
}

void ReadSelector::addStream(Stream &stream, ReadSelector::Handler handler)
{
    addFD(stream.fd(), handler);
}

void ReadSelector::select()
{
    m_pendingCancelation = false;

    fd_set set;
    FD_ZERO(&set);

    int maxfd = -1;
    for (auto iter : m_fds) {
        FD_SET(iter.first, &set);
        if (iter.first > maxfd) {
            maxfd = iter.first;
        }
    }

    if (::select(maxfd + 1, &set, nullptr, nullptr, nullptr) < 0) {
        throw_errno();
    }

    for (auto iter : m_fds) {
        if (FD_ISSET(iter.first, &set)) {
            iter.second();
            if (m_pendingCancelation) {
                return;
            }
        }
    }
}

void ReadSelector::cancel()
{
    m_pendingCancelation = true;
}
