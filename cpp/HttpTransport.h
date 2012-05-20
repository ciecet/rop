#ifndef HTTP_TRANSPORT_H
#define HTTP_TRANSPORT_H

#include "Remote.h"

namespace rop {

struct HttpTransport: Transport {
    int port;
    int websocketFd;
    int requestFd;
    Port *inPort;
    Port *requestPort;
    Buffer inBuffer;
    Buffer outBuffer;
    Buffer requestBuffer;
    pthread_cond_t writable;
    pthread_t loopThread;
    bool isLooping;
    bool isSending;
    bool isKeepAlive;
    
    HttpTransport (int p): port(p),
            websocketFd(-1), requestFd(-1), inPort(0), requestPort(0),
            isLooping(false), isSending(false), isKeepAlive(false) {
        pthread_cond_init(&writable, 0);
    }
    ~HttpTransport () {
        pthread_cond_destroy(&writable);
    }

    void loop (); // handle reading & processing the request

    void readRequest(); // syncrhonous
    void sendResponse(); // syncrhonous
    void tryRead ();
    void waitReadable ();
    void waitWritable ();

////////////////////////////////////////////////////////////////////////////////
//  implements transport

    void send (Port *p);
    void receive (Port *p);
    void notifyUnhandledRequest (Port *p);
};

}

#endif
