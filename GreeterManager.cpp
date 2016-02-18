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

#include "GreeterManager.h"
#include "GreeterConnection.h"


GreeterManager::GreeterManager(XvncManager &xvncManager)
    : m_xvncManager(xvncManager)
{
}

GreeterConnection *GreeterManager::createGreeter(std::string display, std::string xauthFilename, GreeterConnection::NewSessionHandler newSessionHandler, GreeterConnection::OpenSessionHandler openSessionHandler)
{
    std::lock_guard<std::mutex> guard(m_lock);

    GreeterConnection *newGreeter = new GreeterConnection(*this, display, xauthFilename, newSessionHandler, openSessionHandler);
    m_greeters.insert(std::make_pair(newGreeter->greeterPID(), newGreeter));
    return newGreeter;
}

void GreeterManager::releaseGreeter(GreeterConnection *greeterConnection)
{
    std::lock_guard<std::mutex> guard(m_lock);

    m_greeters.erase(greeterConnection->greeterPID());

    delete greeterConnection;
}

void GreeterManager::childDied(pid_t pid)
{
    std::lock_guard<std::mutex> guard(m_lock);

    auto iter = m_greeters.find(pid);
    if (iter == m_greeters.end()) {
        return;
    }

    iter->second->markDead();
    m_greeters.erase(iter);
}
