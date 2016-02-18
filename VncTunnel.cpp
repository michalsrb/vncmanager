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

#include <sstream>
#include <vector>

#include "TLSStream.h"
#include "FdStream.h"
#include "VncTunnel.h"
#include "helper.h"
#include "Configuration.h"
#include "Log.h"


VncTunnel::VncTunnel(XvncManager &xvncManager, GreeterManager &greeterManager, ControllerManager &controllerManager, int fd)
    : m_xvncManager(xvncManager)
    , m_greeterManager(greeterManager)
    , m_controllerManager(controllerManager)
    , m_stream(new FdStream(fd))
    , m_streamFormatter(m_stream)
{
}

VncTunnel::~VncTunnel()
{
    if (m_greeterConnection) {
        m_greeterManager.releaseGreeter(m_greeterConnection);
    }

    delete m_currentConnection;
    delete m_potentialConnection;

    delete m_stream;
}

void VncTunnel::start()
{
    Log::info() << "Accepted client " << (intptr_t)this << "." << std::endl;

    try {
        // Create new session
        bool showGreeter = !Configuration::options["disable-manager"].as<bool>() && (Configuration::options["always-show-greeter"].as<bool>() || m_xvncManager.hasVisibleSessions());

        if (showGreeter) {
            m_tightEncodingDisabled = true;
        }

        auto xvnc = m_xvncManager.createSession(!showGreeter);

        if (showGreeter) {
            m_greeterConnection = m_greeterManager.createGreeter(xvnc->display(), xvnc->xauthFilename(), std::bind(&VncTunnel::newSessionHandler, this), std::bind(&VncTunnel::openSessionHandler, this, std::placeholders::_1));
        }

        m_currentConnection = new XvncConnection(xvnc);
        m_currentConnection->initialize();

        m_pixelFormat = m_currentConnection->pixelFormat();

        clientInitalize();

        while (true) {
            try {
                if (m_greeterConnection) {
                    m_greeterConnection->update();
                }

                select();
            } catch (XvncConnection::ConnectionException &e) {
                if (m_greeterConnection) {
                    m_greeterConnection->showError(e.what());
                }

                if (e.faultyConnection() == m_currentConnection) {
                    throw;
                }

                if (e.faultyConnection() == m_potentialConnection) {
                    delete m_potentialConnection;
                    m_potentialConnection = nullptr;

                    Log::notice() << "Client " << (intptr_t)this << " failed to switch connection: " << e.what() << std::endl;
                }
            } catch (eof_exception &e) {
                break;
            }
        }
    } catch (std::exception &e) {
        Log::error() << "Exception in thread of client " << (intptr_t)this << ": " << e.what() << std::endl;
    }

    Log::info() << "Disconnected client " << (intptr_t)this << "." << std::endl;

    delete this;
}

void VncTunnel::clientInitalize()
{
    // Send our version string
    cFmt().send_raw(HighestVersionString);

    // Receive client's version string
    char versionString[VersionStringLength];
    cFmt().recv(versionString);
    if (strncmp(versionString, HighestVersionString, VersionStringLength) != 0) {
        uint8_t numberOfSecurityTypes = 0;
        cFmt().send(numberOfSecurityTypes);

        sendReason("Client version is not supported.");

        throw std::runtime_error("Client requires unsupported version.");
    }

    // Send our security types
    auto securityTypes = configuredSecurityTypes();
    cFmt().send((uint8_t)securityTypes.size());
    cFmt().send(securityTypes);

    // Receive client's chosen security type
    SecurityType chosenSecurityType;
    cFmt().recv(chosenSecurityType);

    if (std::find(securityTypes.begin(), securityTypes.end(), chosenSecurityType) == securityTypes.end()) {
        cFmt().send(SecurityResult::Failed);

        sendReason("Client chosen invalid security type.");

        throw std::runtime_error("Client chosen invalid security type.");
    }

    switch (chosenSecurityType) {
    case SecurityType::None:
        handleNoneSecurity();
        break;

    case SecurityType::VeNCrypt:
        handleVeNCryptSecurity();
        break;

    default:
        assert(!"Not reached"); // We have already verified that the client chosen one of the types we offered. They should be all handled in this switch.
        break;
    }

    m_securityType = chosenSecurityType;
}

void VncTunnel::handleNoneSecurity()
{
    // Send security result message
    cFmt().send(SecurityResult::OK);

    // Finnish
    finishClientInitialization();
}

void VncTunnel::handleVeNCryptSecurity()
{
    // Send our VeNCrypt version
    VeNCryptVersion version;
    version.major = 0;
    version.minor = 2;
    cFmt().send(version);

    // Receive client's version (which should be the same)
    cFmt().recv(version);

    // Send status (0 means we accept it, anything else means the communication is over)
    uint8_t status = (version.major != 0 || version.minor != 2);
    cFmt().send(status);
    if (status != 0) {
        throw std::runtime_error("Unsupported VeNCrypt version.");
    }

    // Send supported subtypes
    std::vector<VeNCryptSubtype> subtypes = Configuration::options["security"].as<std::vector<VeNCryptSubtype>>();
    uint8_t count = subtypes.size();
    cFmt().send(count);
    cFmt().send(subtypes);

    VeNCryptSubtype selectedSubtype;
    cFmt().recv(selectedSubtype);

    std::string subtypeError = "Client chosen invalid VeNCrypt security subtype.";
    if (std::find(subtypes.begin(), subtypes.end(), selectedSubtype) == subtypes.end()) {
        cFmt().send((uint8_t)0); // TODO: How is this value called?
        cFmt().send(SecurityResult::Failed);
        cFmt().send((uint32_t)subtypeError.length());
        cFmt().send(subtypeError);
        throw std::runtime_error(subtypeError);
    }

    cFmt().send((uint8_t)1); // TODO: How is this value called?

    switch (selectedSubtype) {
    case VeNCryptSubtype::TLSNone:
    case VeNCryptSubtype::X509None: {
        // Convert the client stream into TLSStream
        bool anonymousTLS = (selectedSubtype == VeNCryptSubtype::TLSNone);
        TLSStream *newTLSStream = new TLSStream(m_stream->takeFd(), anonymousTLS);
        delete m_stream;
        m_stream = newTLSStream;
        m_streamFormatter = StreamFormatter(m_stream);

        newTLSStream->initialize();

        // Proceed with handling None security
        handleNoneSecurity();
        break;
    }

    case VeNCryptSubtype::None:
        handleNoneSecurity();
        break;

    default:
        assert(!"Not reached"); // We have verified that the client chose one of the subtypes we offered and they should be all handled in this switch.
    }
}

void VncTunnel::finishClientInitialization()
{
    // Receive client's client init message
    ClientInitMessage clientInit;
    cFmt().recv(clientInit);

    // TODO: React somehow to shared attribute?
//   if(!clientInit.shared) {
//     throw std::runtime_error("Client requires non-shared session, not supported yet.");
//   }

    // Send ServerInit message
    ServerInitMessage serverInit;
    serverInit.framebufferWidth = m_currentConnection->framebufferWidth();
    serverInit.framebufferHeight = m_currentConnection->framebufferHeight();
    serverInit.serverPixelFormat = m_currentConnection->pixelFormat();
    serverInit.nameLength = m_currentConnection->desktopName().length();

    cFmt().send(serverInit);
    cFmt().send(m_currentConnection->desktopName());
}

void VncTunnel::select()
{
    m_selector.clear();
    m_selector.addStream(cStream(), std::bind(&VncTunnel::clientReceive, this));
    m_selector.addStream(sStream(), std::bind(&VncTunnel::serverReceive, this));
    if (m_greeterConnection) {
        m_greeterConnection->prepareSelect(m_selector);
    }

    m_selector.select();
}

void VncTunnel::clientReceive()
{
    ClientMessageType messageType;
    cFmt().recv(messageType);
    cFmt().push_back(messageType);

    switch (messageType) {
    case ClientMessageType::SetPixelFormat:
        processSetPixelFormat();
        break;

    case ClientMessageType::SetEncodings:
        processSetEncodings();
        break;

    case ClientMessageType::FramebufferUpdateRequest:
        processFramebufferUpdateRequest();
        break;

    case ClientMessageType::KeyEvent:
        processKeyEvent();
        break;

    case ClientMessageType::PointerEvent:
        processPointerEvent();
        break;

    case ClientMessageType::ClientCutText:
        processClientCutText();
        break;

    case ClientMessageType::SetDesktopSize:
        processSetDesktopSize();
        break;

    default:
        throw std::runtime_error("Received unknown message type from vnc client");
    }
}

void VncTunnel::processSetPixelFormat()
{
    SetPixelFormatMessage message;
    cFmt().recv(message);

    if (!message.pixelFormat.valid())
        throw std::runtime_error("Received invalid pixel format from vnc client");

    m_pixelFormat = message.pixelFormat;

    m_currentConnection->sendSetPixelFormat(m_pixelFormat);
}

void VncTunnel::processSetEncodings()
{
    SetEncodingsMessage message;
    cFmt().recv(message);

    std::vector<EncodingType> encodings(message.numberOfEncodings);
    cFmt().recv(encodings);

    // Filter down only to encodings we support
    m_supportedEncodingsClient.clear();
    m_supportedEncodingsServer.clear();
    for (EncodingType encoding : encodings) {
        switch (encoding) {
        case EncodingType::Raw:
        case EncodingType::CopyRect:
        case EncodingType::RRE:
        case EncodingType::DesktopSize:
        case EncodingType::LastRect:
        case EncodingType::Cursor:
        case EncodingType::XCursor:
        case EncodingType::DesktopName:
        case EncodingType::ExtendedDesktopSize:
            m_supportedEncodingsClient.insert(encoding);
            m_supportedEncodingsServer.push_back(encoding);
            break;

        case EncodingType::Tight:
            m_supportedEncodingsClient.insert(encoding);
            if (!m_tightEncodingDisabled) {
                m_supportedEncodingsServer.push_back(encoding);
            }
            break;

        }

        if ((int32_t)encoding >= (int32_t)EncodingType::JpegQualityLowest && (int32_t)encoding <= (int32_t)EncodingType::JpegQualityHighest) {
            m_supportedEncodingsClient.insert(encoding);
            m_supportedEncodingsServer.push_back(encoding);
        }
    }

    if (!clientSupportsEncoding(EncodingType::DesktopName)) {
        m_supportedEncodingsServer.push_back(EncodingType::DesktopName);    // We always ask to get desktop name updates from server
    }

    m_currentConnection->sendSetEncodings(m_supportedEncodingsServer);
}

void VncTunnel::processFramebufferUpdateRequest()
{
    cFmt().forward_directly(sStream(), sizeof(FramebufferUpdateRequestMessage));
}

void VncTunnel::processKeyEvent()
{
    cFmt().forward_directly(sStream(), sizeof(KeyEventMessage));
}

void VncTunnel::processPointerEvent()
{
    cFmt().forward_directly(sStream(), sizeof(PointerEventMessage));
}

void VncTunnel::processClientCutText()
{
    ClientCutTextMessage message;
    cFmt().forward(sStream(), message);
    cFmt().forward_directly(sStream(), message.length);
}

void VncTunnel::processSetDesktopSize()
{
    SetDesktopSizeMessage message;
    cFmt().forward(sStream(), message);
    cFmt().forward_directly(sStream(), message.numberOfScreens * sizeof(SetDesktopSizeScreen));
}

void VncTunnel::serverReceive()
{
    ServerMessageType messageType;
    sFmt().recv(messageType);
    sFmt().push_back(messageType);

    switch (messageType) {
    case ServerMessageType::FramebufferUpdate:
        processFramebufferUpdate();
        break;

    case ServerMessageType::SetColourMapEntries:
        processSetColourMapEntries();
        throw std::runtime_error("SetColourMapEntries is not implemented!");
        break;

    case ServerMessageType::Bell:
        processBell();
        break;

    case ServerMessageType::ServerCutText:
        processServerCutText();
        break;

    default:
        throw std::runtime_error("Received unknown message type from Xvnc");
    }
}

void VncTunnel::processFramebufferUpdate()
{
    bool supportsLastRect = clientSupportsEncoding(EncodingType::LastRect);
    bool mustUseLastRect = false;

    FramebufferUpdateMessage message;
    sFmt().recv(message);

    int numberOfRectangles = message.numberOfRectangles;

    int extraRectanglesCount = countExtraRectangles();
    if (message.numberOfRectangles > std::numeric_limits<uint16_t>::max() - extraRectanglesCount) {
        if (supportsLastRect) {
            mustUseLastRect = true;
            message.numberOfRectangles = std::numeric_limits<uint16_t>::max();
        } else {
            throw std::runtime_error("Client doesn't support LastRect pseudo-encoding and sends too many rectangles in one update.");
        }
    } else {
        message.numberOfRectangles += extraRectanglesCount;
    }

    cFmt().send(message);

    sendExtraRectangles();

    bool lastRectReceived = false;
    for (int i = 0; i < numberOfRectangles && !lastRectReceived; i++) {
        FramebufferUpdateRectangle rectangle;
        sFmt().recv(rectangle);

        switch (rectangle.encodingType) {
            // Pass-thru encodings
        case EncodingType::Raw:
        case EncodingType::CopyRect:
        case EncodingType::Cursor:
        case EncodingType::XCursor: {
            std::size_t bufferSize;

            switch (rectangle.encodingType) {
            case EncodingType::Raw:
                bufferSize = rectangle.width * rectangle.height * m_pixelFormat.bitsPerPixel / 8;
                break;

            case EncodingType::CopyRect:
                bufferSize = 4;
                break;

            case EncodingType::Cursor:
                bufferSize = rectangle.width * rectangle.height * m_pixelFormat.bitsPerPixel / 8 + (rectangle.width + 7) / 8 * rectangle.height;
                break;

            case EncodingType::XCursor:
                bufferSize = 6 + (rectangle.width + 7) / 8 * rectangle.height * 2;
                break;

            default:
                assert(!"Not reached.");
                break;
            }

            cFmt().send(rectangle);
            sFmt().forward_directly(cStream(), bufferSize);
            break;
        }

        case EncodingType::RRE: {
            cFmt().send(rectangle);
            uint32_t numberOfSubrectangles;
            sFmt().forward(cStream(), numberOfSubrectangles);
            sFmt().forward_directly(cStream(), (m_pixelFormat.bitsPerPixel / 8) + numberOfSubrectangles * (m_pixelFormat.bitsPerPixel / 8 + 8));
            break;
        }

        case EncodingType::DesktopSize:
            cFmt().send(rectangle);
            m_currentConnection->setFramebufferSize(rectangle.width, rectangle.height);
            break;

        case EncodingType::LastRect:
            cFmt().send(rectangle);
            lastRectReceived = true;
            break;

        case EncodingType::DesktopName: {
            uint32_t nameLength;
            sFmt().recv(nameLength);

            m_currentConnection->setDesktopName(sFmt().recv_string(nameLength));

            if (clientSupportsEncoding(EncodingType::DesktopName)) {
                // The actual desktop name may be different than the one we just received. It is XvncConnection's business to decide.
                cFmt().send(rectangle);
                nameLength = m_currentConnection->desktopName().length();
                cFmt().send(nameLength);
                cFmt().send(m_currentConnection->desktopName());
            } else {
                if (supportsLastRect) {
                    // Just skip it
                    mustUseLastRect = true;
                } else {
                    // Client doesn't support DesktopName pseudo-encoding, but he already expects specific amount of rectangles. We have to send a dummy rectangle.
                    sendDummyRectangle();
                }
            }
            break;
        }

        case EncodingType::ExtendedDesktopSize: {
            if ((ExtendedDesktopSizeStatus)rectangle.yPosition == ExtendedDesktopSizeStatus::NoError) {
                m_currentConnection->setFramebufferSize(rectangle.width, rectangle.height);
            }

            cFmt().send(rectangle);
            ExtendedDesktopSizeRectangleData rectangleData;
            sFmt().forward(cStream(), rectangleData);
            sFmt().forward_directly(cStream(), sizeof(SetDesktopSizeScreen) * rectangleData.numberOfScreens);
            break;
        }

        case EncodingType::Tight: {
            cFmt().send(rectangle);

            auto forwardTightVariableLengthData = [this]() {
                std::size_t length = 0;

                uint8_t byte;
                sFmt().forward(cStream(), byte);
                length += (byte & 0x7f);

                if (byte & 0x80) {
                    sFmt().forward(cStream(), byte);
                    length += (byte & 0x7f) << 7;

                    if (byte & 0x80) {
                        sFmt().forward(cStream(), byte);
                        length += byte << 14;
                    }
                }

                sFmt().forward_directly(cStream(), length);
            };

            TightCompressionControl c;
            sFmt().recv(c);

            if (m_tightZlibResetQueued) {
                m_tightZlibResetQueued = false;
                c.resetStream0 = true;
                c.resetStream1 = true;
                c.resetStream2 = true;
                c.resetStream3 = true;
            }

            cFmt().send(c);

            if (c.isFillCompression()) {
                sFmt().forward_directly(cStream(), sizeof(TightPixel));
            }

            else if (c.isJpegCompression()) {
                forwardTightVariableLengthData();
            }

            else { /* basic compression */
                TightFilter filter = TightFilter::Copy;
                if (c.readFilterId()) {
                    sFmt().forward(cStream(), filter);
                }

                uint8_t bpp = m_pixelFormat.bitsPerPixel;

                if (filter == TightFilter::Palette) {
                    uint8_t paletteLength;
                    sFmt().forward(cStream(), paletteLength);
                    int actualPaletteLength = paletteLength + 1;

                    sFmt().forward_directly(cStream(), sizeof(TightPixel) * actualPaletteLength);

                    if (actualPaletteLength <= 2) {
                        bpp = 1;
                    } else {
                        bpp = 8;
                    }
                }

                std::size_t dataSize = (rectangle.width * bpp + 7) / 8 * rectangle.height;
                if (dataSize < TightMinSizeToCompress) {
                    sFmt().forward_directly(cStream(), dataSize);
                } else {
                    forwardTightVariableLengthData();
                }
            }

            break;
        }

        default:
            throw std::runtime_error("received unknown encoding!");
            break;
        }
    }

    if (supportsLastRect) {
        // If we threw away or added some rectangles and the list of rectangles didn't end with LastRect, send LastRect now.
        if (mustUseLastRect && !lastRectReceived) {
            sendLastRectangle();
        }
    } else {
        assert(!mustUseLastRect);
    }
}

void VncTunnel::processSetColourMapEntries()
{
    SetColourMapEntriesMessage message;
    sFmt().forward(cStream(), message);
    sFmt().forward_directly(cStream(), sizeof(ColourMapEntry) * message.numberOfColours);
}

void VncTunnel::processBell()
{
    sFmt().forward_directly(cStream(), sizeof(BellMessage));
}

void VncTunnel::processServerCutText()
{
    ServerCutTextMessage message;
    sFmt().forward(cStream(), message);
    sFmt().forward_directly(cStream(), message.length);
}

void VncTunnel::sendReason(std::string reason)
{
    uint32_t reasonLength = reason.length();
    cFmt().send(reasonLength);
    cFmt().send(reason);
}

void VncTunnel::sendDummyRectangle()
{
    if (clientSupportsEncoding(EncodingType::Raw)) {
        // Going to blacken left top pixel...
        FramebufferUpdateRectangle rectangle;
        rectangle.encodingType = EncodingType::Raw;
        rectangle.xPosition = 0;
        rectangle.yPosition = 0;
        rectangle.width = 1;
        rectangle.height = 1;
        uint32_t black = 0;
        cFmt().send(rectangle);
        cFmt().send_raw(&black, m_pixelFormat.bitsPerPixel / 8);
    } else if (clientSupportsEncoding(EncodingType::CopyRect)) {
        // Going to break left top pixel...
        FramebufferUpdateRectangle rectangle;
        rectangle.encodingType = EncodingType::CopyRect;
        rectangle.xPosition = 0;
        rectangle.yPosition = 0;
        rectangle.width = 1;
        rectangle.height = 1;
        uint16_t srcX = 1;
        uint16_t srcY = 0;
        cFmt().send(rectangle);
        cFmt().send(srcX);
        cFmt().send(srcY);

    } else {
        throw std::runtime_error("Needed to send dummy rectangle, but client doesn't support any suitable encoding.");
    }
}

void VncTunnel::sendLastRectangle()
{
    FramebufferUpdateRectangle rectangle;
    rectangle.encodingType = EncodingType::LastRect;
    cFmt().send(rectangle);
}

void VncTunnel::newSessionHandler()
{
    switchToConnection(m_xvncManager.createSession(true));
}

void VncTunnel::openSessionHandler(int id)
{
    switchToConnection(m_xvncManager.getSession(id));
}

void VncTunnel::switchToConnection(std::shared_ptr<Xvnc> xvnc)
{
    if (m_tightEncodingDisabled) {
        m_tightEncodingDisabled = false;
        if (clientSupportsEncoding(EncodingType::Tight)) {
            m_supportedEncodingsServer.insert(m_supportedEncodingsServer.begin(), EncodingType::Tight);
        }
    }

    delete m_potentialConnection;
    m_potentialConnection = new XvncConnection(xvnc);

    m_selector.cancel();

    assert(m_greeterConnection);
    m_potentialConnection->initialize(
        std::bind(&VncTunnel::connectionSwitched, this),
        std::bind(&GreeterConnection::askForPassword, m_greeterConnection, std::placeholders::_1),
        std::bind(&GreeterConnection::askForCredentials, m_greeterConnection, std::placeholders::_1)
    );
}

void VncTunnel::connectionSwitched()
{
    assert(m_greeterConnection);
    m_greeterManager.releaseGreeter(m_greeterConnection);
    m_greeterConnection = nullptr;

    assert(m_potentialConnection);
    delete m_currentConnection;
    m_currentConnection = m_potentialConnection;
    m_potentialConnection = nullptr;

    m_selector.cancel();

    if (m_currentConnection->pixelFormat() != m_pixelFormat) {
        m_currentConnection->sendSetPixelFormat(m_pixelFormat);
    }

    m_currentConnection->sendSetEncodings(m_supportedEncodingsServer);

    m_currentConnection->sendNonIncrementalFramebufferUpdateRequest(); // XXX, TODO: The response to this may come as surprise to the client if it didn't have pending request.

    m_tightZlibResetQueued = true;

    if (clientSupportsEncoding(EncodingType::DesktopName)) {
        m_desktopNameChangeQueued = true;
    }
}

int VncTunnel::countExtraRectangles()
{
    int count = 0;

    if (m_desktopNameChangeQueued) {
        count++;
    }

    return count;
}

void VncTunnel::sendExtraRectangles()
{
    if (m_desktopNameChangeQueued) {
        m_desktopNameChangeQueued = false;
        FramebufferUpdateRectangle rectangle;
        rectangle.xPosition = 0;
        rectangle.yPosition = 0;
        rectangle.width = 0;
        rectangle.height = 0;
        rectangle.encodingType = EncodingType::DesktopName;

        cFmt().send(rectangle);

        std::string name = m_currentConnection->desktopName();

        uint32_t nameLength = name.length();
        cFmt().send(nameLength);
        cFmt().send(name);
    }
}

std::vector<SecurityType> VncTunnel::configuredSecurityTypes()
{
    std::vector<SecurityType> securityTypes;

    auto venCryptSubtypes = Configuration::options["security"].as<std::vector<VeNCryptSubtype>>();

    bool venCryptIncluded = false;

    for (auto subtype : venCryptSubtypes) {
        switch (subtype) {
        case VeNCryptSubtype::None:
            securityTypes.push_back(SecurityType::None);
            break;

        case VeNCryptSubtype::TLSNone:
        case VeNCryptSubtype::X509None:
            if (!venCryptIncluded) {
                securityTypes.push_back(SecurityType::VeNCrypt);
                venCryptIncluded = true;
            }
            break;

        default:
            assert(!"Not reached"); // Configuration is not supposed to allow any other values
        }
    }

    return securityTypes;
}

bool VncTunnel::clientSupportsEncoding(EncodingType encoding)
{
    return m_supportedEncodingsClient.find(encoding) != m_supportedEncodingsClient.end();
}
