#ifndef UNIX_TRANSPORT_H
#define UNIX_TRANSPORT_H

#include "Remote.h"

namespace rop {

struct UnixTransport: Transport {
    int inFd;
    int outFd;
    Buffer inBuffer;
    Port *inPort;
    Buffer outBuffer;
    pthread_cond_t writableCondition;
    bool isSending;
    pthread_t loopThread;
    bool isLooping;
    
    UnixTransport (Registry &r, int i, int o): Transport(r),
            inFd(i),outFd(o),isSending(false), inPort(0), isLooping(false) {
        pthread_cond_init(&writableCondition, 0);
    }
    ~UnixTransport () {
        pthread_cond_destroy(&writableCondition);
    }

    void loop (); // handle reading
    
    void tryReceive ();
    void waitReadable ();
    void waitWritable ();

////////////////////////////////////////////////////////////////////////////////
//  implements transport

    void flushPort (Port *p);
    void waitPort (Port *p);
    void notifyUnhandledRequest (Port *p) {} // already checked by loop()
};

}

#endif
