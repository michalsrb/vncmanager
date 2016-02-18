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

#ifndef READSELECTOR_H
#define READSELECTOR_H

#include <functional>
#include <map>


class Stream;


/**
 * @brief helper class for select
 *
 * @remark This class is not thread-safe and requires external synchronization if shared between threads.
 */
class ReadSelector
{
public:
    typedef std::function<void(void)> Handler;

public:
    /**
     * Forget all file descriptors and their handles.
     */
    void clear();

    /**
     * Add file descriptor and its read handler.
     */
    void addFD(int fd, Handler handler);

    /**
     * Add stream instance and its read handler.
     */
    void addStream(Stream &stream, Handler handler);

    /**
     * Do the select.
     * This will block until at least one file descriptor is ready for reading.
     * Handlers will be called from this method for all read-ready file descriptors unless canceled by cancel() method.
     */
    void select();

    /**
     * Do not call any other handlers.
     * This method is used from a handler does something that invalidates something other handlers may depend on.
     */
    void cancel();

private:
    std::map<int, Handler> m_fds;

    bool m_pendingCancelation;
};

#endif // READSELECTOR_H
