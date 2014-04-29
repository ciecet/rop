#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include <pthread.h>
#include <assert.h>
#include <sys/time.h>
#include <map>
#include <string>
#include "rop.h"
#include "Ref.h"
#include "EventDriver.h"
#include "HttpReactor.h"
#include "dlink.h"

#ifdef ED_MANGLE
#define HttpTransport ED_MANGLE(HttpTransport)
#define HttpServer ED_MANGLE(HttpServer)
#endif

namespace rop {

struct HttpTransport;

struct HttpServer: acba::FileReactor {

    bool isRopMultiThreaded;
    std::map<std::string,InterfaceRef> exportables;
    std::map<std::string, HttpTransport*> transports;

    acba::Log log;

    HttpServer (bool rmt = true): acba::FileReactor(true),
            isRopMultiThreaded(rmt), log("hsrv ") {}

    void setExportables (const std::map<std::string,InterfaceRef> &exps) {
        exportables = exps;
    }

    int start (int port, acba::EventDriver *ed);
    int start (acba::EventDriver *ed, int port) { // deprecated
        return start(port, ed);
    }

    void stop ();

    void read ();

    void closeStaleTransports ();
};

struct HttpConnection: acba::HttpReactor, acba::RefCounter<HttpConnection> {
    HttpTransport *transport; // attached transport in case of websocket.
    HttpServer *server;

    HttpConnection (HttpServer *srv): transport(0), server(srv) {
        ref(); // corresponding deref will be called on close.
    }
    void readRequest ();
    void readFrame ();
    void close ();
};

struct HttpTransport: Transport, acba::Reactor {

    HttpTransport (const std::string &sid, bool mt=true):
            Transport(mt), acba::Reactor(false),
            sessionId(sid), closed(false), websocket(0),
            xmlHttpRequest(0), currentXhrSession(0),
            log("htrans ") {
        touchTimeout();
    }

    ~HttpTransport () {}

    // used only in single-thread mode
    void requestRun () {
        acba::EventDriver *ed = websocket->getEventDriver();
        if (ed->isEventThread()) ed->setTimeout(0, this);
    }

    // used only in single-thread mode
    void processEvent (int events) {
        registry->run();
    }

    int send (Port *p, acba::Buffer &buf);
    int wait (Port *p);
    void close ();

    // handle incoming request via websocket or xhr.
    // this function doesn't call registry.recv() in order to handle xhr.
    void handleMessage (uint8_t *buf, int len, HttpConnection *xhr);

    void setWebsocket (HttpConnection *ws);

    bool hasWebsocket () {
        return websocket.get() != 0;
    }

    const static long TIMEOUT = 60 * 1000; // in millis

    void touchTimeout () {
        clock_gettime(CLOCK_MONOTONIC, &timeout);
        timeout.tv_sec += TIMEOUT / 1000;
        timeout.tv_nsec += (TIMEOUT % 1000) * 1000000;
    }

    bool isStale () {
        if (websocket) return false;
        timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (timeout.tv_sec > now.tv_sec) return false;
        if (timeout.tv_sec < now.tv_sec) return true;
        return (timeout.tv_nsec < now.tv_nsec);
    }

    std::string sessionId;

private:

    bool closed;
    acba::Ref<HttpConnection> websocket;

    // Represents the binding of port and xhr session.
    // xhr session may span to multiple xhr connections.
    struct XhrSession: DLinkArray<1> {
        int portId; // port id attached with this xhr session.
        int callDepth; // increase on sync call, decrease on return
        // pending xhr responses (each one prefixed by length(32bit))
        acba::Buffer responses;
    };

    // xhr request waiting for response.
    // xmlHttpRequest was located out of XhrSession since rop uses blocked xhr
    // and only one xhr connection may exist at a time.
    acba::Ref<HttpConnection> xmlHttpRequest;
    XhrSession *currentXhrSession;

    // list of xhr sessions.
    // Multiple xhr sessions may exist at a time as following scenario:
    // 1. browser sends *sync* call (from port A)
    // 2. webmw sends back inner *async* call
    // 3. browser sends sync call again in there. (from port B, not A)
    IntrusiveDoubleList<XhrSession, 0> xhrSessions;

    acba::Buffer pendingMessages;

    acba::Log log;

    XhrSession *getXhrSession (int portId);
    void sendNextResponse (HttpConnection *xhr, XhrSession *sess);

    // timeout for xhr-initiated transport before websocket is attached.
    timespec timeout;
};

}

#endif
