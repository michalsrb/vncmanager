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

#ifndef INCOMINGCLIENT_H
#define INCOMINGCLIENT_H

#include <cstdlib>
#include <iostream>
#include <set>
#include <utility>
#include <vector>

#include "rfb.h"
#include "ControllerConnection.h"
#include "ControllerManager.h"
#include "GreeterConnection.h"
#include "GreeterManager.h"
#include "ReadSelector.h"
#include "Stream.h"
#include "StreamFormatter.h"
#include "XvncConnection.h"
#include "XvncManager.h"


/**
 * @brief a class that takes care of single VNC client.
 *
 * This class acts as VNC proxy that forwards VNC messages between its client and associated XvncConnection. It can switch the client from current to a new XvncConnection.
 * It also handles communication with greeter using GreeterConnection if one is displayed in current session.
 *
 * This class is meant to live in its own thread started by start() method. The thread quits and this class gets deleted when the client disconnects.
 *
 * @remark This class must be allocated with new operator. It owns itself and deletes itself at the end of start function.
 * @remark This class is not thread safe and is meant to be used inside its own thread.
 */
class VncTunnel
{
public:
    /**
     * @brief Construct new VncTunnel instance using given managers and accepted file descriptor.
     *
     * VncTunnel becomes the owner of the file descriptor.
     *
     * @param xvncManager Reference to XvncManager
     * @param greeterManager Reference to GreeterManager
     * @param controllerManager ControllerManager
     * @param fd Accepted file descriptor with VNC client on the other side.
     */
    VncTunnel(XvncManager &xvncManager, GreeterManager &greeterManager, ControllerManager &controllerManager, int fd);

    VncTunnel(const VncTunnel &) = delete;
    VncTunnel &operator=(const VncTunnel &) = delete;

    ~VncTunnel();

    /**
     * @brief Start communicating with the VNC client
     *
     * This method is intended to be the starting function of new thread. The instance of this class is deleted before this method returns.
     */
    void start();

private:
    Stream &cStream() { return *m_stream; }
    Stream &sStream() { return m_currentConnection->stream(); }
    StreamFormatter &cFmt() { return m_streamFormatter; }
    StreamFormatter &sFmt() { return m_currentConnection->fmt(); }

    void clientInitalize();
    void handleNoneSecurity();
    void handleVeNCryptSecurity();
    void finishClientInitialization();

    void select();
    void clientReceive();
    void serverReceive();

    void processSetPixelFormat();
    void processSetEncodings();
    void processFramebufferUpdateRequest();
    void processKeyEvent();
    void processPointerEvent();
    void processClientCutText();
    void processSetDesktopSize();

    void processFramebufferUpdate();
    void processSetColourMapEntries();
    void processBell();
    void processServerCutText();

    void sendReason(std::string reason);

    /**
     * This is last-resort option used when VncTunnel needs to discard framebuffer update rectangle.
     */
    void sendDummyRectangle();

    void sendLastRectangle();

    void newSessionHandler();
    void openSessionHandler(int id);

    void switchToConnection(std::shared_ptr<Xvnc> xvnc);
    void connectionSwitched();

    int countExtraRectangles();
    void sendExtraRectangles();

    std::vector<SecurityType> configuredSecurityTypes();

    bool clientSupportsEncoding(EncodingType encoding);

private:
    XvncManager &m_xvncManager;
    GreeterManager &m_greeterManager;
    ControllerManager &m_controllerManager;

    Stream *m_stream;
    StreamFormatter m_streamFormatter;

    ReadSelector m_selector;

    XvncConnection *m_currentConnection = nullptr;
    XvncConnection *m_potentialConnection = nullptr;
    GreeterConnection *m_greeterConnection = nullptr;

    SecurityType m_securityType = SecurityType::Invalid;
    PixelFormat m_pixelFormat;

    // List of encodings that both our client and we support.
    std::set<EncodingType> m_supportedEncodingsClient;

    // List of encodings that we report to the server.
    std::vector<EncodingType> m_supportedEncodingsServer; // Note: vector, not set. The order is client's priority. There is always just few elements.

    bool m_tightZlibResetQueued = false;
    bool m_desktopNameChangeQueued = false;

    // Some VNC clients do not handle reset of zlib streams in tight encoding correctly. To minimize the problems, disable tight encoding if we know that we'll be switching to another Xvnc soon.
    bool m_tightEncodingDisabled = false;
};

#endif // INCOMINGCLIENT_H
