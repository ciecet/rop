// Websocket implementaion was based on Hiroshi Ichikawa's websocket-ruby.
// https://github.com/gimite/web-socket-ruby

#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <map>
#include <sstream>
#include <string>
#include <stdio.h>

#include "openssl/sha.h"
#include "base64.h"
#include "HttpReactor.h"
#include "ScopedLock.h"

using namespace std;
using namespace acba;

static void apply_mask (uint8_t *payload, int len, uint8_t *mask) {
    for (int i = 0; i < len; i++) {
        payload[i] ^= mask[i % 4];
    }
}

static string makeAcceptString (string key)
{
    uint8_t msg[20];
    uint8_t encoded[29];
    key.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

    // digest key.
    {
        SHA_CTX c;
        SHA1_Init(&c);
        SHA1_Update(&c, key.c_str(), key.length());
        SHA1_Final(msg, &c);
    }

    int len = base64_encode(msg, (char*)encoded, 20);
    encoded[len] = 0;
    return (char*)encoded;
}

static void bitdump (uint8_t *s, int off, int len) {
    while (len-- > 0) {
        int i = off / 8;
        int j = off % 8;
        off++;
        printf("%c", (s[i]&(1<<(7-j))) ? '1' : '0');
        if (j == 7) {
            printf(" | ");
        }
    }
    printf("\n");
}

void HttpReactor::send (const uint8_t *buf, int32_t len)
{
    ScopedLock sl(mutex);

    if (outBuffer.size()) {
        // already being sent.
        outBuffer.writeBytes(buf, len);
        return;
    }

    outBuffer.writeBytes(buf, len);
    if (!tryWrite()) {
        if (getEventDriver()->isEventThread()) {
            // assume that connection will be closed soon.
            return;
        } else {
            assert(!"write failed");
        }
    }

    // continue writing only when outBuffer has remaining data
    if (outBuffer.size()) setWritable(true);
}

void HttpReactor::send (const string &str) {
    send(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

void HttpReactor::sendFrame (const uint8_t *msg, int32_t len, bool isText)
{
    if (websocketVersion <= 0) {
        assert(!"Not Supported");
    }

    // work-around for WEBMW-325
    // if (!isText) return sendFrameAsText(msg, len);

    OPCODE op = isText ? OPCODE_TEXT : OPCODE_BINARY;

    Buffer buf;
    buf.writeI8(0x80 | op);
    if (len < 126) {
        buf.writeI8(len);
    } else if (len < 65536) {
        buf.writeI8(126);
        buf.writeI16(len);
    } else {
        buf.writeI8(127);
        buf.writeI64(len);
    }

    buf.writeBytes(msg, len);

    send(buf.begin(), buf.size());
    log.debug("sent a frame (%d bytes).\n", buf.size());
}

void HttpReactor::sendFrameAsText (const uint8_t *msg, int32_t len)
{
    if (websocketVersion <= 0) {
        assert(!"Not Supported");
    }

    // hard-coded at this time.
    OPCODE op = OPCODE_TEXT;

    Buffer buf;

    // use base64 for text encoding.
    int64_t plen = (op == OPCODE_TEXT) ? (len + 2) / 3 * 4 : len;
    buf.writeI8(0x80 | op);
    if (plen < 126) {
        buf.writeI8(plen);
    } else if (plen < 65536) {
        buf.writeI8(126);
        buf.writeI16(plen);
    } else {
        buf.writeI8(127);
        buf.writeI64(plen);
    }

    if (op == OPCODE_TEXT) {
        buf.ensureMargin(plen);
        int l = base64_encode(msg, (char*)buf.end(), len);
        assert(l == plen);
        buf.grow(l);
    } else {
        buf.writeBytes(msg, plen);
    }

    send(buf.begin(), buf.size());
    log.debug("sent a frame (%d bytes).\n", buf.size());
}

void HttpReactor::sendResponse (int32_t status, const std::string &mimetype,
        const std::string &content)
{
    stringstream ss;
    ss << "HTTP/1.1 " << status << " ";
    switch (status) {
    case 200: ss << "OK"; break;
    case 404: ss << "Not Found"; break;
    default: ss << "Unknown Code"; break;
    }
    ss << "\r\n";
    ss << "Content-Type: " << mimetype << "\r\n";
    ss << "Content-Length: " << content.length() << "\r\n";
    ss << "Access-Control-Allow-Origin: *\r\n";
    ss << "\r\n";
    ss << content;
    send(ss.str());
}

bool HttpReactor::parseHeader ()
{
    // check if in-buffer contains complete http heads.
    int n = 0;
    char *s = reinterpret_cast<char*>(inBuffer.begin());
    char *end = reinterpret_cast<char*>(inBuffer.end());
    for (; s < end; s++) {
        if (*s == 0x0d) continue;
        if (*s == 0x0a) {
            n++;
            if (n == 2) {
                end = s + 1;
                s = reinterpret_cast<char*>(inBuffer.begin());
                inBuffer.drop(end - s);
                break;
            }
        } else {
            n = 0;
        }
    }
    if (n != 2) return false;

    // string from s to end(exclusive)
    char *colon = 0;
    char *start = 0;
    for (; s < end; s++) {
        if (!start) {
            if (*s != 0x0d && *s != 0x0a) {
                start = s;
            }
            continue;
        }
        if (!colon && *s == ':') {
            colon = s;
            continue;
        }
        if (*s != '\r' && *s != '\n') continue;
        *s = '\0';

        // a line starts from start
        if (requestLine.empty()) {
            requestLine = start;
            log.trace("REQ %s\n", start);
            colon = 0;
            start = 0;
            size_t f = requestLine.find(" ");
            size_t s = requestLine.find(" ", f + 1);
            if (f != string::npos && s != string::npos) {
                requestMethod = requestLine.substr(0, f);
                requestPath = requestLine.substr(f + 1, s - (f + 1));
                requestProtocol = requestLine.substr(s + 1);
            }
            continue;
        }

        if (!colon) {
            log.warn("header field without colon:%s\n", start);
            continue;
        }

        // normalize header key
        do {
            *colon++ = '\0';
        } while (*colon == ' ');
        for (char *t = start; *t; t++) {
            char c = *t;
            if (c >= 'A' && c <= 'Z') {
                *t = c + ('a' - 'A');
            }
        }
        log.trace("REQ %s: %s\n", start, colon);
        headers[start] = colon;
        colon = 0;
        start = 0;
    }

    if (headers.find("content-length") != headers.end()) {
        contentLength = atoi(headers["content-length"].c_str());
    }
    return true;
}

bool HttpReactor::handshake () {
    if (strcasecmp(headers["upgrade"].c_str(), "websocket") != 0) return true;

    // determine websocket version
    if (headers.find("sec-websocket-version") != headers.end()) {
        websocketVersion = atoi(headers["sec-websocket-version"].c_str());
    } else if (headers.find("sec-websocket-key1") != headers.end() &&
            headers.find("sec-websocket-key2") != headers.end()) {
        websocketVersion = -76; // hixie-76
    }
    if (websocketVersion == 0) return true;

    if (websocketVersion < 0) { // hixie-76
        assert(!"Not Supported websocket version");
        uint8_t key3[8];
        inBuffer.readBytes(key3, 8);
        {
            string reply =
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Origin: http://localhost\r\n"
                    "Sec-WebSocket-Location: ws://localhost\r\n";
            string keys;
            keys = headers["sec-websocket-key1"];
            // write md5
            // def hixie_76_security_digest(key1, key2, key3)
            // bytes1 = websocket_key_to_bytes(key1)
            // bytes2 = websocket_key_to_bytes(key2)
            // def websocket_key_to_bytes(key)
              // num = key.gsub(/[^\d]/n, "").to_i() / key.scan(/ /).size
              // return [num].pack("N")
            // end
            // return Digest::MD5.digest(bytes1 + bytes2 + key3)
            reply += "\r\n\r\n";
            //send(reply);
        }
        return true;
    } else { // rfc6455
        string reply =
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                "Sec-WebSocket-Accept: ";
        reply += makeAcceptString(headers["sec-websocket-key"]);
        reply += "\r\n\r\n";
        log.debug("sending... %s\n", reply.c_str());
        send(reply);
        return true;
    }
}

bool HttpReactor::readWebSocketHeader ()
{
    if (inBuffer.size() < 2) {
        return false;
    }
    int h0 = inBuffer.peek(0);
    int h1 = inBuffer.peek(1);
    int minread = 2;
#if ROP_DEBUG
    bitdump(inBuffer.begin(), 0, 16);
#endif

    //bool fin = (h0 & 0x80) != 0;
    opcode = OPCODE(h0 & 0x0f);
    bool mask = (h1 & 0x80) != 0;
    if (mask) {
        minread += 4;
    } else {
        abort("received unmasked frame.");
        return false;
    }
    int64_t plen = h1 & 0x7f;
    if (plen == 126) {
        minread += 2;
    } else if (plen == 127) {
        minread += 8;
    }
    if (inBuffer.size() < minread) {
        return false;
    }
    inBuffer.drop(2);

    if (plen == 126) {
        plen = inBuffer.readI16();
    } else if (plen == 127) {
        plen = inBuffer.readI64();
    }
    contentLength = plen;
    inBuffer.readBytes(frameMask, 4);
    return true;
}

bool HttpReactor::tryWrite ()
{
    int fd = getFd();
    if (fd < 0) {
        log.debug("fd already closed\n");
        return false;
    }

    while (true) {
        if (outBuffer.size() == 0) break;
        int w = ::send(fd, outBuffer.begin(), outBuffer.size(), (
                MSG_DONTWAIT | MSG_NOSIGNAL));
        if (w < 0) {
            if (w == EAGAIN || w == EWOULDBLOCK) return true;
            log.debug("Write failed\n");
            return false;
        }
        outBuffer.drop(w);
        outBuffer.moveToFront();
    }
    return true;
}

void HttpReactor::read ()
{
    int fd = getFd();
    // fill in buffer
    while (true) {
        inBuffer.ensureMargin(1);
        int r = ::recv(fd, inBuffer.end(), inBuffer.margin(), MSG_DONTWAIT);
        log.debug("read from fd:%d returned %d\n", fd, r);
        if (r == 0) {
            // stop reading
            // changeMask(mask & ~EVENT_READ);
            abort("Received EOF");
            return;
        }
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            abort(strerror(errno));
            return;
        }
        inBuffer.grow(r);
    }

handle_message:

    switch (state) {

    case BEGIN_READ:
        requestLine.clear();
        contentBuffer.clear();
        contentLength = -1;
        headers.clear();
        state = READ_HEADS;
        // fall through

    case READ_HEADS:
        if (!parseHeader()) return; // header is incomplete.
        state = TRY_HANDSHAKE;
        // fall through

    case TRY_HANDSHAKE:
        if (!handshake()) return; // handshake is incomplete.

        // if this is websocket
        if (websocketVersion) {
            state = (websocketVersion > 0) ? READ_WEBSOCKET :
                READ_WEBSOCKET_76;
            readRequest();
            break;
        }

        if (contentLength == -1) {
            state = BEGIN_READ;
            readRequest();
            break;
        }

        state = READ_BODY;
        // fall through

    case READ_BODY:
        if (inBuffer.size() < contentLength) return;

        // decode base64-encoded payload to contentBuffer
        {
            int est = (contentLength + 3) / 4 * 3;
            contentBuffer.clear();
            contentBuffer.ensureMargin(est);
            string s;
            s.assign(reinterpret_cast<char*>(inBuffer.begin()), contentLength);
            int len = base64_decode(reinterpret_cast<char*>(inBuffer.begin()),
                    contentBuffer.end(), contentLength);
            s.assign(reinterpret_cast<char*>(inBuffer.begin()), contentLength);
            inBuffer.drop(contentLength);
            inBuffer.moveToFront();
            contentBuffer.grow(len);
            contentLength = len;

            state = BEGIN_READ;
            readRequest();
            break;
        }

    case READ_WEBSOCKET:
        if (!readWebSocketHeader()) return;
        state = READ_WEBSOCKET_PAYLOAD;
        // fall through

    case READ_WEBSOCKET_PAYLOAD:
        if (inBuffer.size() < contentLength) return;

        apply_mask(inBuffer.begin(), contentLength, frameMask);
        state = READ_WEBSOCKET;

        switch (opcode) {
        case OPCODE_TEXT:
            // decode base64-encoded payload to contentBuffer
            {
                int est = (contentLength + 3) / 4 * 3;
                contentBuffer.clear();
                contentBuffer.ensureMargin(est);
                int len = base64_decode(
                        reinterpret_cast<char*>(inBuffer.begin()),
                        contentBuffer.end(), contentLength);
                inBuffer.drop(contentLength);
                inBuffer.moveToFront();
                contentBuffer.grow(len);
                contentLength = len;
                readFrame();
                break;
            }

        case OPCODE_BINARY:
            contentBuffer.clear();
            contentBuffer.writeBytes(inBuffer.begin(), contentLength);
            inBuffer.drop(contentLength);
            readFrame();
            break;

        case OPCODE_CLOSE:
            abort("websocket closed by peer.\n");
            return;

        case OPCODE_PING:
            log.warn("ping not supported.\n");
            break;

        case OPCODE_PONG:
            log.warn("pong not supported.\n");
            break;

        default:
            log.error("unknown opcode:%d\n", opcode);
            break;
        }
        break;

    case READ_WEBSOCKET_76:
        /*
        packet = gets("\xff")
        return nil if !packet
        if packet =~ /\A\x00(.*)\xff\z/nm
        return force_encoding($1, "UTF-8")
      elsif packet == "\xff" && read(1) == "\x00" # closing
        close(1005, "", :peer)
        return nil
      else
        raise(WebSocket::Error, "input must be either '\\x00...\\xff' or '\\xff\\x00'")
      end
      */
        break;

    case READ_WEBSOCKET_PAYLOAD_76:
        log.error("Unsupported websocket version\n");
        break;

    default:
        log.error("Illegal state:%d\n", state);
        break;
    }

    if (getFd() != -1) goto handle_message;
}

void HttpReactor::write () {
    ScopedLock sl(mutex);

    if (!tryWrite()) {
        abort("write fail\n");
        return;
    }

    if (outBuffer.size() == 0) setWritable(false);
}
