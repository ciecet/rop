#ifndef UNIX_TRANSPORT_H
#define UNIX_TRANSPORT_H

#include <pthread.h>
#include "ScopedLock.h"
#include "EventDriver.h"
#include "Ref.h"
#include "rop.h"
#include "Log.h"

#ifdef ED_MANGLE
#define SodketServer ED_MANGLE(SocketServer)
#define SodketTransport ED_MANGLE(SocketTransport)
#endif

namespace rop {

struct SocketServer: acba::FileReactor {
    bool isRopMultiThreaded;
    std::map<std::string,InterfaceRef> exportables;

    acba::Log log;

    SocketServer(bool rmt = true): acba::FileReactor(true),
        isRopMultiThreaded(rmt), log("Sock ") {}

    void setExportables (const std::map<std::string,InterfaceRef> &exps) {
        exportables = exps;
    }

    int start(int port, acba::EventDriver* ed);
    int start(acba::EventDriver* ed, int port) { // deprecated
        return start(port, ed);
    }

    void stop();
    void read();
};

struct SocketTransport: Transport, acba::FileReactor {

    struct RequestReactor: acba::Reactor {
        Registry *registry;
        RequestReactor (Registry *reg): acba::Reactor(false), registry(reg) {}
        void processEvent (int events) {
            registry->run();
        }
    } requestReactor;

    struct OutBufferMonitor: acba::Reactor {
        SocketTransport *socketTransport;
        pthread_cond_t inLimit;
        // used only in multi-threaded mode
        OutBufferMonitor (SocketTransport *trans): acba::Reactor(true),
                socketTransport(trans) {
            pthread_condattr_t attr;
            pthread_condattr_init(&attr);
            pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
            pthread_cond_init(&inLimit, &attr);
        }
        ~OutBufferMonitor () {
            pthread_cond_destroy(&inLimit);
        }
        void processEvent (int events) {
            socketTransport->getEventDriver()->wait();
            acba::ScopedLock sl(socketTransport->registry->monitor);
            pthread_cond_broadcast(&inLimit);
        }
        void waitUntilFlushed () {
            SocketTransport *trans = socketTransport;
            while (trans->outBuffer.size() > 0) { //= trans->outBufferLimit) {
                socketTransport->getEventDriver()->setTimeout(0, this);
                socketTransport->registry->wait(inLimit);
            }
        }
    } outBufferMonitor;

    acba::Buffer inBuffer;
    acba::Buffer outBuffer;

    acba::Log log;

    SocketTransport (bool mt=true): Transport(mt), acba::FileReactor(),
            requestReactor(registry.get()),
            outBufferMonitor(this),
            log("socktrans "), outBufferLimit(0) {}

    ~SocketTransport () {
        log.trace("deleted\n");
    }

////////////////////////////////////////////////////////////////////////////////
//  implements file reactor

    void read ();
    void write ();
    void close (); // defined from both file reactor and transport

////////////////////////////////////////////////////////////////////////////////
//  implements transport

    int send (Port *p, acba::Buffer &buf);
    int wait (Port *p);
    void requestRun ();

////////////////////////////////////////////////////////////////////////////////
//  config api

    void setOutBufferLimit (int l) {
        outBufferLimit = l;
    }

private:
    int outBufferLimit;
    int tryWrite (acba::Buffer &buf);
};

}

#endif
