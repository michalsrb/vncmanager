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

#ifndef HELPER_H
#define HELPER_H

#include <unistd.h>

#include <system_error>

#include <gnutls/gnutls.h>


/**
 * Helper function to throw std::system_error with current errno.
 */
inline void throw_errno()
{
    throw std::system_error(errno, std::system_category());
}

/**
 * Helper function to throw std::system_error with current errno and extra message
 */
inline void throw_errno(const char *what_arg)
{
    throw std::system_error(errno, std::system_category(), what_arg);
}

/**
 * Helper function to throw std::system_error with current errno and extra message
 */
inline void throw_errno(const std::string &what_arg)
{
    throw std::system_error(errno, std::system_category(), what_arg);
}

/**
 * EOF for Stream.
 */
class eof_exception : std::exception {}; // TODO: Move/change?

/**
 * Helper class that can be used to assure closing of file descriptor on scope exit.
 */
class FD
{
public:
    FD() {}
    FD(int fd ) : m_fd(fd ) {}
    FD(const FD & ) = delete;
    ~FD() {
        if(m_fd != -1 ) {
            ::close(m_fd);
        }
    }

    FD &operator=(const FD & ) = delete;
    FD &operator=(int fd ) {
        if(m_fd != -1 ) {
            ::close(m_fd);
        }
        m_fd = fd;

        return *this;
    }

    operator int() const {
        return m_fd;
    }

    void close() {
        ::close(m_fd);
        m_fd = -1;
    }

private:
    int m_fd = -1;
};

/**
 * Helper to do gnutls init/deinit RAII style.
 */
class GnuTlsInstance {
public:
    GnuTlsInstance() {
        gnutls_global_init();
    }

    ~GnuTlsInstance() {
        gnutls_global_deinit();
    }
};

#endif // HELPER_H
