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

#include <netinet/in.h>
#include <stdio.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "Configuration.h"
#include "Log.h"
#include "ReadSelector.h"
#include "Server.h"
#include "VncTunnel.h"


Server::Server()
    : m_controlManager(m_vncManager)
    , m_greeterManager(m_vncManager)
{
    prepareSignals();

    std::vector<std::string> addresses;
    if (Configuration::options["listen"].empty()) {
        addresses.push_back(std::string());
    } else {
        addresses = Configuration::options["listen"].as<std::vector<std::string>>();
    }

    std::string port = Configuration::options["port"].as<std::string>();
    listen(addresses, port);
}

Server::~Server()
{
    for (int fd : m_listenfds) {
        close(fd);
    }
}

void Server::run()
{
    m_run = true;

    ReadSelector selector;
    for (int fd : m_listenfds) {
        selector.addFD(fd, std::bind(&Server::accept, this, fd));
    }
    selector.addFD(m_sigfd, std::bind(&Server::handleSignal, this));

    m_controlManager.prepareSelect(selector);

    while (m_run) {
        selector.select();
    }
}

void Server::listen(addrinfo *results)
{
    for (addrinfo *result = results; result; result = result->ai_next) {
        char address_text[64];
        strcpy(address_text, "UNKNOWN");
        if (result->ai_family == AF_INET) {
            inet_ntop(result->ai_family, &((struct sockaddr_in *)result->ai_addr)->sin_addr, address_text, sizeof(address_text));
        } else if (result->ai_family == AF_INET6) {
            inet_ntop(result->ai_family, &((struct sockaddr_in6 *)result->ai_addr)->sin6_addr, address_text, sizeof(address_text));
        }

        Log::debug() << "Starting to listen on address " << address_text << std::endl;

        int fd = socket(result->ai_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            Log::notice() << "Failed to create socket: " << strerror(errno) << std::endl;
            continue;
        }

        if (result->ai_family == AF_INET6) {
            int yes = 1;
            if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes)) < 0) {
                Log::notice() << "Failed setsockopt on socket: " << strerror(errno) << std::endl;
            }
        }

        if (bind(fd, result->ai_addr, result->ai_addrlen) < 0) {
            Log::notice() << "Failed bind on " << address_text << " address: " << strerror(errno) << std::endl;
            close(fd);
            continue;
        }

        if (::listen(fd, LISTEN_QUEUE) < 0)  {
            Log::notice() << "Failed listen: " << strerror(errno) << std::endl;
            close(fd);
            continue;
        }

        m_listenfds.push_back(fd);
    }
}

void Server::listen(const std::vector<std::string> &addresses, const std::string &port)
{
    addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG | AI_NUMERICSERV | AI_PASSIVE;

    for (const std::string & address : addresses) {
        addrinfo *results;
        int err = getaddrinfo(address.empty() ? nullptr : address.c_str(), port.c_str(), &hints, &results);
        if (err != 0) {
            Log::notice() << "Failed getaddrinfo on address \"" << address << "\": " << gai_strerror(err) << std::endl;
            continue;
        }

        listen(results);
        freeaddrinfo(results);
    }

    if (m_listenfds.size() == 0) {
        throw std::runtime_error("Could not bind to any address.");
    }
}

void Server::accept(int listenfd)
{
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);
    int fd = accept4(listenfd, (struct sockaddr *)&cliaddr, &clilen, SOCK_CLOEXEC);
    if (fd < 0) {
        throw_errno();
    }

    VncTunnel *tunnel = new VncTunnel(m_vncManager, m_greeterManager, m_controlManager, fd);
    std::thread(&VncTunnel::start, tunnel).detach();
}

void Server::prepareSignals()
{
    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGTERM);
    sigaddset(&sigmask, SIGPIPE);
    sigaddset(&sigmask, SIGCHLD);

    if (sigprocmask(SIG_BLOCK, &sigmask, nullptr) < 0) {
        throw_errno();
    }

    m_sigfd = signalfd(-1, &sigmask, SFD_CLOEXEC | SFD_NONBLOCK);
    if (m_sigfd < 0) {
        throw_errno();
    }
}

void Server::handleSignal()
{
    struct signalfd_siginfo siginfo;
    ssize_t ret = read(m_sigfd, &siginfo, sizeof(siginfo));
    if (ret != sizeof(siginfo)) {
        throw_errno();
    }

    switch (siginfo.ssi_signo) {
    case SIGINT:
    case SIGTERM:
        m_run = false;
        break;

    case SIGCHLD:
        // Clean any waiting child processes.
        while (pid_t pid = waitpid(-1, NULL, WNOHANG)) {
            if (pid == 0 || (pid < 0 && errno == ECHILD)) { // No (more) children to take care of.
                break;
            }

            if (pid < 0) {
                throw_errno();
            }

            m_vncManager.childDied(pid);
            m_greeterManager.childDied(pid);
        }
        break;

    case SIGPIPE:
        // Ignoring SIGPIPEs of greeters.
        // TODO: Any other SIGPIPEs we could potentially get?
        break;
    }
}
