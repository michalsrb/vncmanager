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

#ifndef XVNCMANAGER_H
#define XVNCMANAGER_H

#include <sys/types.h>

#include <map>
#include <memory>
#include <mutex>


class Xvnc;

/**
 * @brief class that manages instances of Xvnc
 *
 * @remark This class is thread-safe.
 */
class XvncManager
{
public:
    typedef std::map<int, std::shared_ptr<Xvnc>> XvncMap;

public:
    XvncManager() = default;

    XvncManager(const XvncManager &) = delete;
    XvncManager &operator=(const XvncManager &) = delete;

    /**
     * @brief Create new Xvnc session.
     *
     * @param queryDisplayManager Whether it should query display manager.
     */
    std::shared_ptr<Xvnc> createSession(bool queryDisplayManager);

    std::shared_ptr<Xvnc> getSession(int id);
    std::shared_ptr<Xvnc> getSessionByDisplayNumber(int displayNumber);

    /**
     * Return copy of the session list.
     */
    const XvncMap sessionList() const;

    /**
     * This number increases every time the list of sessions or some of the sessions changes.
     */
    int sessionListVersion() const { return m_sessionListVersion; }

    /**
     * Whether there are any session with visibility = true.
     */
    bool hasVisibleSessions() const;

    /**
     * Notify that some session changed. (E.g. session name changed.)
     */
    void notifySessionChanged();

    /**
     * Notify that child process which may have been Xvnc died.
     */
    void childDied(pid_t pid);

private:
    mutable std::recursive_mutex m_lock;

    XvncMap m_xvncs;

    int m_nextId = 0;

    int m_sessionListVersion = 0;
};

#endif // XVNCMANAGER_H
