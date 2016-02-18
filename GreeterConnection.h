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

#ifndef GREETERCONNECTION_H
#define GREETERCONNECTION_H

#include <iostream>
#include <functional>
#include <string>

#include <boost/iostreams/device/file_descriptor.hpp>
#include <boost/iostreams/stream.hpp>

#include "ReadSelector.h"


class GreeterManager;

/**
 * @brief a class taking care of communicating with single greeter program.
 *
 * This class takes care of starting, stopping and communicating with a greeter program.
 *
 * @remark This class is not thread-safe and is intended to be used inside VncTunnel's thread.
 *
 */
class GreeterConnection
{
public:
    typedef std::function<void(void)> NewSessionHandler;
    typedef std::function<void(int)> OpenSessionHandler;
    typedef std::function<void(std::string)> PasswordHandler;
    typedef std::function<void(std::string, std::string)> CredentialsHandler;

public:
    /**
     * @brief Construct the object, spawn greeter program and open connection to him.
     *
     * @param greeterManager Reference to GreeterManager
     * @param display X server display name the greeter program should display on
     * @param xauthFilename Path to xauth file containing access cookie to the X server.
     * @param newSessionHandler Handler for New Session event.
     * @param openSessionHandler Handler for Open Session event.
     */
    GreeterConnection(GreeterManager &greeterManager, std::string display, std::string xauthFilename, NewSessionHandler newSessionHandler, OpenSessionHandler openSessionHandler);

    GreeterConnection(const GreeterConnection &) = delete;
    GreeterConnection &operator=(const GreeterConnection &) = delete;

    ~GreeterConnection();

    /**
     * This function should be called regularly to keep greeter program up-to-date.
     */
    void update();

    /**
     * Register handlers into given ReadSelector
     */
    void prepareSelect(ReadSelector &readSelector);

    /**
     * @brief Send request to ask for password to the greeter program.
     *
     * @param passwordHandler Handler that will be called once the password is received.
     */
    void askForPassword(PasswordHandler passwordHandler);

    /**
     * @brief Send request to ask for credentials to the greeter program.
     *
     * @param credentialsHandler Handler that will be called once the credentials are received.
     */
    void askForCredentials(CredentialsHandler credentialsHandler);

    /**
     * Send request to show error to the greeter program.
     */
    void showError(std::string error);

    /**
     * PID of the greeter program.
     */
    pid_t greeterPID() const { return m_greeterPID; }

    /**
     * Mark the greeter program as dead.
     * This method should be called when it is known for sure that the program is not running anymore.
     */
    void markDead();

private:
    void sendSessions();
    void receive();

private:
    GreeterManager &m_greeterManager;

    NewSessionHandler m_newSessionHandler;
    OpenSessionHandler m_openSessionHandler;
    PasswordHandler m_passwordHandler;
    CredentialsHandler m_credentialsHandler;

    pid_t m_greeterPID;

    int m_greeterStdin;
    int m_greeterStdout;

    boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_sink> m_greeterIn;
    boost::iostreams::stream_buffer<boost::iostreams::file_descriptor_source> m_greeterOut;

    std::istream m_in;
    std::ostream m_out;

    bool m_dead = false;

    int m_lastSentSessionListVersion = 0;
};

#endif // GREETERCONNECTION_H
