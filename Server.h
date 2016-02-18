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

#ifndef SERVER_H
#define SERVER_H

#include <netdb.h>

#include <string>
#include <vector>

#include "helper.h"
#include "ControllerManager.h"
#include "GreeterManager.h"
#include "XvncManager.h"


/**
 * @brief a main server class.
 *
 * It listen on TCP port for VNC connections and creates VncTunnel instances.
 * It handles signals.
 *
 * @remark This class is not thread-safe and is intended to be used by main thread.
 *
 */
class Server
{
private:
    static constexpr int LISTEN_QUEUE = 32;

public:
    Server();

    Server(const Server &) = delete;
    Server &operator=(const Server &) = delete;

    ~Server();

    /**
     * Start accepting incomming connections and picking-up signals.
     * This method returns when a terminating signal is received.
     */
    void run();

private:
    void listen(addrinfo *results);
    void listen(const std::vector<std::string> &addresses, const std::string &port);
    void accept(int listenfd);

    void prepareSignals();
    void handleSignal();

private:
    XvncManager m_vncManager;
    GreeterManager m_greeterManager;
    ControllerManager m_controlManager;

    bool m_run;

    int m_sigfd;

    std::vector<int> m_listenfds;
};

#endif // SERVER_H
