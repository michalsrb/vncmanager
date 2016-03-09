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

#include <algorithm>
#include <cassert>

#include "Xvnc.h"
#include "XvncManager.h"


std::shared_ptr<Xvnc> XvncManager::createSession(bool queryDisplayManager)
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    auto ptr = std::make_shared<Xvnc>(*this, m_nextId++, queryDisplayManager);
    [[gnu::unused]] auto result = m_xvncs.insert(std::make_pair(ptr->id(), ptr));
    assert(result.second); // New Xvnc object was created, it must be unique in the set, unless there is something very wrong.

    return ptr;
}

std::shared_ptr<Xvnc> XvncManager::getSession(int id)
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    auto iter = m_xvncs.find(id);
    if (iter == m_xvncs.end()) {
        return std::shared_ptr<Xvnc>();
    }

    return iter->second;
}

std::shared_ptr< Xvnc > XvncManager::getSessionByDisplayNumber(int displayNumber)
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    for (auto iter : m_xvncs) {
        if (iter.second->displayNumber() == displayNumber) {
            return iter.second;
        }
    }

    return std::shared_ptr<Xvnc>();
}

const XvncManager::XvncMap XvncManager::sessionList() const
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    return m_xvncs;
}

bool XvncManager::hasVisibleSessions() const
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    return std::any_of(m_xvncs.begin(), m_xvncs.end(), [](const XvncManager::XvncMap::value_type & pair) {
        return pair.second->visible();
    });
}

void XvncManager::notifySessionChanged()
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    m_sessionListVersion++;
}

void XvncManager::childDied(pid_t pid)
{
    std::lock_guard<std::recursive_mutex> guard(m_lock);

    for (auto iter = m_xvncs.begin(); iter != m_xvncs.end(); ++iter) {
        if (iter->second->pid() == pid) {
            m_xvncs.erase(iter);
            break;
        }
    }
}
