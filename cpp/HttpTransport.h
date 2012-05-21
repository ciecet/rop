#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include "Remote.h"

namespace rop {

struct HttpTransport: Transport {
    int listenFd;
    int port;

    int websocketFd;
    Port *inPort;
    Buffer inBuffer;
    Buffer outBuffer;

    int xhrFd;
    Port *xhrPort;
    Buffer xhrBuffer;
    int32_t xhrPortId;
    pthread_cond_t xhrWritable;

    pthread_cond_t writable;

    pthread_t loopThread;
    bool isLooping;

    bool isSending; //?
    
    HttpTransport (int p):
            listenFd(-1), port(p),
            websocketFd(-1), inPort(0),
            xhrFd(-1), xhrPort(0),
            isLooping(false),
            isSending(false) {
        pthread_cond_init(&writable, 0);
        pthread_cond_init(&xhrWritable, 0);
    }
    ~HttpTransport () {
        pthread_cond_destroy(&writable);
        pthread_cond_destroy(&xhrWritable);
    }

    void loop (); // handle reading & processing the request

    void readRequest(); // syncrhonous
    void sendResponse(); // syncrhonous
    void tryRead ();
    void handleMessage ();
    void waitWritable ();

////////////////////////////////////////////////////////////////////////////////
//  implements transport

    void send (Port *p);
    void receive (Port *p);
    void notifyUnhandledRequest (Port *p);
};

}

#endif
