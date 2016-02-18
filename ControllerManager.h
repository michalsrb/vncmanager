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

#ifndef CONTROLMANAGER_H
#define CONTROLMANAGER_H

#include <string>
#include <map>
#include <memory>

#include "helper.h"
#include "ControllerConnection.h"
#include "ReadSelector.h"
#include "XvncManager.h"


/**
 * @brief a class that takes care of control socket.
 *
 * This class creates control socket for communication with controller programs.
 * It accepts incomming connections and spawns ControllerConnection for them.
 *
 * @remark This class is not thread-safe and is intended to be used by Server's thread.
 */
class ControllerManager
{
public:
    ControllerManager(XvncManager &vncManager);

    ControllerManager(const ControllerManager &) = delete;
    ControllerManager &operator=(const ControllerManager &) = delete;

    ~ControllerManager();

    /**
     * Register handlers into given ReadSelector
     */
    void prepareSelect(ReadSelector &readSelector);

private:
    void accept();

private:
    XvncManager &m_vncManager;

    std::string m_endpointFilename;
    FD m_fd;
};

#endif // CONTROLMANAGER_H
