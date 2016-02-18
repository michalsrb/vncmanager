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

#ifndef CONTROLLERCONNECTION_H
#define CONTROLLERCONNECTION_H

#include <memory>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

#include "Xvnc.h"
#include "XvncManager.h"


/**
 * @brief a class that takes care of communicating with a single controller program.
 *
 * It is meant to live in its own thread which is started by start() method.
 *
 * @remark This class must be allocated with new operator. It owns itself and deletes itself at the end of start function.
 * @remark This class is not thread safe and is meant to be used inside its own thread.
 */
class ControllerConnection
{
private:
    static const int approvalTries = 100; // TODO: Tune

public:
    /**
     * @brief Construct new ControllerConnection from accepted connection.
     *
     * @param vncManager Reference to XvncManager
     * @param fd File descriptor of accepted connection from a controller program.
     */
    ControllerConnection(XvncManager &vncManager, int fd);

    ControllerConnection(const ControllerConnection &) = delete;
    ControllerConnection &operator=(const ControllerConnection &) = delete;

    /**
     * @brief Start communicating with the controller program.
     *
     * This method is intended to be the starting function of new thread. The instance of this class is deleted before this method returns.
     */
    void start();

private:
    bool initialize();
    void receive();

private:
    XvncManager &m_vncManager;

    std::shared_ptr<Xvnc> m_xvnc;

    int m_fd;
    boost::iostreams::stream_buffer<boost::iostreams::file_descriptor> m_controllerStreamBuffer;
    std::iostream m_controllerStream;
};

#endif // CONTROLLERCONNECTION_H
