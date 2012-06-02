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
#include <fcntl.h>
#include "Remote.h"
#include "HttpTransport.h"
#include "base64.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;

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

void HttpTransport::HttpConnection::readRequest ()
{
    Log &l = self->log;
    bool needClose = false;

    // handle websocket connection
    if (webSocketVersion) {
        if (self->webSocket) {
            // ignore another websocket connectiion.
            close("ignored another websocket");
            return;
        }
        self->webSocket = this;
        return;
    }

    // handle xhr request
    
    // We only accept sync xhr request on specified path.
    // However, browser may request other requests such as icon.
    // In such case, we drop the connection.
    if (strncmp(requestLine.c_str(), "POST /rop/xhr ", 14) != 0) {
        l.warn("Expected POST on /rop/xhr, got %s\n", requestLine.c_str());
        close("unexpected xhr path");
        return;
    }

    // If previous connection is still alive,
    if (self->xmlHttpRequest && self->xmlHttpRequest != this) {
        close("ignored another xhr");
        return;
    }

    self->registry->lock();

    self->xmlHttpRequest = this;

    // Decode base64-encoded input from contentBuffer.
    contentBuffer.ensureMargin(1);
    *contentBuffer.end() = 0;
    int len = base64_decode(reinterpret_cast<char*>(contentBuffer.begin()),
            contentBuffer.begin(), contentBuffer.size());
    contentBuffer.grow(len - contentBuffer.size());

    if (contentBuffer.size() < 4) {
        self->registry->unlock();
        close("invalid content. Too short xhr message");
        return;
    }

    // read pid first.
    int p = contentBuffer.read();
    p = (p << 8) + contentBuffer.read();
    p = (p << 8) + contentBuffer.read();
    p = (p << 8) + contentBuffer.read();
    p = -p; // reverse local<->remote port id

    Port *inPort = self->registry->unsafeGetPort(p);
    l.debug("getting port:%d\n", inPort->id);

    // check if initial xhr request.
    if (self->xhrCallDepth == 0) {
        self->xhrPortId = inPort->id;
    } else {
        // xhr port was not yet finished.
        if (self->xhrPortId != inPort->id) {
            l.error("Expected request to the port:%d, but got %d\n",
                    self->xhrPortId, inPort->id);
            self->registry->releasePort(inPort);
            close("xhr port id mismatch");
            return;
        }
    }

    self->log.info("remaining content buffer to port... %d\n", contentBuffer.size());

    // browser may send xhr having no port message at all
    // merely to get response.
    if (!contentBuffer.size()) {
        requestCount++;
        pthread_cond_signal(&self->xhrWritable);
        self->registry->releasePort(inPort);
        return;
    }

    //bitdump(contentBuffer.begin(), 0, 8);

    // manage xhr call depth.
    switch (contentBuffer.peek(0) >> 6) {
    case 0:
        self->xhrCallDepth++;
        break;
    case 1: case 2:
        if (self->xhrCallDepth == 0) {
            self->log.info("Sending dummy response\n");
            // No need to send response back to client.
            // Send dummy reponse immediately.
            static const uint8_t dres[] = 
                    "HTTP/1.1 200 OK\r\n"
                    "Access-Control-Allow-Origin: *\r\n"
                    "Content-Type: text/plain\r\n"
                    "Content-Length: 0\r\n"
                    "\r\n";
            outBuffer.ensureMargin(sizeof(dres) - 1);
            outBuffer.writeBytes(dres, sizeof(dres) - 1);
            beginWrite();
        }
        break;
    case 3:
        self->xhrCallDepth--;
        ASSERT(self->xhrCallDepth > 0);
        break;
    }

    const char *err = 0;
    switch (inPort->decode(&contentBuffer)) {
    case Frame::COMPLETE:
        requestCount++;
        pthread_cond_signal(&self->xhrWritable);
        break;

    case Frame::STOPPED:
        err = "Expected full port message from xhr.";
        break;

    case Frame::ABORTED:
        err = "Exception occurred during message deserialization.";
        break;

    default: // not expected.
        err = "Unexpected error during the message deserialization.\n";
        break;
    }

    self->registry->releasePort(inPort);
    if (err) {
        close(err);
    }
}

// Each frame starts with port number followed by input stream to the port. 
void HttpTransport::HttpConnection::readFrame ()
{
    Log &l = self->log;
    l.info("reading websocket frame\n");

    if (contentBuffer.size() < 4) {
        close("invalid content.");
        return;
    }

    // read pid first.
    int p = contentBuffer.read();
    p = (p << 8) + contentBuffer.read();
    p = (p << 8) + contentBuffer.read();
    p = (p << 8) + contentBuffer.read();
    p = -p; // reverse local<->remote port id

    Port *inPort = self->registry->getPort(p);
    l.debug("getting port:%d\n", inPort->id);

    const char *err = 0;
    switch (inPort->decode(&contentBuffer)) {
    case Frame::COMPLETE:
    case Frame::STOPPED:
        break;

    case Frame::ABORTED:
        err = "Exception occurred during message deserialization.";
        break;

    default: // not expected.
        err = "Unexpected error during the message deserialization.";
        break;
    }

    self->registry->releasePort(inPort);
    if (err) {
        close(err);
    }
}

// Sends a frame if any pending.
// The frame begins with port number followed by output stream from the port.
// If the output stream is too long, we divide them into several frames.
void HttpTransport::HttpConnection::writeFrame ()
{
    Port *p;
    Log &l = self->log;
    l.info("writing websocket frame\n");
    self->registry->lock();

    if (!self->sendingPorts) {
        l.info("nothing to write.\n");
        goto exit;
    }

    p = self->sendingPorts;
    ASSERT(self->portBuffer.size() == 0);

    l.debug("sending port:%d...\n", p->id);
    self->portBuffer.write(p->id >> 24);
    self->portBuffer.write(p->id >> 16);
    self->portBuffer.write(p->id >> 8);
    self->portBuffer.write(p->id);

    switch (p->encode(&self->portBuffer)) {
    case Frame::COMPLETE:
        self->sendingPorts = p->next;
        if (!p->next) {
            self->sendingPortsTail = &self->sendingPorts;
        }
        pthread_cond_signal(&p->updated);
        break;

    case Frame::STOPPED:
        break;

    default:
        l.error("Unexpected error during the message serialization.\n");
        goto exit;
    }

    sendFrame(OPCODE_TEXT, self->portBuffer.begin(), self->portBuffer.size());

exit:
    self->portBuffer.reset();
    self->registry->unlock();
}

void HttpTransport::HttpConnection::close (const char *msg)
{
    if (this == self->webSocket) {
        self->log.info("websocket closed.\n");
        self->stop(); // this closes all connections including this.
        return;
    }

    if (this == self->xmlHttpRequest) {
        self->registry->lock();
        self->log.info("xhr closed.\n");
        self->xmlHttpRequest = 0;
        self->registry->unlock();
    }

    // exclude this from connection list.
    for (HttpConnection **phc = &self->connections; *phc; phc = &(*phc)->next) {
        if (*phc == this) {
            *phc = next;
            break;
        }
    }

    self->log.warn("Connection closed by: %s\n", msg);
    ::close(fd);
    self->eventDriver->remove(this);
    delete this;
}

void HttpTransport::HttpServer::read ()
{
    // allocate new connection and add into connection list.
    HttpConnection *hc = new HttpConnection(self);
    struct sockaddr_in clientAddr;
    socklen_t len = sizeof(clientAddr);
    hc->fd = accept(fd, (struct sockaddr*)&clientAddr, &len);
    if (!hc->fd) {
        self->log.error("Accept failed.\n");
        delete hc;
        return;
    }
    hc->next = self->connections;
    self->connections = hc;

    // make non-block socket and activate.
    fcntl(hc->fd, F_SETFL, O_NONBLOCK);
    self->log.info("new connection:%d\n", hc->fd);
    eventDriver->add(hc);
}

void HttpTransport::start (int port, EventDriver *ed)
{
    eventDriver = ed;

    struct sockaddr_in servAddr; 
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    server.fd = socket(AF_INET, SOCK_STREAM, 0);
    {
        int optval = 1;
        setsockopt(server.fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                sizeof(optval));
    }
    if (bind(server.fd, (struct sockaddr*)&servAddr, sizeof(servAddr))) {
        log.error("bind failed ret:%d\n", errno);
    }
    listen(server.fd, 1);
    ed->add(&server);
    log.info("listening on port %d with socket:%d\n", port, server.fd);
}

void HttpTransport::stop ()
{
    while (connections) {
        ::close(connections->fd);
        eventDriver->remove(connections);
        connections = connections->next;
    }
    eventDriver = 0;
}

void HttpTransport::sendResponse (Port *p)
{
    //log.debug("xhr response:\n");
    ASSERT(portBuffer.size() == 0);
    bool isEventThread = eventDriver->isRunningThread();
    //ASSERT(out.size() == 0);

    // It's possible that we're trying to send response before getting request.
    // if then, wait until requestCount becomes non-zero.
    while (!(xmlHttpRequest && xmlHttpRequest->requestCount > 0)) {
        if (isEventThread) {
            registry->unlock();
            eventDriver->handleEvent();
            registry->lock();
        } else {
            pthread_cond_wait(&xhrWritable, &registry->monitor);
        }
    }
    Buffer &out = xmlHttpRequest->outBuffer;
    ASSERT(out.size() == 0);

    // Writes output from the port directly into the xmlHttpRequest.outBuffer
    // regardless of calling thread since this is the only writer holding lock.
    while (p->writer.frame) {
        switch (p->encode(&portBuffer)) {
        case Frame::COMPLETE:
            break;

        case Frame::STOPPED:
            portBuffer.ensureMargin(512); // heuristic value
            continue;

        default:
        case Frame::ABORTED:
            log.error("Exception occurred during message serialization.\n");
            portBuffer.reset();
            break;
        }
    }
    int len64 = (portBuffer.size() + 2) / 3 * 4; // base64 encoded length 
    uint8_t head = portBuffer.peek(0); // peek head byte for later use.

    char buf[512];
    int l = sprintf(buf,
            "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: %d\r\n"
            "\r\n", len64);
    out.ensureMargin(l+len64);
    out.writeBytes(reinterpret_cast<uint8_t*>(buf), l);
    base64_encode(portBuffer.begin(), reinterpret_cast<char*>(out.end()),
            portBuffer.size());
    out.grow(len64);
    portBuffer.reset();

    out.ensureMargin(1);
    *out.end() = 0;
    log.trace("%s\n", out.begin());

    if (isEventThread) {
        xmlHttpRequest->requestCount--;
        registry->unlock(); // TODO: chagne to recursive lock to remove it.
        xmlHttpRequest->beginWrite();
        registry->lock();

        // run nested event driver until sending complete.
        // if beginWrite() was sufficient to send over socket,
        // p->writer.frame becomes 0, and this returns immediately.
        while (p->writer.frame) {
            registry->unlock();
            eventDriver->handleEvent();
            registry->lock();
        }
    } else {
        struct WriteTrigger: AsyncReactor {
            HttpTransport *self;
            void processEvent (int e) {
                self->xmlHttpRequest->beginWrite();
                delete this;
            }
        } *wt = new WriteTrigger();

        wt->self = this;
        eventDriver->add(wt);
        xmlHttpRequest->requestCount--;

        // wait until event loop completes sending.
        while (p->writer.frame) {
            pthread_cond_wait(&p->updated, &registry->monitor);
        }
    }

    // trace call depth.
    // reaching 0 means that there will be no additional sends via xhr.
    switch (head >> 6) {
    case 0:
        xhrCallDepth++;
        break;
    case 3:
        xhrCallDepth--;
        break;
    }
}

void HttpTransport::send (Port *p)
{
    if (xhrCallDepth > 0 && p->id == xhrPortId) {
        sendResponse(p);
        return;
    }

    *sendingPortsTail = p;
    sendingPortsTail = &p->next;

    if (eventDriver && eventDriver->isRunningThread()) {
        if (sendingPorts == p) {
            registry->unlock(); // TODO: chagne to recursive lock to remove it.
            webSocket->beginWrite();
            registry->lock();
            // if beginWrite() was sufficient to send over socket,
            // p->writer.frame becomes 0, and this send() return immediately.
        }

        // run nested event driver until sending complete.
        while (p->writer.frame) {
            eventDriver->handleEvent();
        }
    } else {
        struct WriteTrigger: AsyncReactor {
            HttpTransport *self;
            void processEvent (int e) {
                self->webSocket->beginWrite();
                delete this;
            }
        } *wt;

        if (sendingPorts == p) {
            // try to write directly in this thread.
            registry->unlock(); // TODO: chagne to recursive lock to remove it.
            webSocket->write();
            registry->lock();

            if (sendingPorts == p) {
                wt = new WriteTrigger();
                wt->self = this;
                eventDriver->add(wt);
            }
        }

        // wait until event loop completes sending.
        while (p->writer.frame) {
            pthread_cond_wait(&p->updated, &registry->monitor);
        }
    }
}

void HttpTransport::receive (Port *p)
{
    if (eventDriver && eventDriver->isRunningThread()) {
        eventDriver->handleEvent();
    } else {
        pthread_cond_wait(&p->updated, &registry->monitor);
    }
}

static void *processRequests (void *arg)
{
    Port *p = reinterpret_cast<Port*>(arg);
    p->registry->lock();

    while (p->processRequest());
    p->isProcessingRequests = false;

    p->registry->unlock();
    return 0;
}

void HttpTransport::notifyUnhandledRequest (Port *p)
{
    if (p->isProcessingRequests) {
        return;
    }

    p->isProcessingRequests = true;
    pthread_create(&p->processingThread, 0, processRequests, p);
}
