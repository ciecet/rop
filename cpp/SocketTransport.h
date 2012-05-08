#ifndef UNIX_TRANSPORT_H
#define UNIX_TRANSPORT_H

#include "Remote.h"

namespace rop {

struct SocketTransport: Transport {
    int inFd;
    int outFd;
    Buffer inBuffer;
    Port *inPort;
    Buffer outBuffer;
    pthread_cond_t writableCondition;
    bool isSending;
    pthread_t loopThread;
    bool isLooping;
    
    SocketTransport (Registry &r, int i, int o): Transport(r),
            inFd(i),outFd(o),isSending(false), inPort(0), isLooping(false) {
        pthread_cond_init(&writableCondition, 0);
    }
    ~SocketTransport () {
        pthread_cond_destroy(&writableCondition);
        close(inFd);
        close(outFd);
    }

    void loop (); // handle reading
    
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
