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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "helper.h"
#include "GreeterConnection.h"
#include "Configuration.h"
#include "Log.h"
#include "Xvnc.h"


static void private_mkdir(const std::string &path)
{
    if (mkdir(path.c_str(), S_IRWXU) < 0 && errno != EEXIST) {
        throw_errno(path);
    }
}

Xvnc::Xvnc(XvncManager &xvncManager, int id, bool queryDisplayManager)
    : m_xvncManager(xvncManager)
    , m_id(id)
{
    std::string tmpPath = Configuration::options["rundir"].as<std::string>();;
    std::string socketPath = tmpPath + "/socket";
    std::string authPath = tmpPath + "/auth";

    private_mkdir(tmpPath);

    private_mkdir(socketPath);
    m_socketFilename = std::string(socketPath) + "/" + std::to_string(m_id);

    if (m_socketFilename.size() >= sizeof(sockaddr_un::sun_path)) {
        throw std::runtime_error("Path to socket \"" + m_socketFilename + "\" is too long.");
    }

    if (!queryDisplayManager) {
        private_mkdir(authPath);
        m_xauthFilename = std::string(authPath) + "/" + std::to_string(m_id);
    }

    execute(queryDisplayManager);

    if (!queryDisplayManager) {
        generateXAuthorityFile();
    }
}

Xvnc::~Xvnc()
{
    markVisible(false);

    unlink(m_socketFilename.c_str());

    if (!m_xauthFilename.empty()) {
        unlink(m_xauthFilename.c_str());
    }
}

void Xvnc::markVisible(bool newVisible)
{
    std::lock_guard<std::mutex> guard(m_lock);

    if (m_visible == newVisible) {
        return;
    }

    m_visible = newVisible;

    m_xvncManager.notifySessionChanged();
}

void Xvnc::setDesktopName(const std::string &newDesktopName)
{
    std::lock_guard<std::mutex> guard(m_lock);

    static const char controllerKey[] = "CONTROLLER_KEY:";
    if (newDesktopName.compare(0, sizeof(controllerKey) - 1, controllerKey) == 0) {
        m_approvedControllerKeys.insert(newDesktopName.substr(sizeof(controllerKey) - 1));
        return;
    }

    if (m_desktopName == newDesktopName) {
        return;
    }

    m_desktopName = newDesktopName;

    m_xvncManager.notifySessionChanged();
}

void Xvnc::setSessionUsername(const std::string &newSessionUsername)
{
    if (m_sessionUsername == newSessionUsername) {
        return;
    }

    m_sessionUsername = newSessionUsername;

    m_xvncManager.notifySessionChanged();
}

FdStream Xvnc::connect()
{
    std::lock_guard<std::mutex> guard(m_lock);

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        throw_errno();
    }

    if (::connect(fd, (sockaddr *)(&m_endpoint), sizeof(m_endpoint)) < 0) {
        throw_errno();
    }

    m_connectionCount++;

    return FdStream(fd);
}

void Xvnc::disconnect()
{
    std::lock_guard<std::mutex> guard(m_lock);

    assert(m_connectionCount > 0);

    m_connectionCount--;
}

void Xvnc::execute(bool queryDisplayManager)
{
    if (unlink(m_socketFilename.c_str()) < 0 && errno != ENOENT) {
        throw_errno(m_socketFilename);
    }

    bzero(&m_endpoint, sizeof(m_endpoint));
    m_endpoint.sun_family = AF_UNIX;
    strcpy(m_endpoint.sun_path, m_socketFilename.c_str());

    FD fd(socket(AF_UNIX, SOCK_STREAM, 0));
    if (fd < 0) {
        throw_errno();
    }

    if (bind(fd, (sockaddr *)(&m_endpoint), sizeof(m_endpoint)) < 0) {
        throw_errno(m_socketFilename);
    }

    if (listen(fd, 100) < 0) { // TODO: Tune the number?
        throw_errno(m_socketFilename);
    }

    int displayNumberPipe[2];
    if (pipe(displayNumberPipe) < 0) {
        throw_errno();
    }

    pid_t pid = fork();
    if (pid < 0) {
        throw_errno();
    }

    if (pid == 0) {
        // Close receiving half of the displayNumberPipe
        close(displayNumberPipe[0]);

        // Forward the listening socket as stdin and stdout
        close(0);
        close(1);

        dup2(fd, 0);
        dup2(fd, 1);

        fd.close();

        // Remove all signal handlers
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigprocmask(SIG_SETMASK, &sigmask, nullptr);

        // Prepare arguments
        std::string xvncBinary = Configuration::options["xvnc"].as<std::string>();
        std::string displayNumberPipeText = std::to_string(displayNumberPipe[1]);
        std::vector<const char *> argv = {
            xvncBinary.c_str(),
            "-log", "*:syslog:30,TcpSocket:syslog:-1", // TcpSocket is unfortunatelly confused by the unix socket he gets, so silence him to prevent spam.
            "-inetd",
            "-MaxDisconnectionTime=5",
            "-securitytypes=none",
            "-displayfd", displayNumberPipeText.c_str(),
            "-geometry", Configuration::options["geometry"].as<std::string>().c_str(),
            "-AllowOverride="
                // These parameters are normally allowed by default
                "Desktop,AcceptPointerEvents,SendCutText,AcceptCutText,"
                // Additionally allow clients to configure sharing related parameters
                "MaxDisconnectionTime,MaxConnectionTime,MaxIdleTime,QueryConnect,QueryConnectTimeOut,AlwaysShared,NeverShared,DisconnectClients,"
                // Additionally allow clients to set up some security related parameters
                "SecurityTypes,Password,PlainUsers"
        };
        if (queryDisplayManager) {
            argv.push_back("-query");
            argv.push_back(Configuration::options["query"].as<std::string>().c_str());
            argv.push_back("-once");
            argv.push_back("-desktop");
            argv.push_back("New session");
        } else {
            argv.push_back("-auth");
            argv.push_back(m_xauthFilename.c_str());
            argv.push_back("-desktop");
            argv.push_back("VNC manager");
        }

        XvncArgList xvnc_args;
        if (!Configuration::options["xvnc-args"].empty()) {
            xvnc_args = Configuration::options["xvnc-args"].as<XvncArgList>();
        }
        for (auto &xvnc_arg : xvnc_args.values) {
            argv.push_back(xvnc_arg.c_str());
        }

        argv.push_back(nullptr);

        // Execute
        if (execve(xvncBinary.c_str(), const_cast<char *const *>(argv.data()), 0) < 0) { // Note: const_cast is ok here, the execve's argv parameter could be const, but it isn't for compability reasons.
            exit(1);
        }
    } else {
        // Close the sending half of the displayNumberPipe
        close(displayNumberPipe[1]);

        // Close the listening socket
        fd.close();

        // Remember the pid
        m_pid = pid;

        // Wait for the display number
        m_displayNumber = 0;
        while (true) {
            char buf;
            ssize_t len = read(displayNumberPipe[0], &buf, 1);
            if (len == 0) {
                throw std::runtime_error("Xvnc did not report display number correctly.");
            }
            if (len < 0) {
                throw_errno();
            }

            if (buf >= '0' && buf <= '9') {
                m_displayNumber = m_displayNumber * 10 + (int)(buf - '0');
            }

            if (buf == '\n') {
                break;
            }
        }
        close(displayNumberPipe[0]);
        m_display = std::string(":") + std::to_string(m_displayNumber);
    }

    Log::info() << "Spawned Xvnc (id: #" << id() << ", pid: " << m_pid << ", display: " << m_displayNumber << ")" << std::endl;
}


void Xvnc::generateXAuthorityFile()
{
    // generate cookie
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);

    // resever 32 bytes
    m_xauthCookie.reserve(32);

    // create a random hexadecimal number
    const char *digits = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        m_xauthCookie.push_back(digits[dis(gen)]);
    }

    // prepare empty file with right permissions (so xauth doesn't complain)
    int fd = open(m_xauthFilename.c_str(), O_RDWR | O_CREAT, S_IRWXU | S_IRWXG);
    if (fd < 0) {
        throw_errno(m_xauthFilename);
    }
    close(fd);

    // execute xauth
    std::string cmd = Configuration::options["xauth"].as<std::string>() + " -f " + m_xauthFilename + " -q";
    FILE *fp = popen(cmd.c_str(), "w");

    // check file
    if (!fp) {
        throw_errno(cmd);
    }

    fprintf(fp, "remove %s\n", m_display.c_str());
    fprintf(fp, "add %s . %s\n", m_display.c_str(), m_xauthCookie.c_str());
    fprintf(fp, "exit\n");

    // close pipe
    pclose(fp);
}

bool Xvnc::isKeyApproved(std::string key)
{
    std::lock_guard<std::mutex> guard(m_lock);

    return m_approvedControllerKeys.find(key) != m_approvedControllerKeys.end();
}
