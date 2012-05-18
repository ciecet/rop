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
    pthread_cond_t writableCondition;
    pthread_t loopThread;
    bool isLooping;
    bool isSending;
    
    HttpTransport (Registry &r, int p): Transport(r), port(p),
            websocketFd(-1), requestFd(-1), inPort(0), requestPort(0),
            isLooping(false), isSending(false) {
        pthread_cond_init(&writableCondition, 0);
    }
    ~HttpTransport () {
        pthread_cond_destroy(&writableCondition);
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
    void notifyUnhandledRequest (Port *p) {} // already checked by loop()
};

}

#endif
