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

#include <string>

#include "Configuration.h"
#include "TLSStream.h"


TLSStream::TLSStream(int fd, bool anonymous)
    : m_fd(fd), m_anonymous(anonymous)
{}

TLSStream::~TLSStream()
{
    if (m_tls.session) {
        gnutls_bye(m_tls.session, GNUTLS_SHUT_WR);
    }

    if (m_tls.dh_params) {
        gnutls_dh_params_deinit(m_tls.dh_params);
    }

    if (m_tls.anon_cred) {
        gnutls_anon_free_server_credentials(m_tls.anon_cred);
    }

    if (m_tls.cert_cred) {
        gnutls_certificate_free_credentials(m_tls.cert_cred);
    }

    if (m_tls.session) {
        gnutls_deinit(m_tls.session);
    }

    close(m_fd);
}

void TLSStream::initialize()
{
    int err;
    if ((err = gnutls_init(&m_tls.session, GNUTLS_SERVER)) != GNUTLS_E_SUCCESS) {
        throw GnuTlsException("gnutls_init", err);
    }

    if ((err = gnutls_set_default_priority(m_tls.session)) != GNUTLS_E_SUCCESS) {
        throw GnuTlsException("gnutls_set_default_priority", err);
    }

    const char *priority;
    if (m_anonymous) {
        priority = Configuration::options["tls-priority-anonymous"].as<std::string>().c_str();
    } else {
        priority = Configuration::options["tls-priority-certificate"].as<std::string>().c_str();
    }

    const char *err_pos;
    if ((err = gnutls_priority_set_direct(m_tls.session, priority, &err_pos)) != GNUTLS_E_SUCCESS) {
        if (err == GNUTLS_E_INVALID_REQUEST) {
            throw GnuTlsException(std::string("Invalid priority syntax. Error at: ") + err_pos);
        }
        throw GnuTlsException("gnutls_priority_set_direct", err);
    }

    if ((err = gnutls_dh_params_init(&m_tls.dh_params)) != GNUTLS_E_SUCCESS) {
        throw GnuTlsException("gnutls_dh_params_init", err);
    }

    unsigned int dh_bits = gnutls_sec_param_to_pk_bits(GNUTLS_PK_DH, GNUTLS_SEC_PARAM_NORMAL);
    if ((err = gnutls_dh_params_generate2(m_tls.dh_params, dh_bits)) != GNUTLS_E_SUCCESS) {
        throw GnuTlsException("gnutls_dh_params_generate2", err);
    }

    if (m_anonymous) {
        if ((err = gnutls_anon_allocate_server_credentials(&m_tls.anon_cred)) != GNUTLS_E_SUCCESS) {
            throw GnuTlsException("gnutls_anon_allocate_server_credentials", err);
        }

        gnutls_anon_set_server_dh_params(m_tls.anon_cred, m_tls.dh_params);

        if ((err = gnutls_credentials_set(m_tls.session, GNUTLS_CRD_ANON, m_tls.anon_cred)) != GNUTLS_E_SUCCESS) {
            throw GnuTlsException("gnutls_credentials_set", err);
        }

    } else {
        if ((err = gnutls_certificate_allocate_credentials(&m_tls.cert_cred)) != GNUTLS_E_SUCCESS) {
            throw GnuTlsException("gnutls_certificate_allocate_credentials", err);
        }

        gnutls_certificate_set_dh_params(m_tls.cert_cred, m_tls.dh_params);

        const char *certfile = Configuration::options["tls-cert"].as<std::string>().c_str();
        const char *keyfile = Configuration::options["tls-key"].as<std::string>().c_str();
        if ((err = gnutls_certificate_set_x509_key_file(m_tls.cert_cred, certfile, keyfile, GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS) {
            throw GnuTlsException("gnutls_certificate_set_x509_key_file", err);
        }

        if ((err = gnutls_credentials_set(m_tls.session, GNUTLS_CRD_CERTIFICATE, m_tls.cert_cred)) != GNUTLS_E_SUCCESS) {
            throw GnuTlsException("gnutls_credentials_set", err);
        }
    }

    gnutls_transport_set_int(m_tls.session, fd());

    while (true) {
        int err = gnutls_handshake(m_tls.session);
        if (err != GNUTLS_E_SUCCESS) {
            if (gnutls_error_is_fatal(err)) {
                throw GnuTlsException("gnutls_handshake", err);
            }
            continue;
        }
        break;
    }
}

void TLSStream::recv(void *buf, std::size_t len)
{
    char *ptr = (char *)buf;
    while (len > 0) {
        ssize_t ret = gnutls_record_recv(m_tls.session, ptr, len);
        if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) {
            continue;
        }
        if (ret == 0) { // TODO: Is this really EOF here?
            throw eof_exception();
        }
        if (ret == GNUTLS_E_PREMATURE_TERMINATION) {
            throw eof_exception();
        }
        if (ret < 0) {
            throw GnuTlsException("gnutls_record_recv", ret);
        }

        ptr += ret;
        len -= ret;
    }
}

void TLSStream::send(const void *buf, std::size_t len)
{
    const char *ptr = (const char *)buf;
    while (len > 0) {
        ssize_t ret = gnutls_record_send(m_tls.session, ptr, len);
        if (ret == GNUTLS_E_INTERRUPTED || ret == GNUTLS_E_AGAIN) {
            continue;
        }
        if (ret < 0) {
            throw GnuTlsException("gnutls_record_send", ret);
        }

        ptr += ret;
        len -= ret;
    }
}

int TLSStream::takeFd()
{
    assert(!"Not supported."); // Taking fd from TLS stream shouldn't be needed, so it is not supported.
    return -1;
}
