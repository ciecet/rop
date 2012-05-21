#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include "Remote.h"
#include "HttpTransport.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;

int base64_decode(char *text, unsigned char *dst, int numBytes);
int base64_encode(char *src, char *text, int numBytes);

void HttpTransport::loop ()
{
    Log l("loop ");
    isLooping = true;
    loopThread = pthread_self();

    struct sockaddr_in servAddr; 
    listenFd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);
    {
        int optval = 1;
        setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR,&optval,sizeof(optval));
    }
    if (bind(listenFd, (struct sockaddr*)&servAddr, sizeof(servAddr))) {
        l.error("bind failed ret:%d\n", errno);
    }
    listen(listenFd, 1);

    registry->lock();
    while (true) {
        handleMessage();
    }
    registry->unlock();
}

void HttpTransport::readRequest ()
{
    int step = 0;
    unsigned char lineBuf[2048];
    unsigned char *cursor = lineBuf;
    string msg;
    int len = 0;
    Log l("xmlHttpRequest ");

    if (xhrBuffer.size) {
        l.fatal("xhrBuffer is not empty:%d\n", xhrBuffer.size);
    }

    while (step != 3) {
        l.info("step:%d\n", step);

        // fill in buffer
        int r;
        if (xhrBuffer.hasWrappedMargin()) {
            iovec io[2];
            io[0].iov_base = xhrBuffer.end();
            io[0].iov_len = xhrBuffer.margin() - xhrBuffer.offset;
            io[1].iov_base = xhrBuffer.buffer;
            io[1].iov_len = xhrBuffer.offset;
            r = readv(xhrFd, io, 2);
        } else {
            r = read(xhrFd, xhrBuffer.end(), xhrBuffer.margin());
        }
        if (r == 0) {
            // eof
            l.debug("eof\n");
            break;
        } if (r < 0) {
            // fail or EWOULDBLOCK
            l.debug("errno:%d\n", errno);
            break;
        }
        xhrBuffer.size += r;
        l.debug("read %d bytes.\n", r);

        switch (step) {
        case 0: // read first line
            while (xhrBuffer.size) {
                char c = xhrBuffer.read();
                if (c == '\r') continue;
                if (c == '\n') {
                    *cursor = 0;
                    cursor = lineBuf;
                    l.debug("got %s\n", lineBuf);
                    if (strncmp((char*)lineBuf, "POST ", 5) != 0) {
                        close(xhrFd);
                        xhrFd = -1;
                        xhrBuffer.reset();
                        return;
                    }
                    step++;
                    break;
                }
                *cursor++ = c;
            }
            if (step == 0) break;

        case 1: // read heads
            while (xhrBuffer.size) {
                char c = xhrBuffer.read();
                if (c == '\r') continue;
                if (c == '\n') {
                    if (cursor == lineBuf) {
                        // on head end
                        step++;
                        if (len > 0) {
                            msg.reserve(len);
                        }
                        break;
                    }
                    *cursor = 0;
                    cursor = lineBuf;
                    l.debug("got %s\n", lineBuf);
                    if (sscanf((char*)lineBuf, "CONTENT-LENGTH:%d",
                            &len) == 1) {
                        l.info("length:%d\n", len);
                    }
                    continue;
                }
                *cursor++ = (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c;
            }
            if (step == 1) break;

        case 2:
            while (xhrBuffer.size) {
                msg.push_back((char)xhrBuffer.read());
            }
            if (msg.length() >= len) {
                step = 3;
            }
        }
    }

    // start pushing data into specified port
    unsigned char *s = reinterpret_cast<unsigned char*>(const_cast<char*>(
            msg.c_str()));
    len = base64_decode((char*)s, s, msg.length());
    if (len < 4) {
        l.warn("aborting... abnormal request\n");
        return;
    }
    int i = 0;
    {
        int p = s[i++];
        p = (p << 8) + s[i++];
        p = (p << 8) + s[i++];
        p = (p << 8) + s[i++];
        xhrPortId = p;
        xhrPort = registry->getPortWithLock(-p);
        l.debug("receiving %d bytes from port:%d\n", len-4, -p);
    }

    if (xhrPort->reader.frame) {
        l.fatal("xhrPort already has reader. :%08x\n", xhrPort->reader.frame);
    }
    while (i < len) {
        while (i < len && xhrBuffer.margin()) {
            l.warn("%d '%c'\n", s[i], s[i]);
            xhrBuffer.write(s[i++]);
        }
        switch (xhrPort->decode(&xhrBuffer)) {
        case Frame::COMPLETE:
            if (i > len) {
                l.error("dropped remaining %d bytes\n", (len - i));
                i = len;
            }
            if (xhrBuffer.size) {
                l.error("dropped remaining %d bytes in buffer\n", xhrBuffer.size);
                xhrBuffer.reset();
            }
            break;
        case Frame::STOPPED:
            break; // continue reading
        case Frame::ABORTED:
        default:
            // clean up
            close(xhrFd);
            xhrFd = -1;
            registry->releasePortWithLock(xhrPort);
            xhrPort = 0;
            return;
        }
    }
}

void HttpTransport::tryRead ()
{
#if 0
    Log l("read ");

    while (true) {

        // fill in buffer
        int r;
        if (inBuffer.hasWrappedMargin()) {
            iovec io[2];
            io[0].iov_base = inBuffer.end();
            io[0].iov_len = inBuffer.margin() - inBuffer.offset;
            io[1].iov_base = inBuffer.buffer;
            io[1].iov_len = inBuffer.offset;
            r = readv(websocketFd, io, 2);
        } else {
            r = read(websocketFd, inBuffer.end(), inBuffer.margin());
        }
        if (r == 0) {
            // eof
            break;
        } if (r < 0) {
            // fail or EWOULDBLOCK
            break;
        }
        inBuffer.size += r;
        l.debug("read %d bytes.\n", r);

begin_message:

        // on start of message frame...
        if (inPort == 0) {
            // need port id and payload length
            if (inBuffer.size < 4) {
                break;
            }
            int p = inBuffer.read();
            p = (p << 8) + inBuffer.read();
            p = (p << 8) + inBuffer.read();
            p = (p << 8) + inBuffer.read();
            inPort = getPortWithLock(-p); // reverse local<->remote port id
            l.debug("getting port:%d\n", -p);
        }

        switch (inPort->decode(&inBuffer)) {
        case Frame::COMPLETE:
            inPort = 0;
            goto begin_message;
        case Frame::STOPPED:
            break; // continue reading
        case Frame::ABORTED:
        default: // not expected.
            // handle it? could be handled in upper layer.
            l.error("Unexpected IO exception.\n");
            return;
        }
    }
#endif
}

void HttpTransport::waitWritable ()
{
#if 0
    fd_set inSet;
    fd_set outfdset;
    fd_set expfdset;
    bool isLoopThread = false;
    int nfds;

    pthread_t self = pthread_self();
    if (pthread_equal(self, loopThread)) {
        isLoopThread = true;
    }

    if (isLoopThread) {
        nfds = ((websocketFd > websocketFd) ? websocketFd : websocketFd ) + 1;
    } else {
        nfds = websocketFd + 1;
    }

    FD_ZERO(&inSet);
    FD_ZERO(&outfdset);
    FD_ZERO(&expfdset);
    FD_SET(websocketFd, &outfdset);
    FD_SET(websocketFd, &expfdset);
    if (isLoopThread) {
        FD_SET(websocketFd, &inSet);
        FD_SET(websocketFd, &expfdset);
    }

    pthread_mutex_unlock(&monitor);
    int ret = pselect(nfds, &inSet, &outfdset, &expfdset, 0, 0);
    pthread_mutex_lock(&monitor);
    if (ret <= 0) {
        return;
    }

    if (FD_ISSET(websocketFd, &expfdset) || FD_ISSET(websocketFd, &expfdset)) {
        return;
    }

    if (FD_ISSET(websocketFd, &inSet)) {
        tryRead();
    }
#endif
}

void HttpTransport::sendResponse ()
{
    string msg;
    unsigned char encoded[4*128+1];
    unsigned char buf[3*128];
    int len;
    FILE *f;
    Log l("sendres ");

    l.debug("xhrBuffer:%d\n", xhrBuffer.size);
    while (xhrPort->writer.frame) {
        xhrPort->encode(&xhrBuffer);
        for (len = 0; len < sizeof(buf) && xhrBuffer.size; len++) {
            buf[len] = xhrBuffer.read();
        }
        l.debug("encoding %d bytes\n", len);
        base64_encode((char*)buf, (char*)encoded, len);
        msg.append((char*)encoded);
    }
    l.info("sending %s\n", msg.c_str());

    f = fdopen(xhrFd, "w");
    fprintf(f, "HTTP/1.1 200 OK\r\n");
    fprintf(f, "Access-Control-Allow-Origin: *\r\n");
    fprintf(f, "Content-Type: text/plain\r\n");
    fprintf(f, "Content-Length: %d\r\n", msg.length());
    fprintf(f, "\r\n");
    fprintf(f, "%s", msg.c_str());
    l.warn("HTTP/1.1 200 OK\r\n");
    l.warn("Access-Control-Allow-Origin: *\n");
    l.warn("Content-Type: text/plain\n");
    l.warn("Content-Length: %d\n", msg.length());
    l.warn("\n");
    l.warn("%s\n", msg.c_str());
    //shutdown(fileno(f), SHUT_WR);
    fclose(f);

    xhrFd = -1;
    registry->releasePortWithLock(xhrPort);
    xhrPort = 0;
}

void HttpTransport::send (Port *p)
{
    Log l("send ");

    l.warn("port:%d\n", p->id);
    if (p->id == -1) {
        l.debug("pending send... waiting for port:-1 open\n");
        while (p != xhrPort) {
            if (isLooping && !pthread_equal(pthread_self(), loopThread)) {
                pthread_cond_wait(&xhrWritable, &registry->monitor);
            } else {
                handleMessage(); // wait until xhr is open for response
            }
        }
        l.debug("ok, now sending...\n");
        sendResponse();
        return;
    }

    while (isSending) {
        l.trace("waiting for lock...\n");
        pthread_cond_wait(&writable, &registry->monitor);
    }
    isSending = true;

    l.debug("sending port:%d...\n", p->id);
    outBuffer.write(p->id >> 24);
    outBuffer.write(p->id >> 16);
    outBuffer.write(p->id >> 8);
    outBuffer.write(p->id);

    while (true) {

        // fill in buffer
        if (p->writer.frame) {
            l.info("... write to buffer\n");
            if (p->encode(&outBuffer) == Frame::ABORTED) {
                // TODO: handle abortion.
                l.error("ABORTED..?\n");
            }
        } else {
            if (outBuffer.size == 0) {
                // all flushed.
                break;
            }
        }

        int w;
        if (outBuffer.hasWrappedData()) {
            iovec io[2];
            io[0].iov_base = outBuffer.begin();
            io[0].iov_len = Buffer::BUFFER_SIZE - outBuffer.offset;
            io[1].iov_base = outBuffer.buffer;
            io[1].iov_len = outBuffer.end() - outBuffer.buffer;
            l.debug("sending message (wrapped)...\n");
            //w = writev(websocketFd, io, 2);
            w = io[0].iov_len + io[1].iov_len;
        } else {
            l.debug("sending message...\n");
            //w = write(websocketFd, outBuffer.begin(), outBuffer.size);
            w = outBuffer.size;
        }
        if (w >= 0) {
            l.debug("sent %d bytes.\n", w);
            outBuffer.drop(w);
            continue;
        }

        if (errno != EWOULDBLOCK) {
            break;
        }

        l.debug("buffer full...\n");
        waitWritable();
    }

    isSending = false;
    pthread_cond_signal(&writable);
}

void HttpTransport::handleMessage ()
{
    Log l("recv ");
    fd_set inSet;
    int nfds = 0;
    FD_ZERO(&inSet);
    if (listenFd >= 0) {
        FD_SET(listenFd, &inSet);
        nfds = (nfds > listenFd+1) ? nfds : listenFd+1;
    }
    if (websocketFd >= 0) {
        FD_SET(websocketFd, &inSet);
        nfds = (nfds > websocketFd+1) ? nfds : websocketFd+1;
    }
    if (xhrFd >= 0) {
        FD_SET(xhrFd, &inSet);
        nfds = (nfds > xhrFd+1) ? nfds : xhrFd+1;
    }

    l.info("blocked for reading. (lsn:%d,ws:%d,req:%d)\n",
            listenFd, websocketFd, xhrFd);

    registry->unlock();
    if (pselect(nfds, &inSet, 0, 0, 0, 0) <= 0) {
        registry->lock();
        return;
    }
    registry->lock();

    if (websocketFd != -1 && FD_ISSET(websocketFd, &inSet)) {
        l.info("reading from websocket...\n");
        // TODO: handle websocket fd

        // execute processors
        for (map<int,Port*>::iterator i = registry->ports.begin();
                i != registry->ports.end();) {
            Port *p = i->second;
            if (!p->requests.empty()) {
                l.info("executing request...\n");
            }
            while (p->processRequest());
            i++;
            registry->releasePortWithLock(p);
        }
    } else {
        if (FD_ISSET(listenFd, &inSet)) {
            if (websocketFd == -1) {
                // TODO: open websocket 
            }
            if (xhrFd != -1) {
                // close previous connection
                close(xhrFd);
                xhrFd = -1;
            }

            l.info("accepting...\n");
            {
                struct sockaddr_in clientAddr;
                socklen_t len = sizeof(clientAddr);
                xhrFd = accept(listenFd, (struct sockaddr*)&clientAddr,
                        &len);
                l.info("xhrFd:%d\n", xhrFd);
            }
        } else if (xhrFd != -1 && FD_ISSET(xhrFd, &inSet)) {
            l.info("reading request...\n");
            readRequest();
            if (xhrPort) {
                l.info("request from port:%d\n", xhrPort->id);
                pthread_cond_signal(&xhrWritable);
                xhrPort->processRequest();
            }
        }
    }
}

void HttpTransport::receive (Port *p)
{
    Log l("waitport ");
    if (isLooping) {
        if (!pthread_equal(pthread_self(), loopThread)) {
            l.debug("waiting on condvar for port:%d\n", p->id);
            pthread_cond_wait(&p->updated, &registry->monitor);
            return;
        }
    }

    handleMessage();
}

void HttpTransport::notifyUnhandledRequest (Port *p)
{
    Log l("nur ");
    if (p != xhrPort) {
        return;
    }
    Request *req = p->requests.front();
    if (req->returnWriter) {
        return;
    }

    l.error("sending empty response\n");
    // close xhr connection for asynchronous call
    FILE *f = fdopen(xhrFd, "w");
    fprintf(f, "HTTP/1.1 200 OK\r\n");
    fprintf(f, "Access-Control-Allow-Origin: *\r\n");
    fprintf(f, "Content-Type: text/plain\r\n");
    fprintf(f, "Content-Length: 0\r\n");
    fprintf(f, "\r\n");
    fclose(f);
    xhrFd = -1;
}

/*------ Base64 Encoding Table ------*/
static const char MimeBase64[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
    'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
    'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
    'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
    'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
    'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
    'w', 'x', 'y', 'z', '0', '1', '2', '3',
    '4', '5', '6', '7', '8', '9', '+', '/'
};

/*------ Base64 Decoding Table ------*/
static int DecodeMimeBase64[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
};

int base64_decode(char *text, unsigned char *dst, int numBytes )
{
    const char* cp;
    int space_idx = 0, phase;
    int d, prev_d = 0;
    unsigned char c;

    space_idx = 0;
    phase = 0;

    for ( cp = text; *cp != '\0'; ++cp ) {
        d = DecodeMimeBase64[(int) *cp];
        if ( d != -1 ) {
            switch ( phase ) {
                case 0:
                    ++phase;
                    break;
                case 1:
                    c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
                    if ( space_idx < numBytes )
                        dst[space_idx++] = c;
                    ++phase;
                    break;
                case 2:
                    c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
                    if ( space_idx < numBytes )
                        dst[space_idx++] = c;
                    ++phase;
                    break;
                case 3:
                    c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
                    if ( space_idx < numBytes )
                        dst[space_idx++] = c;
                    phase = 0;
                    break;
            }
            prev_d = d;
        }
    }
    return space_idx;
}

int base64_encode(char *src, char *text, int numBytes)
{
    unsigned char input[3]  = {0,0,0};
    unsigned char output[4] = {0,0,0,0};
    int   index, i, j, size;
    char *p, *plen;

    plen           = src + numBytes - 1;
    size           = (4 * (numBytes / 3)) + (numBytes % 3? 4 : 0) + 1;
    j              = 0;

    for  (i = 0, p = src;p <= plen; i++, p++) {
        index = i % 3;
        input[index] = *p;

        if (index == 2 || p == plen) {
            output[0] = ((input[0] & 0xFC) >> 2);
            output[1] = ((input[0] & 0x3) << 4) | ((input[1] & 0xF0) >> 4);
            output[2] = ((input[1] & 0xF) << 2) | ((input[2] & 0xC0) >> 6);
            output[3] = (input[2] & 0x3F);

            text[j++] = MimeBase64[output[0]];
            text[j++] = MimeBase64[output[1]];
            text[j++] = index == 0? '=' : MimeBase64[output[2]];
            text[j++] = index <  2? '=' : MimeBase64[output[3]];

            input[0] = input[1] = input[2] = 0;
        }
    }

    text[j] = '\0';

    return size;
}
