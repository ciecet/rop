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
    pthread_cond_t writable;
    bool isSending;
    pthread_t loopThread;
    bool isLooping;
    
    SocketTransport (Registry &r, int i, int o):
            inFd(i), outFd(o), inPort(0), isSending(false), isLooping(false) {
        pthread_cond_init(&writable, 0);
        r.setTransport(this);
    }
    ~SocketTransport () {
        if (registry) {
            registry->setTransport(0);
        }

        pthread_cond_destroy(&writable);
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
