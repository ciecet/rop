#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include <pthread.h>
#include "Remote.h"
#include "EventDriver.h"
#include "Http.h"

namespace rop {

struct HttpTransport: Transport {

    // listen to server port and create HttpConnection on accept.
    struct HttpServer: base::FileReactor {
        HttpTransport *self;
        HttpServer (HttpTransport *ht): self(ht) {
            mask = base::EVENT_READ;
        }
        void read ();
        void write () {}
    };

    // websocket or xhr.
    struct HttpConnection: base::HttpReactor {
        HttpTransport *self;
        int requestCount; // balance of request-response in keep-alive
        HttpConnection *next;
        HttpConnection (HttpTransport *ht): self(ht), requestCount(0) {
            mask = base::EVENT_READ;
        }
        void readRequest ();
        void writeResponse () {}
        void readFrame ();
        void writeFrame ();
        void close (const char *msg);
    };

    HttpServer server;
    HttpConnection *webSocket;
    HttpConnection *xmlHttpRequest;
    HttpConnection *connections;
    base::EventDriver *eventDriver;

    base::Buffer inBuffer;
    base::Buffer outBuffer;
    base::Buffer portBuffer; // intermediate buffer between outBuffer and port.
    Port *sendingPorts;
    Port **sendingPortsTail;

    int32_t xhrPortId;
    int32_t xhrCallDepth;
    pthread_cond_t xhrWritable;

    Log log;

    HttpTransport ():
            server(this), webSocket(0), xmlHttpRequest(0), connections(0),
            eventDriver(0),
            sendingPorts(0), sendingPortsTail(&sendingPorts),
            xhrPortId(0), xhrCallDepth(0),
            log("htrans ") {
        pthread_cond_init(&xhrWritable, 0);
    }

    ~HttpTransport () {
        pthread_cond_destroy(&xhrWritable);
    }

    void start (int port, base::EventDriver *ed);
    void stop ();

    void tryRead ();
    void handleMessage ();
    void waitWritable ();

////////////////////////////////////////////////////////////////////////////////
//  implements transport

    void send (Port *p);
    void sendResponse(Port *p); // xhr version of send()
    void receive (Port *p);
    void notifyUnhandledRequest (Port *p);
};

}

#endif
