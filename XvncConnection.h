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

#ifndef XVNCCONNECTION_H
#define XVNCCONNECTION_H

#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "rfb.h"

#include "FdStream.h"
#include "StreamFormatter.h"
#include "Xvnc.h"


/**
 * @brief a class that takes care of one VNC connection to a Xvnc process.
 *
 * This class acts as VNC client takes care of initializing VNC session with a Xvnc. After the connection is initialized, it lets others talk directly to the Xvnc.
 *
 * @remark This class is not thread-safe and is intended to be used in VncTunnel's thread.
 */
class XvncConnection
{
public:
    typedef std::function<void(void)> ConnectionInitializedHandler;
    typedef std::function<void(std::string)> PasswordHandler;
    typedef std::function<void(PasswordHandler)> PasswordRequestHandler;
    typedef std::function<void(std::string, std::string)> CredentialsHandler;
    typedef std::function<void(CredentialsHandler)> CredentialsRequestHandler;

    class ConnectionException : public std::runtime_error
    {
    public:
        ConnectionException(const XvncConnection *faultyConnection, std::string message)
            : std::runtime_error(message), m_faultyConnection(faultyConnection)
        {}

        const XvncConnection *faultyConnection() const { return m_faultyConnection; }

    private:
        const XvncConnection *m_faultyConnection;
    };

public:
    XvncConnection(std::shared_ptr<Xvnc> xvnc);

    XvncConnection(const XvncConnection &) = delete;
    XvncConnection &operator=(const XvncConnection &) = delete;

    ~XvncConnection();

    /**
     * Initialize VNC connection while expecting None security type.
     * The connection is initialized when the method returns.
     */
    void initialize();

    /**
     * Initialize VNC connection with Xvnc that may ask for password or credentials.
     *
     * @param connectionInitializedHandler Handler that will be called when the connection is initialized.
     * @param passwordRequestHandler Handler that will be called if password is needed.
     * @param credentialsRequestHandler Handler that will be called if credentials are needed.
     */
    void initialize(ConnectionInitializedHandler connectionInitializedHandler, PasswordRequestHandler passwordRequestHandler, CredentialsRequestHandler credentialsRequestHandler);

    void sendSetPixelFormat(const PixelFormat &pixelFormat);
    void sendSetEncodings(const std::vector<EncodingType> &encodings);
    void sendNonIncrementalFramebufferUpdateRequest();

    /**
     * Stream used to communicate with the VNC server.
     */
    Stream &stream() { return m_stream; }

    /**
     * Stream format on top of the stream used to communicate with the VNC server.
     */
    StreamFormatter &fmt() { return m_streamFormatter; }

    uint16_t framebufferWidth() const { return m_framebufferWidth; }
    uint16_t framebufferHeight() const { return m_framebufferHeight; }
    void setFramebufferSize(uint16_t width, uint16_t height);

    std::string desktopName() const { return m_xvnc->desktopName(); }
    void setDesktopName(const std::string &desktopName);
    PixelFormat pixelFormat() const { return m_pixelFormat; }

private:
    SecurityType startInitialization(std::set<SecurityType> supportedTypes);
    void handleNoneSecurity();
    void handleVncAuthSecurity();
    void handleVncAuthSecurityWithPassword(std::string password);
    void handleVeNCryptSecurity();
    void handleVeNCryptSecurityWithCredentials(std::string username, std::string password);
    void receiveSecurityResult();
    void completeInitialization();

    std::string receiveFailureReason();

private:
    std::shared_ptr<Xvnc> m_xvnc;

    FdStream m_stream;
    StreamFormatter m_streamFormatter;

    uint16_t m_framebufferWidth, m_framebufferHeight;
    PixelFormat m_pixelFormat;

    ConnectionInitializedHandler m_connectionInitializedHandler;
    PasswordRequestHandler m_passwordRequestHandler;
    CredentialsRequestHandler m_credentialsRequestHandler;
};

#endif // XVNCCONNECTION_H
