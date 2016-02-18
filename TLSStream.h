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

#ifndef TLSSTREAM_H
#define TLSSTREAM_H

#include <gnutls/gnutls.h>

#include "FdStream.h"
#include "Stream.h"


/**
 * @brief implementation of Stream with TLS encryption.
 *
 * @remark This class is not thread-safe and requires external synchronization if shared between threads.
 */
class TLSStream : public Stream
{
public:
    class GnuTlsException : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;

        GnuTlsException(const std::string &what, int gnutls_err)
            : std::runtime_error(what + ": " + gnutls_strerror(gnutls_err)) {}
    };

public:
    /**
     * @brief Create new TLSStream using given file descriptor.
     *
     * TLSStream becomes owner of the file descriptor.
     *
     * @param fd Opened file descriptor that can be read from and written to.
     * @param anonymous True for anonymous TLS, false for TLS with certificate.
     */
    TLSStream(int fd, bool anonymous);

    TLSStream(TLSStream &) = delete;
    FdStream &operator=(FdStream &) = delete;

    virtual ~TLSStream();

    /**
     * Configure TLS and do handshake.
     */
    void initialize();

    virtual void recv(void *buf, std::size_t len);

    virtual void send(const void *buf, std::size_t len);

    virtual int fd() const { return m_fd; }
    virtual int takeFd();

private:
    int m_fd;
    bool m_anonymous;

    struct {
        gnutls_session_t session = nullptr;
        gnutls_dh_params_t dh_params = nullptr;
        gnutls_anon_server_credentials_t anon_cred = nullptr;
        gnutls_certificate_credentials_t cert_cred = nullptr;
    } m_tls;
};

#endif // TLSSTREAM_H
