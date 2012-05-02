#ifndef UNIX_TRANSPORT_H
#define UNIX_TRANSPORT_H

#include "Remote.h"

struct UnixPort: Port {
    pthread_cond_t responseCondition;
    UnixPort () {
        pthread_cond_init(&responseCondition, 0);
    }
    ~UnixPort () {
        pthread_cond_destroy(&responseCondition);
    }
};

struct UnixTransport: Transport {
    int inFd;
    int outFd;
    Buffer inBuffer;
    PortRef inPort;
    Buffer outBuffer;
    PortRef outPort;
    pthread_mutex_t lock;
    pthread_cond_t writableCondition;
    bool isReading;
    
    UnixTransport (Registry &pt, int i, int o):
            Transport(pt), inFd(i),outFd(o),isReading(false) {
        pthread_mutex_init(&lock, 0);
        pthread_cond_init(&writableCondition);
    }
    ~UnixTransport () {
        pthread_cond_destroy(&writableCondition);
        pthread_mutex_destroy(&lock);
    }

    PortRef getPort ();
    PortRef createPort () {
        return UnixPort();
    }
    void sendAndWait (PortRef p);
    void notify (PortRef p);
    void send (PortRef p);
    void loop (); // handle reading
    
    void tryReceive ();
    void trySend ();
    void tryHandleProcesses ();
    void waitWritable ();
};

#endif
