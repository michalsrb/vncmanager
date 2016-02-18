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

#include <chrono>
#include <thread>

#include <sys/types.h>
#include <pwd.h>

#include "ControllerConnection.h"
#include "Log.h"
#include "Xvnc.h"


ControllerConnection::ControllerConnection(XvncManager &vncManager, int fd)
    : m_vncManager(vncManager)
    , m_fd(fd)
    , m_controllerStreamBuffer(boost::iostreams::file_descriptor(fd, boost::iostreams::close_handle))
    , m_controllerStream(&m_controllerStreamBuffer)
{}

void ControllerConnection::start()
{
    Log::info() << "Accepted controller " << (intptr_t)this << "." << std::endl;

    try {
        if (initialize()) {
            while (m_controllerStream.good()) {
                receive();
            }
        }
    } catch (std::exception &e) {
        Log::error() << "Exception in thread of controller " << (intptr_t)this << ": " << e.what() << std::endl;
    }

    Log::info() << "Disconnected controller " << (intptr_t)this << "." << std::endl;

    delete this;
}

bool ControllerConnection::initialize()
{
    int displayNumber;
    m_controllerStream >> displayNumber;

    m_xvnc = m_vncManager.getSessionByDisplayNumber(displayNumber);
    if (m_xvnc) {
        m_controllerStream << "OK" << std::endl;
        m_controllerStream.flush();
    } else {
        Log::notice() << "Controller " << (intptr_t)this << " asked for display number " << displayNumber << " which is not managed by vncmanager." << std::endl;
        return false;
    }

    std::string key;
    m_controllerStream >> key;

    for (int tries = 0; ; tries++) {
        if (m_xvnc->isKeyApproved(key)) {
            break;
        }

        if (tries == approvalTries) {
            Log::notice() << "Failed to approve key of controller " << (intptr_t)this << " in time." << std::endl;
            return false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // TODO: Tune
    }

    m_controllerStream << "OK" << std::endl;
    m_controllerStream.flush();

    struct ucred ucred;
    socklen_t len = sizeof(struct ucred);
    if (getsockopt(m_fd, SOL_SOCKET, SO_PEERCRED, &ucred, &len) == 0) {
        struct passwd *pwuid = getpwuid(ucred.uid);
        if (pwuid) {
            m_xvnc->setSessionUsername(pwuid->pw_name);
        }
    }

    Log::info() << "Controller " << (intptr_t)this << " approved for session #" << m_xvnc->id() << "." << std::endl;

    return true;
}

void ControllerConnection::receive()
{
    std::string cmd;
    m_controllerStream >> cmd;

    if (cmd == "VISIBLE") {
        bool yes;
        m_controllerStream >> yes;
        m_xvnc->markVisible(yes);
        return;
    }
}
