// Websocket implementaion was based on Hiroshi Ichikawa's websocket-ruby.

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
#include <string>
#include <stdio.h>
#include <openssl/sha.h>
#include "base64.h"
#include "Http.h"

using namespace std;
using namespace base;

static void apply_mask(uint8_t *payload, int len, uint8_t *mask) {
    for (int i = 0; i < len; i++) {
        payload[i] ^= mask[i % 4];
    }
}

static string makeAcceptString (string key)
{
    uint8_t msg[20];
    uint8_t encoded[29];
    key.append("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    SHA1((uint8_t*)key.c_str(), key.length(), msg);
    base64_encode(msg, (char*)encoded, 20);
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

void HttpReactor::read ()
{
reread:
    // fill in buffer
    inBuffer.moveFront();
    int r = ::read(fd, inBuffer.end(), inBuffer.margin());
    log.debug("read from fd:%d returned %d\n", fd, r);
    if (r == 0) {
        // stop reading
        // changeMask(mask & ~EVENT_READ);
        close("Received EOF");
        return;
    } else if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }

        close(strerror(errno));
        return;
    }
    inBuffer.grow(r);

reswitch:
    switch (state) {

    case BEGIN_READ:
        requestLine.clear();
        contentBuffer.reset();
        headers.clear();
        length = 0;
        state = READ_HEADS;
        // fall through

    case READ_HEADS:
        while (true) {
            uint8_t *p = inBuffer.begin();
            for (; p < inBuffer.end(); p++) {
                if (*p == '\r') {
                    *p = 0;
                    continue;
                }
                if (*p == '\n') {
                    *p = 0;
                    break;
                }
            }
            if (p == inBuffer.end()) {
                if (inBuffer.size() == inBuffer.capacity()) {
                    close("Too long header line.");
                    return;
                }
                if (r > 0) goto reread;
                return;
            }

            char *line = reinterpret_cast<char*>(inBuffer.begin());
            inBuffer.drop(p - inBuffer.begin() + 1);

            // empty line. end of head.
            if (line[0] == 0) {
                break;
            }

            // read request line
            if (requestLine.size() == 0) {
                requestLine = line;
                log.trace("request %s\n", line);
                continue;
            }

            // normalize header key
            char *s = line;
            while (true) {
                char c = *s;
                if (!c || c == ':') break;
                *s++ = (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
            }
            if (!*s) {
                log.error("Invalid header:%s\n", line);
                continue;
            }
            do {
                *s++ = 0;
            } while (*s == ' ');
            log.trace("header %s: %s\n", line, s);
            headers[line] = s;
        }

        if (strcasecmp(headers["UPGRADE"].c_str(), "websocket") == 0 &&
                strcasestr(headers["CONNECTION"].c_str(), "upgrade") != 0) {
            if (headers.find("SEC-WEBSOCKET-VERSION") != headers.end()) {
                webSocketVersion = atoi(
                        headers["SEC-WEBSOCKET-VERSION"].c_str());
            } else if (headers.find("SEC-WEBSOCKET-KEY1") != headers.end() &&
                    headers.find("SEC-WEBSOCKET-KEY2") != headers.end()) {
                webSocketVersion = -76; // hixie-76
            }
        }

        if (webSocketVersion == 0) {
            if (headers.find("CONTENT-LENGTH") != headers.end()) {
                length = atoll(headers["CONTENT-LENGTH"].c_str());
            }
            state = READ_BODY;
            goto reswitch;
        } else if (webSocketVersion == -76) {
            state = READ_WEBSOCKET_KEY3;
            goto reswitch;
        }

        // do handshake of websocket (rfc6455)
        {
            string reply = 
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Accept: ";
            reply += makeAcceptString(headers["SEC-WEBSOCKET-KEY"]);
            reply += "\r\n\r\n";
            log.debug("sending... %s\n", reply.c_str());
            outBuffer.ensureMargin(reply.length());
            outBuffer.writeBytes(reinterpret_cast<const uint8_t*>(reply.data()),
                    reply.length());
            beginWrite();
        }
        state = READ_WEBSOCKET;
        readRequest();
        return;

    case READ_WEBSOCKET:
        {
            if (inBuffer.size() < 2) {
                if (r > 0) goto reread;
                return;
            }
            int h0 = inBuffer.peek(0);
            int h1 = inBuffer.peek(1);
            int minread = 2;
            //bitdump(buf, 0, 16);

            //bool fin = (h0 & 0x80) != 0;
            int op = h0 & 0x0f;
            bool mask = (h1 & 0x80) != 0;
            if (mask) {
                minread += 4;
            } else {
                close("received unmasked frame.");
                return;
            }
            int64_t plen = h1 & 0x7f;
            if (plen == 126) {
                minread += 2;
            } else if (plen == 127) {
                minread += 8;
            }
            if (inBuffer.size() < minread) {
                if (r > 0) goto reread;
                return;
            }
            inBuffer.drop(2);

            if (plen == 126) {
                plen = ((inBuffer.read()&0xff) << 8) | inBuffer.read();
            } else if (plen == 127) {
                plen = inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
                plen = (plen << 8) | inBuffer.read();
            }
            length = plen;
            inBuffer.readBytes(frameMask, 4);
            contentBuffer.reset();
            contentBuffer.ensureMargin(plen);
        }
        state = READ_WEBSOCKET_PAYLOAD;
        // fall through

    case READ_WEBSOCKET_PAYLOAD:
        if (length > 0) {
            int len = (length < inBuffer.size()) ? length : inBuffer.size();
            contentBuffer.writeBytes(inBuffer.begin(), len);
            inBuffer.reset();
            length -= len;
        }
        if (length > 0) {
            if (r > 0) goto reread;
            return;
        }
        apply_mask(contentBuffer.begin(), contentBuffer.size(), frameMask);

        switch (opcode) {
        case OPCODE_TEXT:
            {
                // decode base64 encoded string
                contentBuffer.ensureMargin(1);
                *contentBuffer.end() = 0;
                int len = base64_decode(
                        reinterpret_cast<char*>(contentBuffer.begin()),
                        contentBuffer.begin(), contentBuffer.size());
                contentBuffer.grow(len - contentBuffer.size());
            }
            // fall through
        case OPCODE_BINARY:
            state = READ_WEBSOCKET;
            readFrame();
            return;

        case OPCODE_CLOSE:
            close("websocket closed by peer.\n");
            return;

        case OPCODE_PING:
            log.warn("ping not supported.\n");
            break;

        case OPCODE_PONG:
            log.warn("pong not supported.\n");
            break;

        default:
            printf("unknown opcode:%d\n", opcode);
            break;
        }
        state = READ_WEBSOCKET;
        goto reswitch;

    case READ_WEBSOCKET_KEY3:
        if (inBuffer.size() < 8) {
            if (r > 0) goto reread;
            return;
        }
        inBuffer.readBytes(key3, 8);
        {
            string reply = 
                    "HTTP/1.1 101 Switching Protocols\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: Upgrade\r\n"
                    "Sec-WebSocket-Origin: http://localhost\r\n"
                    "Sec-WebSocket-Location: ws://localhost\r\n";
            string keys;
            keys = headers["SEC-WEBSOCKET-KEY1"]; 
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
        state = READ_WEBSOCKET_76;
        readRequest();
        return;

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

    case READ_BODY:
        if (length) {
            int s = inBuffer.size();
            contentBuffer.ensureMargin(s);
            contentBuffer.writeBytes(inBuffer.begin(), s);
            inBuffer.reset();
            length -= s;
        }
        if (length) {
            if (r > 0) goto reread;
            return;
        }
        state = BEGIN_READ;
        readRequest();
        return;

    default:
        log.error("Illegal state:%d\n", state); 
        return;
    }
}

void HttpReactor::write () {
refill:
    if (!outBuffer.size()) {
        outBuffer.moveFront();
        if (webSocketVersion) {
            writeFrame(); // fill-in more frame data
        } else {
            writeResponse(); // fill-in more response data
        }
        if (!outBuffer.size()) {
            // finish writing.
            changeMask(mask & ~EVENT_WRITE);
            return;
        }
    }

rewrite:
    int w = ::write(fd, outBuffer.begin(), outBuffer.size());
    if (w > 0) {
        log.debug("sent %d bytes\n", w);
        outBuffer.drop(w);
        goto refill;
    } else if (w < 0) {
        if (w == EAGAIN || w == EWOULDBLOCK) {
            log.debug("EAGAIN\n");
            return;
        }
        close("websocket closed by peer.");
        return;
    } else {
        goto rewrite;
    }
}

void HttpReactor::sendFrame (OPCODE op, uint8_t *msg, int64_t len)
{
    if (webSocketVersion == -76) {
      //data = force_encoding(data.dup(), "ASCII-8BIT")
      //write("\x00#{data}\xff")
      //flush()
      return;
    }

    // use base64 for text encoding.
    int64_t plen = (op == OPCODE_TEXT) ? (len + 2) / 3 * 4 : len;

    uint8_t buf[10];
    buf[0] = 0x80 | op;
    buf[1] = 0; // 0x80; // masked
    if (plen < 126) {
        buf[1] |= plen;
        bitdump(buf+1, 0, 8);
        outBuffer.ensureMargin(2 + plen);
        outBuffer.writeBytes(buf, 2);
    } else if (plen < 65536) {
        buf[1] |= 126;
        buf[2] = plen >> 8;
        buf[3] = plen;
        outBuffer.ensureMargin(4 + plen);
        outBuffer.writeBytes(buf, 4);
    } else {
        buf[1] |= 127;
        buf[2] = plen >> 56;
        buf[3] = plen >> 48;
        buf[4] = plen >> 40;
        buf[5] = plen >> 32;
        buf[6] = plen >> 24;
        buf[7] = plen >> 16;
        buf[8] = plen >> 8;
        buf[9] = plen;
        outBuffer.ensureMargin(10 + plen);
        outBuffer.writeBytes(buf, 10);
    }
    if (op == OPCODE_TEXT) {
        int l = base64_encode(msg, (char*)outBuffer.end(), len);
        ASSERT(l == plen);
        outBuffer.grow(plen);
    } else {
        outBuffer.writeBytes(msg, plen);
    }

    log.fatal("Wrote %d bytes in a new frame\n", outBuffer.size()); 
}
