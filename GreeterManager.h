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

#ifndef GREETERMANAGER_H
#define GREETERMANAGER_H

#include <sys/types.h>

#include <atomic>
#include <map>
#include <mutex>

#include "GreeterConnection.h"
#include "XvncManager.h"


/**
 * @brief a class that manages instances of GreeterConnection
 *
 * @remark This class is thread-safe.
 *
 */
class GreeterManager
{
public:
    GreeterManager(XvncManager &xvncManager);

    GreeterManager(const GreeterManager &) = delete;
    GreeterManager &operator=(const GreeterManager &) = delete;

    /**
     * @brief Create GreeterConnection with given parameters.
     *
     * See GreeterConnection::GreeterConnection for details about the parameters.
     */
    GreeterConnection *createGreeter(std::string display, std::string xauthFilename, GreeterConnection::NewSessionHandler newSessionHandler, GreeterConnection::OpenSessionHandler openSessionHandler);

    /**
     * Announce that who was using the greeter is done with it.
     */
    void releaseGreeter(GreeterConnection *greeterConnection);

    /**
     * Notify that child process which may have been greeter died.
     */
    void childDied(pid_t pid);

    /**
     * This number increases every time the list of sessions or some of the sessions changes.
     */
    int sessionListVersion() const { return m_xvncManager.sessionListVersion(); }

    /**
     * Return copy of the session list.
     */
    const XvncManager::XvncMap sessionList() const { return m_xvncManager.sessionList(); }

private:
    mutable std::mutex m_lock;

    XvncManager &m_xvncManager;

    std::map<pid_t, GreeterConnection *> m_greeters;
};

#endif // GREETERMANAGER_H
