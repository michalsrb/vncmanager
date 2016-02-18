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

#ifndef STREAMFORMATER_H
#define STREAMFORMATER_H

#include <type_traits>

#include <string.h>

#include <arpa/inet.h>

#include "Stream.h"


/**
 * @brief a class that takes care of reading/writing structures to/from Stream.
 *
 * It contains several template methods to automatically handle reading and writing of various data types.
 *
 * Methods types:
 *  * recv/send/forward: The size is taken from given type. Endianity is appropriatelly converted before/after writing/reading. Class types need to have public ntoh and hton methods that switch the endianity of the members of the object in place.
 *  * recv_raw/send_raw/forward_raw: The size is taken from given type. Endianity is not handled.
 *  * forward_direct: Forward given amount of bytes without touching them.
 *
 * @remark This class is not thread-safe and requires external synchronization if shared between threads.
 */

class StreamFormatter
{
private:
    template<class T, typename std::enable_if<std::is_class<T>::value>::type *dummy = nullptr>
    static void hton(T &t) {
        t.hton();
    }

    template<class T, typename std::enable_if<std::is_enum<T>::value>::type *dummy = nullptr>
    static void hton(T &t) {
        auto ti = static_cast<typename std::underlying_type<T>::type>(t);
        hton(ti);
        t = static_cast<T>(ti);
    }

    static void hton(uint8_t &t) {}
    static void hton(int8_t &t)  {}
    static void hton(char &t)    {}
    static void hton(uint16_t &t) {
        t = htons(t);
    }
    static void hton(int16_t &t)  {
        t = htons(t);
    }
    static void hton(uint32_t &t) {
        t = htonl(t);
    }
    static void hton(int32_t &t)  {
        t = htonl(t);
    }


    template<class T, typename std::enable_if<std::is_class<T>::value>::type *dummy = nullptr>
    static void ntoh(T &t) {
        t.ntoh();
    }

    template<class T, typename std::enable_if<std::is_enum<T>::value>::type *dummy = nullptr>
    static void ntoh(T &t) {
        auto ti = static_cast<typename std::underlying_type<T>::type>(t);
        ntoh(ti);
        t = static_cast<T>(ti);
    }

    static void ntoh(uint8_t &t) {}
    static void ntoh(int8_t &t)  {}
    static void ntoh(char &t)    {}
    static void ntoh(uint16_t &t) {
        t = ntohs(t);
    }
    static void ntoh(int16_t &t)  {
        t = ntohs(t);
    }
    static void ntoh(uint32_t &t) {
        t = ntohl(t);
    }
    static void ntoh(int32_t &t)  {
        t = ntohl(t);
    }

public:
    StreamFormatter(Stream *stream);

    Stream *stream() const {
        return m_stream;
    }


    /**
     * Send raw data to the stream as they are.
     */
    void send_raw(const void *buf, std::size_t len);

    /**
     * Send referenced variable to the stream as-is.
     * Writes sizeof(T) bytes.
     */
    template<typename T>
    void send_raw(const T &t) {
        send_raw(&t, sizeof(T));
    }

    /**
     * Send referenced array to the stream as-is.
     * Writes sizeof(T)*array_size bytes.
     */
    template<typename T, std::size_t length>
    void send_raw(const T(& array) [length]) {
        send_raw(array, length * sizeof(T));
    }

    /**
     * Send content of referenced vector as-is.
     * Writes sizeof(T)*vector.size() bytes.
     */
    template<typename T, typename A>
    void send_raw(std::vector<T, A> const &vector) {
        send_raw(vector.data(), vector.size() * sizeof(T));
    }

    /**
     * Send given std::string as-is.
     * Writes text.length() bytes.
     */
    void send(const std::string &text) {
        send_raw(text.data(), text.length());
    }

    /**
     * Send given variable in network endianity.
     * Writes sizeof(T) bytes.
     */
    template<class T>
    void send(T t) {
        StreamFormatter::hton(t);
        send_raw(t);
    }

    /**
     * Send content of referenced vector in network endianity.
     * Writes sizeof(T)*vector.size() bytes.
     */
    template<typename T, typename A>
    void send(std::vector<T, A> vector) {
        for(auto &t : vector) {
            StreamFormatter::hton(t);
        }
        send_raw(vector);
    }


    /**
     * Receives given amount of data without converting.
     */
    void recv_raw(void *buf, std::size_t len);

    /**
     * Receives into referenced variable without converting.
     * Reads sizeof(T) bytes.
     */
    template<typename T>
    void recv_raw(T &t) {
        recv_raw(&t, sizeof(T));
    }

    /**
     * Receives into referenced array without converting.
     * Reads sizeof(T)*array_size bytes.
     */
    template<typename T, std::size_t length>
    void recv_raw(T(& array) [length]) {
        recv_raw(array, length * sizeof(T));
    }

    /**
     * Receives into referenced vector without converting.
     * Reads sizeof(T)*vector.size() bytes.
     */
    template<typename T, typename A>
    void recv_raw(std::vector<T, A> &vector) {
        recv_raw(vector.data(), vector.size() * sizeof(T));
    }

    /**
     * Receives into referenced variable converting to host endianity.
     * Reads sizeof(T) bytes.
     */
    template<class T>
    void recv(T &t) {
        recv_raw(t);
        StreamFormatter::ntoh(t);
    }

    /**
     * Receives into referenced array converting to host endianity.
     * Reads sizeof(T)*array_size bytes.
     */
    template<typename T, std::size_t length>
    void recv(T(& array) [length]) {
        recv_raw(array);
        for(auto &t : array) {
            StreamFormatter::ntoh(t);
        }
    }

    /**
     * Receives into referenced vector converting to host endianity.
     * Reads sizeof(T)*vector.size() bytes.
     */
    template<typename T, typename A>
    void recv(std::vector<T, A> &vector) {
        recv_raw(vector);
        for(auto &t : vector) {
            StreamFormatter::ntoh(t);
        }
    }

    /**
     * Receives given amount of bytes and returns them as std::string.
     */
    std::string recv_string(std::size_t length);


    /**
     * Shortcut for recv_raw & send_raw.
     */
    void forward_raw(Stream &output, void *buf, std::size_t len);

    /**
     * Forwards given amount of data directly to given stream without explicit buffer.
     */
    void forward_directly(Stream &output, std::size_t len);

    /**
     * Shortcut for recv_raw & send_raw.
     */
    template<typename T>
    void forward_raw(Stream &output, T &t) {
        forward_raw(output, &t, sizeof(t));
    }

    /**
     * Shortcut for recv_raw & send_raw.
     */
    template<typename T, typename A>
    void forward_raw(Stream &output, std::vector<T,A> &vector) {
        forward_raw(output, vector.data(), vector.size() * sizeof(T));
    }

    /**
     * Effective shortcut for recv & send.
     */
    template<typename T>
    void forward(Stream &output, T &t) {
        forward_raw(output, t);
        StreamFormatter::ntoh(t);
    }

    /**
     * Effective shortcut for recv & send.
     */
    template<typename T, typename A>
    void forward(Stream &output, std::vector<T,A> &vector) {
        forward_raw(output, vector);
        for(auto &t : vector) {
            StreamFormatter::ntoh(t);
        }
    }

    /**
     * Push referenced variable back into receiving buffer.
     * Next recv or forward will retrieve it first.
     */
    template<typename T>
    void push_back(const T &t) {
        if(bufferSize + sizeof(t) > bufferMaxSize) {
            throw std::runtime_error("Stream: Push back buffer overflow.");
        }

        memcpy(&buffer[bufferSize], &t, sizeof(t));
        bufferSize += sizeof(t);
    }

private:
    Stream *m_stream;

    static constexpr std::size_t bufferMaxSize = 1; // TODO: Single byte because that's all we currently we need to push back. But it could change in future.
    uint8_t buffer[bufferMaxSize];
    std::size_t bufferSize = 0;
};

#endif // STREAMFORMATER_H
