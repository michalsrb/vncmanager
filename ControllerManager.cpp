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

#include <string.h>

#include <thread>

#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "helper.h"
#include "Configuration.h"
#include "ControllerConnection.h"
#include "ControllerManager.h"


ControllerManager::ControllerManager(XvncManager &vncManager)
    : m_vncManager(vncManager)
{
    std::string tmpPath = Configuration::options["rundir"].as<std::string>();
    std::string controlPath = tmpPath + "/control";

    if (mkdir(controlPath.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0 && errno != EEXIST) {
        throw_errno(controlPath);
    }

    m_endpointFilename = controlPath + "/control";

    if (m_endpointFilename.size() >= sizeof(sockaddr_un::sun_path)) {
        throw std::runtime_error("Path to socket \"" + m_endpointFilename + "\" is too long.");
    }

    if (unlink(m_endpointFilename.c_str()) < 0 && errno != ENOENT) {
        throw_errno(m_endpointFilename);
    }

    sockaddr_un endpoint;
    bzero(&endpoint, sizeof(endpoint));
    endpoint.sun_family = AF_UNIX;
    strcpy(endpoint.sun_path, m_endpointFilename.c_str());

    m_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (m_fd < 0) {
        throw_errno();
    }

    if (bind(m_fd, (sockaddr *)(&endpoint), sizeof(endpoint)) < 0) {
        throw_errno(m_endpointFilename);
    }

    if (chmod(m_endpointFilename.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) < 0) {
        throw_errno(m_endpointFilename);
    }

    if (listen(m_fd, 100) < 0) { // TODO: Tune the number?
        throw_errno(m_endpointFilename);
    }
}

ControllerManager::~ControllerManager()
{
    unlink(m_endpointFilename.c_str());
}

void ControllerManager::prepareSelect(ReadSelector &readSelector)
{
    readSelector.addFD(m_fd, std::bind(&ControllerManager::accept, this));
}

void ControllerManager::accept()
{
    struct sockaddr_un cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int fd = accept4(m_fd, (struct sockaddr *)&cliaddr, &clilen, SOCK_CLOEXEC);
    if (fd < 0) {
        throw_errno();
    }

    ControllerConnection *controllerConnection = new ControllerConnection(m_vncManager, fd);
    std::thread(&ControllerConnection::start, controllerConnection).detach();
}
