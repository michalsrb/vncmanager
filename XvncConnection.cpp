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

#include <iostream>
#include <vector>

#include <gnutls/gnutls.h>
#include <gnutls/crypto.h>

#include "helper.h"

#include "Log.h"
#include "XvncConnection.h"


XvncConnection::XvncConnection(std::shared_ptr<Xvnc> xvnc)
    : m_xvnc(xvnc)
    , m_stream(xvnc->connect())
    , m_streamFormatter(&m_stream)
{
    Log::debug() << "Opening connection to Xvnc #" << m_xvnc->id() << std::endl;
}

XvncConnection::~XvncConnection()
{
    Log::debug() << "Closing connection to Xvnc #" << m_xvnc->id() << std::endl;

    m_xvnc->disconnect();
}

void XvncConnection::initialize()
{
    SecurityType selectedSecurityType = startInitialization( { SecurityType::None });
    if (selectedSecurityType != SecurityType::None) {
        throw ConnectionException(this, std::string("Connection to Xvnc was expecting security None, but got ") + std::to_string((int)selectedSecurityType));
    }

    handleNoneSecurity();
}

void XvncConnection::initialize(ConnectionInitializedHandler connectionInitializedHandler, PasswordRequestHandler passwordRequestHandler, CredentialsRequestHandler credentialsRequestHandler)
{
    m_connectionInitializedHandler = connectionInitializedHandler;
    m_passwordRequestHandler = passwordRequestHandler;
    m_credentialsRequestHandler = credentialsRequestHandler;

    SecurityType selectedSecurityType = startInitialization( {
        SecurityType::None, SecurityType::VncAuth, SecurityType::VeNCrypt
    });

    switch (selectedSecurityType) {
    case SecurityType::None:
        handleNoneSecurity();
        break;

    case SecurityType::VncAuth:
        handleVncAuthSecurity();
        break;

    case SecurityType::VeNCrypt:
        handleVeNCryptSecurity();
        break;

    default:
        assert(!"Not reached");
        break;
    }
}

SecurityType XvncConnection::startInitialization(std::set<SecurityType> supportedTypes)
{
    // Read the server's version
    char versionString[VersionStringLength];
    fmt().recv_raw(versionString, sizeof(versionString));
    if (strncmp(versionString, HighestVersionString, VersionStringLength) != 0) {
        throw ConnectionException(this, "Unsupported version of RFB protocol");
    }

    // Respond with the same version.
    fmt().send_raw(versionString, sizeof(versionString));

    // Negotiate security
    uint8_t numberOfSecurityTypes;
    fmt().recv(numberOfSecurityTypes);

    // Zero means the server wants to report failure
    if (numberOfSecurityTypes == 0) {
        throw ConnectionException(this, std::string("Connection failed, reason: ") + receiveFailureReason());
    }

    // Read the list of security types
    std::vector<SecurityType> securityTypes(numberOfSecurityTypes);
    fmt().recv(securityTypes);

    // Find any supported security type
    SecurityType selectedSecurityType = SecurityType::Invalid;
    for (SecurityType securityType : securityTypes) {
        if (supportedTypes.find((SecurityType)securityType) != supportedTypes.end()) {
            selectedSecurityType = securityType;
            break;
        }
    }
    if (selectedSecurityType == SecurityType::Invalid) {
        throw ConnectionException(this, "No supported security type offered");
    }

    // Respond with chosen security type
    fmt().send(selectedSecurityType);

    return selectedSecurityType;
}

void XvncConnection::handleNoneSecurity()
{
    receiveSecurityResult();

    completeInitialization();
}

void XvncConnection::handleVncAuthSecurity()
{
    m_passwordRequestHandler(std::bind(&XvncConnection::handleVncAuthSecurityWithPassword, this, std::placeholders::_1));
}

void XvncConnection::handleVncAuthSecurityWithPassword(std::string password)
{
    // Receive challenge
    VNCAuthMessage challenge_response;
    fmt().recv(challenge_response);

    // Convert the password into key
    unsigned char key[8];
    for (std::string::size_type i = 0; i < sizeof(key); i++) {
        if (i >= password.size()) {
            key[i] = '\0';
        } else {
            key[i] = ((password[i] * 0x0202020202ULL & 0x010884422010ULL) % 1023) & 0xfe;    // Reverse the order of bits in the byte. That's how VncAuth does it...
        }
    }

    // Encrypt the challenge with DES-ECB (gnutls offers only CBC, to get ECB we reinit the cipher for each block)
    const gnutls_datum_t datum = { key, sizeof(key) };
    gnutls_cipher_hd_t handle;

    for (int i = 0; i < 2; i++) {
        if (gnutls_cipher_init(&handle, GNUTLS_CIPHER_DES_CBC, &datum, nullptr) < 0) {
            throw std::runtime_error("gnutls_cipher_init failed");
        }

        if (gnutls_cipher_encrypt(handle, &challenge_response.data[i * 8], 8) < 0) {
            throw std::runtime_error("gnutls_cipher_encrypt failed");
        }

        gnutls_cipher_deinit(handle);
    }

    // Send response back
    fmt().send(challenge_response);

    receiveSecurityResult();

    completeInitialization();
}

void XvncConnection::handleVeNCryptSecurity()
{
    // Receive server's VeNCrypt version
    VeNCryptVersion version;
    fmt().recv(version);
    if (version.major != 0 || version.minor != 2) {
        throw ConnectionException(this, "Unsupported VeNCrypt version.");
    }

    // Send our version (which is the same as server's)
    fmt().send(version);

    // Receive status
    uint8_t status;
    fmt().recv(status);
    if (status != 0) {
        throw ConnectionException(this, "VeNCrypt version selection failed.");
    }

    // Receive list of VeNCrypt subtypes
    uint8_t length;
    fmt().recv(length);
    std::vector<VeNCryptSubtype> subtypes(length);
    fmt().recv(subtypes);

    VeNCryptSubtype selectedSubtype = VeNCryptSubtype::Invalid;
    for (VeNCryptSubtype subtype : subtypes) {
        switch (subtype) {
        case VeNCryptSubtype::Plain:
        case VeNCryptSubtype::None:
        case VeNCryptSubtype::VncAuth:
            selectedSubtype = subtype;
            break;

        default:
            // Other VeNCrypt subtypes are not supported.
            // Empty default case to silence Wswitch.
            break;
        }

        if (selectedSubtype != VeNCryptSubtype::Invalid) {
            break;
        }
    }

    if (selectedSubtype == VeNCryptSubtype::Invalid) {
        throw ConnectionException(this, "No supported VeNCrypt subtype available.");
    }

    // Send the subtype we want
    fmt().send(selectedSubtype);

    switch (selectedSubtype) {
    case VeNCryptSubtype::None:
        handleNoneSecurity();
        break;

    case VeNCryptSubtype::VncAuth:
        handleVncAuthSecurity();
        break;

    case VeNCryptSubtype::Plain:
        // Request username and password
        m_credentialsRequestHandler(std::bind(&XvncConnection::handleVeNCryptSecurityWithCredentials, this, std::placeholders::_1, std::placeholders::_2));
        break;

    default:
        assert(!"Not reached"); // This would mean that we selected subtype that we don't support -> our bug.
        break;
    }
}

void XvncConnection::handleVeNCryptSecurityWithCredentials(std::string username, std::string password)
{
    VeNCryptPlainMessage message;
    message.usernameLength = username.length();
    message.passwordLength = password.length();

    fmt().send(message);
    fmt().send(username);
    fmt().send(password);

    receiveSecurityResult();

    completeInitialization();
}

void XvncConnection::receiveSecurityResult()
{
    // Expect the security result to succeed
    uint32_t status;
    fmt().recv(status);

    if (status != 0) {
        throw ConnectionException(this, std::string("Connection failed, reason: ") + receiveFailureReason());
    }
}

void XvncConnection::completeInitialization()
{
    // Send shared flag
    ClientInitMessage clientInit;
    clientInit.shared = true;
    fmt().send(clientInit);

    // Receive server init
    ServerInitMessage serverInit;
    fmt().recv(serverInit);

    m_framebufferWidth = serverInit.framebufferWidth;
    m_framebufferHeight = serverInit.framebufferHeight;

    m_pixelFormat = serverInit.serverPixelFormat;

    m_xvnc->setDesktopName(fmt().recv_string(serverInit.nameLength));

    if (m_connectionInitializedHandler) {
        m_connectionInitializedHandler();
        m_connectionInitializedHandler = ConnectionInitializedHandler();
    }
}

std::string XvncConnection::receiveFailureReason()
{
    uint32_t reasonLength;
    fmt().recv(reasonLength);

    return fmt().recv_string(reasonLength);
}

void XvncConnection::sendSetPixelFormat(const PixelFormat &pixelFormat)
{
    m_pixelFormat = pixelFormat;

    SetPixelFormatMessage message;
    message.pixelFormat = pixelFormat;

    fmt().send(message);
}

void XvncConnection::sendSetEncodings(const std::vector<EncodingType> &encodings)
{
    SetEncodingsMessage message;
    message.numberOfEncodings = encodings.size();

    fmt().send(message);
    fmt().send(encodings);
}

void XvncConnection::sendNonIncrementalFramebufferUpdateRequest()
{
    FramebufferUpdateRequestMessage message;
    message.xPosition = 0;
    message.yPosition = 0;
    message.width = framebufferWidth();
    message.height = framebufferHeight();
    message.incremental = false;

    fmt().send(message);
}

void XvncConnection::setFramebufferSize(uint16_t width, uint16_t height)
{
    m_framebufferWidth = width;
    m_framebufferHeight = height;
}

void XvncConnection::setDesktopName(const std::string &desktopName)
{
    m_xvnc->setDesktopName(desktopName);
}
