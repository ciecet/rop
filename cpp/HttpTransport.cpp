#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/uio.h>
#include "rop.h"
#include "HttpTransport.h"
#include "HttpReactor.h"
#include "base64.h"
#include "Log.h"
#include "ScopedLock.h"

using namespace std;
using namespace acba;
using namespace rop;

static const string createSessionId ()
{
    static const int LEN = 32; // create 32 byte string.
    static const char digits[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    char s[LEN+1];
    {
        timeval t;
        gettimeofday(&t, 0);
        srand(t.tv_usec);
    }
    for (int i = 0; i < LEN; i++) {
        s[i] = digits[rand() % (sizeof(digits) - 1)];
    }
    s[LEN] = 0;
    return s;
}

void HttpConnection::readRequest ()
{
    string sid;
    HttpTransport *trans = 0;

    // if this connection is a websocket,
    if (websocketVersion) {
        if (requestPath.compare(0, 2, "/r") != 0) {
            log.warn("Unsupported websocket path:%s\n", requestPath.c_str());
            abort();
            return;
        }

        // (can be moved to other place if necessary)
        server->closeStaleTransports();

        if (requestPath.length() > 2 && requestPath[2] == '/') {
            // use already existing transport
            sid = requestPath.substr(3);
            map<string, HttpTransport*>::iterator ti =
                    server->transports.find(sid);
            if (ti != server->transports.end()) {
                trans = ti->second;
            }
            if (!trans || trans->hasWebsocket()) {
                if (trans) {
                    log.warn("Duplicated websocket connection\n");
                } else {
                    log.warn("No transport found for the websocket\n");
                }
                abort();
                return;
            }
            assert(trans->sessionId == sid);
        } else {
            // create websocket-initiated transport
            do {
                sid = createSessionId();
            } while (server->transports.find(sid) !=
                    server->transports.end());

            trans = new HttpTransport(sid, server->isRopMultiThreaded);
            if (server->exportables.size() > 0) {
                trans->registry->setExportables(server->exportables);
            }

            log.info("Created websocket-initiated transport for sid:%s\n",
                    sid.c_str());
            server->transports[sid] = trans;
        }

        // send session id
        log.debug("Sending session id...\n");
        Buffer buf;
        buf.write(sid);
        sendFrame(buf.begin(), buf.size());

        transport = trans;
        trans->setWebsocket(this);
        return;
    }

    if (requestPath.compare(0, 2, "/r") != 0) {
        log.warn("Unsupported path:%s\n", requestPath.c_str());
        sendResponse(404, "text/plain", "Unsupported xhr path");
        return;
    }

    // Create transport ahead of websocket connection
    if (requestPath.length() == 2) {
        do {
            sid = createSessionId();
        } while (server->transports.find(sid) !=
                server->transports.end());
        trans = new HttpTransport(sid, server->isRopMultiThreaded);
        if (server->exportables.size() > 0) {
            trans->registry->setExportables(server->exportables);
        }

        log.info("Created xhr-initiated transport for sid:%s\n",
                sid.c_str());
        server->transports[sid] = trans;

        sendResponse(200, "text/plain", sid);
        return;
    }

    // otherwise, treat as rop over xhr.
    sid = requestPath.substr(3);
    map<string, HttpTransport*>::iterator i = server->transports.find(sid);
    if (i == server->transports.end()) {
        log.warn("No corresponding transport for sid:%s\n", sid.c_str());
        sendResponse(404, "text/plain", "No transport here!");
        return;
    }

    i->second->touchTimeout();
    i->second->handleMessage(contentBuffer.begin(), contentBuffer.size(), this);
}

void HttpConnection::readFrame ()
{
    transport->handleMessage(contentBuffer.begin(), contentBuffer.size(), 0);
}

void HttpConnection::close ()
{
    // Delegate close request to the bound transport.
    // it will call this close() again.
    if (transport) {
        string &sid = transport->sessionId;
        log.info("closing transport for sid:%s\n", sid.c_str());
        server->transports.erase(sid);
        HttpTransport *ht = transport;
        transport = 0;
        ht->close();
        return;
    }

    log.info("closing http connection\n");
    HttpReactor::close();
    deref();
    return;

}

int HttpServer::start (int port, EventDriver *ed)
{
    struct sockaddr_in servAddr;
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    {
        int optval = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                sizeof(optval));
    }
    if (bind(fd, (struct sockaddr*)&servAddr, sizeof(servAddr))) {
        log.error("bind failed ret:%d\n", errno);
        ::close(fd);
        return -1;
    }
    listen(fd, 1);
    ed->watchFile(fd, acba::EVENT_READ, this);
    log.info("listening on port %d (fd:%d)\n", port, fd);
    return 0;
}

void HttpServer::stop ()
{
    abort("webserver stopped by request");
}

void HttpServer::read ()
{
    int fd;
    {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        fd = accept(getFd(), (struct sockaddr*)&clientAddr, &len);
    }
    if (!fd) {
        log.warn("Accept failed.\n");
        return;
    }
    log.info("new connection fd:%d\n", fd);

    HttpConnection *hc = new HttpConnection(this);
    getEventDriver()->watchFile(fd, acba::EVENT_READ, hc);
}

void HttpServer::closeStaleTransports () {
    map<string, HttpTransport*>::iterator i = transports.begin();
    while (i != transports.end()) {
        if (!i->second->isStale()) {
            i++;
            continue;
        }
        log.info("Deleting stale transport:%s\n", i->first.c_str());
        i->second->close();
        transports.erase(i++);
    }
}

int HttpTransport::send (Port *p, Buffer &buf)
{
    log.debug("send message(%d bytes) to port:%d\n", buf.size(), p->id);

    if (closed) {
        log.info("sending aborted. transport was closed.\n");
        return -1;
    }

    // add port id as prefix
    int8_t h = buf.peek(0);
    buf.preWriteI32(p->id);

    XhrSession *sess = getXhrSession(p->id);
    if (sess) {
        // trace call depth.
        // it becomes 0 when no subsequent xhrs are expected.
        switch ((h >> 6) & 3) {
        case Port::MESSAGE_SYNC_CALL:
            sess->callDepth++;
            break;
        case Port::MESSAGE_RETURN:
            sess->callDepth--;
            break;
        }

        // construct send buffer
        Buffer sb;
        int len = (buf.size() + 2) / 3 * 4;

        stringstream ss;
        ss << "HTTP/1.1 200 OK\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: " << len << "\r\n"
            "\r\n";
        sb.write(ss.str());
        sb.ensureMargin(len);
        base64_encode(buf.begin(), reinterpret_cast<char*>(sb.end()),
                buf.size());
        sb.grow(len);

        if (sess == currentXhrSession && sess->responses.size() == 0) {
            // directly send the response
            assert(xmlHttpRequest);
            xmlHttpRequest->send(sb.begin(), sb.size());
            xmlHttpRequest = 0;
            currentXhrSession = 0;

            // close xhr session
            if (sess->callDepth == 0) {
                xhrSessions.remove(sess);
                delete sess;
            }

        } else {
            sess->responses.writeI32(sb.size());
            sess->responses.writeBytes(sb.begin(), sb.size());
        }
    } else { // via websocket
        if (websocket) {
            websocket->sendFrame(buf.begin(), buf.size());
        } else {
            pendingMessages.writeI32(buf.size());
            pendingMessages.writeBytes(buf.begin(), buf.size());
        }
    }
    buf.clear();
    return 0;
}

int HttpTransport::wait (Port *p)
{
    if (closed) return -1;
    if (!registry) return -1;
    if (websocket && websocket->getEventDriver()->isEventThread()) {
        ScopedUnlock ul(registry->monitor);
        websocket->getEventDriver()->wait();
    } else {
        registry->wait(p->updated);
    }
    return 0;
}

void HttpTransport::close ()
{
    acba::Ref<Registry> reg = registry; // copy ref to avoid early destroy
    if (!reg) return;

    {
        ScopedLock sl(reg->monitor);

        assert(!closed);
        closed = true;

        if (websocket) {
            websocket->abort("bound transport was closed");
            websocket = 0;
        }

        if (xmlHttpRequest) {
            xmlHttpRequest->abort("bound transport was closed");
            xmlHttpRequest = 0;
        }

        Transport::close();
    }
    reg = 0;
}

HttpTransport::XhrSession *HttpTransport::getXhrSession (int portId)
{
    for (XhrSession *sess = xhrSessions.first(); sess;
            sess = xhrSessions.next(sess)) {
        if (sess->portId == portId) {
            return sess;
        }
    }
    return 0;
}

void HttpTransport::sendNextResponse (HttpConnection *xhr,
        HttpTransport::XhrSession *sess)
{
    Buffer &res = sess->responses;
    if (!res.size()) {
        // keep xhr
        xmlHttpRequest = xhr;
        currentXhrSession = sess;
        return;
    }

    int sz = res.readI32();
    xhr->send(res.begin(), sz);
    res.drop(sz);

    if (sess->callDepth == 0 && res.size() == 0) {
        xhrSessions.remove(sess);
        delete sess;
    }
}

// this function doesn't call registry.recv() in order to handle xhr.
void HttpTransport::handleMessage (uint8_t *msg, int len, HttpConnection *xhr)
{
    ScopedLock sl(registry->monitor);

    if (len < 4) {
        log.error("Message truncated\n");
        close();
        return;
    }
    Buffer buf(msg, len);
    buf.appData = registry;
    int pid = -buf.readI32();
    XhrSession *sess = 0;

    if (xhr) {
        if (xmlHttpRequest) {
            xhr->abort("Concurrent XHR detected.");
            return;
        }

        sess = getXhrSession(pid);
        if (!buf.size()) {
            if (!sess) {
                log.error("Expected valid XHR session\n");
                close();
                return;
            }
            sendNextResponse(xhr, sess);
            return;
        }
    }

    if (buf.size() < 1) {
        log.error("Too short message");
        close();
        return;
    }
    Port *p = registry->getPort(pid);
    int8_t h = buf.readI8();

    log.info("Reading from port:%d len:%d head:%d\n", pid, (len-4), h);

    // establish an xhr session if not found.
    if (xhr && !sess) {
        sess = new XhrSession();
        sess->portId = pid;
        xhrSessions.addBack(sess);
    }

    switch ((h >> 6) & 3) {
    case Port::MESSAGE_RETURN:
        {
            RemoteCall *rc = p->removeRemoteCall();
            if (!rc) {
                log.error("Expected return wait\n");
                close();
                return;
            }
            rc->readReturn(h, buf);
            if (buf.size()) {
                log.warn("Return message has trailing %d bytes.\n", buf.size());
            }
            buf.clear();

            rc->gotReturn = true;
            pthread_cond_signal(&p->updated);
            log.debug("got return of RemoteCall:%08x index:%d.\n", rc,
                    rc->returnIndex);

            if (xhr) {
                sess->callDepth--;
                // browser sent a return, which implies that
                // initial xhr request was not yet finished.
                if (sess->callDepth <= 0) {
                    log.error("XHR call depth mismatch\n");
                    close();
                    return;
                }
            }
        }
        break;

    case Port::MESSAGE_SYNC_CALL:
        if (xhr) {
            sess->callDepth++;
        }
        // fall through

    case Port::MESSAGE_ASYNC_CALL:
        {
            int32_t oid = -buf.readI32();
            SkeletonBase *skel = registry->getSkeleton(oid);
            if (!skel) {
                log.error("No skeleton object found. id:%d\n", oid);
                close();
                return;
            }

            LocalCall *lc = new LocalCall(h, skel, *registry);
            skel->readRequest(*lc, buf);
            if (buf.size()) {
                log.warn("Request message has trailing %d bytes.\n",
                        buf.size());
            }
            buf.clear();

            log.debug("got a request of LocalCall:%08x oid:%d h:%d port:%d\n",
                    lc, skel->id, h, p->id);
            p->addLocalCall(*lc);
        }
        break;

    case Port::MESSAGE_CHAINED_CALL:
        assert(!"Not Supported");
        break;
    }

    // close xhr request if complete.
    if (xhr) {
        // send dummy response, and close it.
        if (sess->callDepth == 0) {
            xhrSessions.remove(sess);
            delete sess;
            xhr->send("HTTP/1.1 200 OK\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: 0\r\n\r\n");
            return;
        }
        sendNextResponse(xhr, sess);
    }
}


void HttpTransport::setWebsocket (HttpConnection *ws)
{
    assert(!hasWebsocket());
    websocket = ws;

    int c = 0;
    Buffer &buf = pendingMessages;
    while (buf.size() > 0) {
        int s = buf.readI32();
        websocket->sendFrame(buf.begin(), s);
        buf.drop(s);
        c++;
    }

    log.info("Sent %d pending messages.\n", c);
}
