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

#include <signal.h>
#include <stdio.h>
#include <unistd.h>

#include <vector>

#include "helper.h"

#include "Configuration.h"
#include "GreeterConnection.h"
#include "GreeterManager.h"
#include "Log.h"
#include "Xvnc.h"


GreeterConnection::GreeterConnection(GreeterManager &greeterManager, std::string display, std::string xauthFilename, NewSessionHandler newSessionHandler, OpenSessionHandler openSessionHandler)
    : m_greeterManager(greeterManager)
    , m_in(&m_greeterOut)
    , m_out(&m_greeterIn)
    , m_newSessionHandler(newSessionHandler)
    , m_openSessionHandler(openSessionHandler)
{
    int greeterStdinPipe[2];
    if (pipe(greeterStdinPipe) < 0) {
        throw_errno();
    }

    int greeterStdoutPipe[2];
    if (pipe(greeterStdoutPipe) < 0) {
        throw_errno();
    }

    pid_t pid = fork();
    if (pid < 0) {
        throw_errno();
    }

    if (pid == 0) {
        // Set the pipes to stdin and stdout
        close(greeterStdinPipe[1]);
        close(greeterStdoutPipe[0]);

        close(0);
        close(1);

        dup2(greeterStdinPipe[0], 0);
        dup2(greeterStdoutPipe[1], 1);

        // Remove all signal handlers
        sigset_t sigmask;
        sigemptyset(&sigmask);
        sigprocmask(SIG_SETMASK, &sigmask, nullptr);

        // Prepare environment
        std::string envDisplay = "DISPLAY=" + display;
        std::string envXauthority = "XAUTHORITY=" + xauthFilename;
        const char *const envp[] = { envDisplay.c_str(), envXauthority.c_str(), nullptr };

        // Execute
        std::string greeterBinary = Configuration::options["greeter"].as<std::string>();
        if (execve(greeterBinary.c_str(), nullptr, const_cast<char *const *>(envp)) < 0) { // Note: const_cast is ok here, the execve's envp parameter could be const, but it isn't for compability reasons.
            exit(1);
        }
    }

    m_greeterPID = pid;

    close(greeterStdinPipe[0]);
    close(greeterStdoutPipe[1]);

    m_greeterStdin = greeterStdinPipe[1];
    m_greeterStdout = greeterStdoutPipe[0];

    m_greeterIn.open(boost::iostreams::file_descriptor_sink(m_greeterStdin, boost::iostreams::never_close_handle));
    m_greeterOut.open(boost::iostreams::file_descriptor_source(m_greeterStdout, boost::iostreams::never_close_handle));

    Log::debug() << "Spawned greeter (pid: " << m_greeterPID << ", display: " << display << ")" << std::endl;
}

GreeterConnection::~GreeterConnection()
{
    Log::debug() << "Terminating greeter " << (m_dead ? "(already dead)" : "") << "(pid: " << m_greeterPID << ")" << std::endl;

    if (!m_dead) {
        kill(m_greeterPID, SIGTERM);
    }

    close(m_greeterStdin);
    close(m_greeterStdout);
}

void GreeterConnection::update()
{
    if (m_dead) {
        throw std::runtime_error("Greeter died unexpectedly.");
    }

    int currentSessionListVersion = m_greeterManager.sessionListVersion();
    if (m_lastSentSessionListVersion < currentSessionListVersion) {
        m_lastSentSessionListVersion = currentSessionListVersion;
        sendSessions();
    }
}

void GreeterConnection::prepareSelect(ReadSelector &readSelector)
{
    readSelector.addFD(m_greeterStdout, std::bind(&GreeterConnection::receive, this));
}

void GreeterConnection::askForPassword(GreeterConnection::PasswordHandler passwordHandler)
{
    m_passwordHandler = passwordHandler;

    m_out << "GET PASSWORD\n";
    m_out.flush();
}

void GreeterConnection::askForCredentials(GreeterConnection::CredentialsHandler credentialsHandler)
{
    m_credentialsHandler = credentialsHandler;

    m_out << "GET CREDENTIALS\n";
    m_out.flush();
}

void GreeterConnection::showError(std::string error)
{
    m_out << "ERROR\n";
    m_out << error;
    m_out << "\nEND ERROR\n";
    m_out.flush();
}

void GreeterConnection::sendSessions()
{
    auto sessions = m_greeterManager.sessionList();

    int count = std::count_if(sessions.begin(), sessions.end(), [](const XvncManager::XvncMap::value_type & pair) {
        return pair.second->visible();
    });

    m_out << "SESSIONS\n" << count << "\n";

    for (auto iter : sessions) {
        if (iter.second->visible()) {
            m_out << iter.first << " " << iter.second->sessionUsername() << " " << iter.second->desktopName() << "\n";
        }
    }

    m_out.flush();
}

void GreeterConnection::receive()
{
    std::string cmd;
    m_in >> cmd;

    if (cmd == "NEW") {
        m_newSessionHandler();
        return;
    }

    if (cmd == "OPEN") {
        int id;
        m_in >> id;
        m_openSessionHandler(id);
        return;
    }

    if (cmd == "PASSWORD") {
        std::string password;
        m_in >> password;
        m_passwordHandler(password);
        return;
    }

    if (cmd == "CREDENTIALS") {
        std::string username;
        m_in >> username;
        std::string password;
        m_in >> password;
        m_credentialsHandler(username, password);
        return;
    }
}

void GreeterConnection::markDead()
{
    Log::debug() << "Greeter died (pid: " << m_greeterPID << ")" << std::endl;

    m_dead = true;
}
