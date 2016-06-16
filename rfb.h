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

#ifndef RFB_H
#define RFB_H

#include <arpa/inet.h>


// The only version we currently support
constexpr size_t VersionStringLength = 12;
constexpr char HighestVersionString[VersionStringLength] = { 'R', 'F', 'B', ' ', '0', '0', '3', '.', '0', '0', '8', '\n' };

enum class ClientMessageType : uint8_t
{
    SetPixelFormat = 0,
    SetEncodings = 2,
    FramebufferUpdateRequest = 3,
    KeyEvent = 4,
    PointerEvent = 5,
    ClientCutText = 6,

    SetDesktopSize = 251
};

enum class ServerMessageType : uint8_t
{
    FramebufferUpdate = 0,
    SetColourMapEntries = 1,
    Bell = 2,
    ServerCutText = 3
};

// Only the security types we support
enum class SecurityType : uint8_t
{
    Invalid = 0,
    None = 1,
    VncAuth = 2,
    VeNCrypt = 19,
};

enum class SecurityResult : uint32_t
{
    OK = 0,
    Failed = 1
};

// Contains only supported encodings
enum class EncodingType : int32_t
{
    Raw = 0,
    CopyRect = 1,
    RRE = 2,
//   Hextile = 5, // TODO?
    Tight = 7,
//   ZRLE = 16,   // TODO?

    JpegQualityLowest = -32,
    JpegQualityHighest = -23,

    DesktopSize = -223,
    LastRect = -224,
    Cursor = -239,
    XCursor = -240,
    DesktopName = -307,
    ExtendedDesktopSize = -308
};

#pragma pack(push, 1)

struct ClientInitMessage {
    uint8_t shared;

    void ntoh() {}
    void hton() {}
};

struct PixelFormat {
    uint8_t bitsPerPixel;
    uint8_t depth;
    uint8_t bigEndianFlag;
    uint8_t trueColourFlag;
    uint16_t redMax;
    uint16_t greenMax;
    uint16_t blueMax;
    uint8_t redShift;
    uint8_t greenShift;
    uint8_t blueShift;

    uint8_t _padding[3] = { 0, 0, 0 };

    void ntoh() {
        if (bigEndianFlag) {
            redMax = ntohs(redMax);
            greenMax = ntohs(greenMax);
            blueMax = ntohs(blueMax);
        }
    }

    void hton() {
        bigEndianFlag = (__BYTE_ORDER == __BIG_ENDIAN);
    }

    bool operator==(const PixelFormat &another) const {
        return
            bitsPerPixel == another.bitsPerPixel &&
            depth == another.depth &&
            bigEndianFlag == another.bigEndianFlag &&
            trueColourFlag == another.trueColourFlag &&
            redMax == another.redMax &&
            greenMax == another.greenMax &&
            blueMax == another.blueMax &&
            redShift == another.redShift &&
            greenShift == another.greenShift &&
            blueShift == another.blueShift;
    }

    bool operator!= (const PixelFormat &another) const {
        return !(*this == another);
    }

    bool valid() {
        // Validating only things that could hurt vncmanager. If there are some other wrong values, underlying VNC server should complain.
        return bitsPerPixel == 8 || bitsPerPixel == 16 || bitsPerPixel == 24 || bitsPerPixel == 32;
    }
};

struct ServerInitMessage {
    uint16_t framebufferWidth;
    uint16_t framebufferHeight;

    PixelFormat serverPixelFormat;

    uint32_t nameLength;

    void ntoh() {
        framebufferWidth = ntohs(framebufferWidth);
        framebufferHeight = ntohs(framebufferHeight);

        serverPixelFormat.ntoh();

        nameLength = ntohl(nameLength);
    }

    void hton() {
        framebufferWidth = htons(framebufferWidth);
        framebufferHeight = htons(framebufferHeight);

        serverPixelFormat.hton();

        nameLength = htonl(nameLength);
    }
};

struct SetPixelFormatMessage {
    ClientMessageType type = ClientMessageType::SetPixelFormat;

    uint8_t _padding[3] = { 0, 0, 0 };

    PixelFormat pixelFormat;

    void ntoh() {
        pixelFormat.ntoh();
    }

    void hton() {
        pixelFormat.hton();
    }
};

struct SetEncodingsMessage {
    ClientMessageType type = ClientMessageType::SetEncodings;

    uint8_t _padding = 0;

    uint16_t numberOfEncodings;

    // list of encodings follows

    void ntoh() {
        numberOfEncodings = ntohs(numberOfEncodings);
    }

    void hton() {
        numberOfEncodings = htons(numberOfEncodings);
    }
};

struct FramebufferUpdateRequestMessage {
    ClientMessageType type = ClientMessageType::FramebufferUpdateRequest;

    uint8_t incremental;
    uint16_t xPosition;
    uint16_t yPosition;
    uint16_t width;
    uint16_t height;

    void ntoh() {
        xPosition = ntohs(xPosition);
        yPosition = ntohs(yPosition);
        width = ntohs(width);
        height = ntohs(height);
    }

    void hton() {
        xPosition = htons(xPosition);
        yPosition = htons(yPosition);
        width = htons(width);
        height = htons(height);
    }
};

struct KeyEventMessage {
    ClientMessageType type = ClientMessageType::KeyEvent;

    uint8_t downFlag;

    uint8_t _padding[2] = { 0, 0 };

    uint32_t key;

    void ntoh() {
        key = ntohl(key);
    }

    void hton() {
        key = htonl(key);
    }
};

struct PointerEventMessage {
    ClientMessageType type = ClientMessageType::PointerEvent;

    uint8_t buttonMask;
    uint16_t xPosition;
    uint16_t yPosition;

    void ntoh() {
        xPosition = ntohs(xPosition);
        yPosition = ntohs(yPosition);
    }

    void hton() {
        xPosition = htons(xPosition);
        yPosition = htons(yPosition);
    }
};

struct ClientCutTextMessage {
    ClientMessageType type = ClientMessageType::ClientCutText;

    uint8_t _padding[3] = { 0, 0, 0 };

    uint32_t length;

    // text follows

    void ntoh() {
        length = ntohl(length);
    }

    void hton() {
        length = htonl(length);
    }
};

struct FramebufferUpdateMessage {
    ServerMessageType type = ServerMessageType::FramebufferUpdate;

    uint8_t _padding = 0;

    uint16_t numberOfRectangles;

    // rectangles follow

    void ntoh() {
        numberOfRectangles = ntohs(numberOfRectangles);
    }

    void hton() {
        numberOfRectangles = htons(numberOfRectangles);
    }
};

struct FramebufferUpdateRectangle {
    uint16_t xPosition = 0;
    uint16_t yPosition = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    EncodingType encodingType;

    // data follow

    void ntoh() {
        xPosition = ntohs(xPosition);
        yPosition = ntohs(yPosition);
        width = ntohs(width);
        height = ntohs(height);
        encodingType =(EncodingType ) ntohl(( int32_t ) encodingType);
    }

    void hton() {
        xPosition = htons(xPosition);
        yPosition = htons(yPosition);
        width = htons(width);
        height = htons(height);
        encodingType =(EncodingType ) htonl(( int32_t ) encodingType);
    }
};

struct SetColourMapEntriesMessage {
    ServerMessageType type = ServerMessageType::SetColourMapEntries;

    uint8_t _padding = 0;

    uint16_t firstColour;
    uint16_t numberOfColours;

    // colours follow

    void ntoh() {
        firstColour = ntohs(firstColour);
        numberOfColours = ntohs(numberOfColours);
    }

    void hton() {
        firstColour = htons(firstColour);
        numberOfColours = htons(numberOfColours);
    }
};

struct ColourMapEntry {
    uint16_t red;
    uint16_t green;
    uint16_t blue;

    void ntoh() {
        red = ntohs(red);
        green = ntohs(green);
        blue = ntohs(blue);
    }

    void hton() {
        red = htons(red);
        green = htons(green);
        blue = htons(blue);
    }
};

struct BellMessage {
    ServerMessageType type = ServerMessageType::Bell;

    void ntoh() {}
    void hton() {}
};

struct ServerCutTextMessage {
    ServerMessageType type = ServerMessageType::ServerCutText;

    uint8_t _padding[3] = { 0, 0, 0 };

    uint32_t length;

    // text follows

    void ntoh() {
        length = ntohl(length);
    }

    void hton() {
        length = htonl(length);
    }
};

struct VNCAuthMessage {
    uint8_t data[16];

    void ntoh() {}
    void hton() {}
};

// ExtendedDesktopSize extension

struct SetDesktopSizeMessage {
    ClientMessageType type = ClientMessageType::SetDesktopSize;

    uint8_t _padding = 0;

    uint16_t width;
    uint16_t height;
    uint8_t numberOfScreens;

    uint8_t _padding2 = 0;

    void ntoh() {
        width = ntohs(width);
        height = ntohs(height);
    }

    void hton() {
        width = htons(width);
        height = htons(height);
    }
};

struct ExtendedDesktopSizeRectangleData {
    uint8_t numberOfScreens;

    uint8_t _padding[3] = { 0, 0, 0 };

    void ntoh() {}
    void hton() {}
};

struct SetDesktopSizeScreen {
    uint32_t id;
    uint16_t xPosition;
    uint16_t yPosition;
    uint16_t width;
    uint16_t height;
    uint32_t flags;

    void ntoh() {
        id = ntohl(id);
        xPosition = ntohs(xPosition);
        yPosition = ntohs(yPosition);
        width = ntohs(width);
        height = ntohs(height);

        // flags are currently not used by vncmanager, not sure if they should be swapped
    }

    void hton() {
        id = htonl(id);
        xPosition = htons(xPosition);
        yPosition = htons(yPosition);
        width = htons(width);
        height = htons(height);

        // flags are currently not used by vncmanager, not sure if they should be swapped
    }
};

enum class ExtendedDesktopSizeStatus
{
    NoError = 0,
    ResizeProhibited = 1,
    OutOfResources = 2,
    InvalidScreenLayout = 3
};

struct VeNCryptVersion {
    uint8_t major;
    uint8_t minor;

    void ntoh() {}
    void hton() {}
};

enum class VeNCryptSubtype : uint32_t   // Contains only the supported subtypes
{
    Invalid = (uint32_t) SecurityType::Invalid,
    None = (uint32_t) SecurityType::None,
    VncAuth = (uint32_t) SecurityType::VncAuth,

    Plain = 256,   // (used in direction towards local Xvnc)
    TLSNone = 257,
    X509None = 260
};

struct VeNCryptPlainMessage {
    uint32_t usernameLength;
    uint32_t passwordLength;

    // username and password follows

    void ntoh() {
        usernameLength = ntohl(usernameLength);
        passwordLength = ntohl(passwordLength);
    }

    void hton() {
        usernameLength = htonl(usernameLength);
        passwordLength = htonl(passwordLength);
    }
};

struct TightCompressionControl {
    unsigned resetStream0 : 1;
    unsigned resetStream1 : 1;
    unsigned resetStream2 : 1;
    unsigned resetStream3 : 1;

    unsigned rest : 4;

    int useStream() const {
        return rest & 0x3;
    }

    bool isBasicCompression() const {
        return !(rest & 0x8);
    }
    bool isFillCompression() const {
        return rest == 0x8;
    }
    bool isJpegCompression() const {
        return rest == 0x9;
    }

    bool readFilterId() const {
        return rest & 0x4;
    }

    void ntoh() {}
    void hton() {}
};

struct TightPixel {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

enum class TightFilter : uint8_t
{
    Copy = 0,
    Palette = 1,
    Gradient = 2
};

constexpr int TightMinSizeToCompress = 12;

#pragma pack(pop)

#endif
