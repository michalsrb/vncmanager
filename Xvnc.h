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

#ifndef XVNC_H
#define XVNC_H

#include <unistd.h>

#include <mutex>
#include <set>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>

#include "FdStream.h"
#include "XvncManager.h"


/**
 * @brief a class that takes care of one running Xvnc program.
 *
 * This class starts and quits the Xvnc program and provides means to connect to it.
 *
 * @remark This class is thread safe.
 */
class Xvnc
{
public:
    /**
     * @brief Start new Xvnc process and construct instance of Xvnc class.
     *
     * @param xvncManager Reference to XvncManager.
     * @param id Internal identifier of this Xvnc.
     * @param queryDisplayManager Whether the Xvnc should query display manager using XDMCP.
     */
    Xvnc(XvncManager &xvncManager, int id, bool queryDisplayManager);

    Xvnc(const Xvnc &) = delete;
    Xvnc &operator=(const Xvnc &) = delete;

    ~Xvnc();

    /**
     * PID of the Xvnc process.
     */
    pid_t pid() const { return m_pid; }

    /**
     * Internal id.
     */
    int id() const { return m_id; }

    /**
     * Whether is this Xvnc visible in the list of sessions.
     */
    bool visible() const { return m_visible; }

    void markVisible(bool newVisible);

    std::string desktopName() const { return m_desktopName; }
    void setDesktopName(const std::string &newDesktopName);

    std::string sessionUsername() const { return m_sessionUsername; }
    void setSessionUsername(const std::string &newSessionUsername);

    /**
     * X display number.
     */
    int displayNumber() const { return m_displayNumber; }

    /**
     * X display string.
     */
    std::string display() const { return m_display; }

    /**
     * Path to file containing xauth cookie for this Xvnc.
     */
    std::string xauthFilename() const { return m_xauthFilename; }

    /**
     * Returns FdStream connected to the Xvnc's VNC port.
     */
    FdStream connect();

    /**
     * Notify that VNC client disconnected.
     */
    void disconnect();

    /**
     * Returns whether given controller key was approved for controlling this session.
     */
    bool isKeyApproved(std::string key);

private:
    void execute(bool queryDisplayManager);
    void generateXAuthorityFile();

private:
    std::mutex m_lock;

    XvncManager &m_xvncManager;

    int m_id;

    pid_t m_pid;

    std::string m_socketFilename;
    sockaddr_un m_endpoint;

    int m_displayNumber;
    std::string m_display;
    std::string m_xauthFilename;
    std::string m_xauthCookie;

    int m_connectionCount = 0;

    bool m_visible = false;
    std::string m_desktopName;
    std::string m_sessionUsername;

    std::set<std::string> m_approvedControllerKeys;
};

#endif // XVNC_H
