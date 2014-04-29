#ifndef EVENT_DRIVER_H
#define EVENT_DRIVER_H

#include <stdint.h>
#include <sys/select.h>
#include <pthread.h>
#include <vector>
#include <map>
#include "Log.h"
#include "dlink.h"

#ifdef ED_MANGLE_PREFIX
#define _ED_CONCAT0(X,Y) X ## Y
#define _ED_CONCAT1(X,Y) _ED_CONCAT0(X,Y)
#define ED_MANGLE(X) _ED_CONCAT1(ED_MANGLE_PREFIX, X)
#endif

#ifdef ED_MANGLE
#define Reactor ED_MANGLE(Reactor)
#define EventDriver ED_MANGLE(EventDriver)
#define FileReactor ED_MANGLE(FileReactor)
#endif

namespace acba {

typedef enum {
    EVENT_READ = 1<<0,
    EVENT_WRITE = 1<<1,
    EVENT_FILE = 1<<2, // use for mask only. set on watchFile */
    EVENT_TIMEOUT = 1<<3
} EVENT_FLAG;

struct EventDriver;

/**
 * Abstraction of event listener.
 * Every events generated from event driver is sent to reactor.
 */
struct Reactor: private DLinkArray<3> {

    Reactor (bool s = true): log("reactor "), safe(s), eventDriver(0),
            mask(0), events(0), fd(-1) {}
    virtual ~Reactor () {}

    // may change after return if called from non event thread
    inline int getMask () { return mask; }

    // may change after return if called from non event thread
    inline int getFd () { return fd; }

    // returns bound event driver.
    inline EventDriver *getEventDriver () { return eventDriver; }

    virtual void processEvent (int events) = 0;

protected:

    Log log;

private:

    bool safe;

    // non-null iff reactor is bound to the event driver.
    EventDriver *eventDriver;

    int mask; // set of interested events
    int events; // set of triggered events
    int fd;
    int64_t timeout;

    friend struct EventDriver;
    friend struct DLinkPicker<Reactor,0>;
    friend struct DLinkPicker<Reactor,1>;
    friend struct DLinkPicker<Reactor,2>;
};

/**
 * Reenterant event handler
 */
struct EventDriver {

    EventDriver ();
    ~EventDriver ();

    void watchFile (int fd, int fmask, Reactor *r);
    void unwatchFile (Reactor *r);
    void setTimeout (int64_t usec, Reactor *r);
    void unsetTimeout (Reactor *r);
    void unbind (Reactor *r);

    /**
     * Block once until any events are processed.
     */
    void wait (int64_t usec = 0) {
        nestCount++;
        handleEvent(usec);
        nestCount--;
    }

    /**
     * Conventional method which repeatedly calls wait().
     */
    void run ();

    bool isEventThread () {
        return pthread_equal(thread, pthread_self());
    }

private:

    // thread running this event driver. 
    // by default, set as the thread which called constructor.
    // modify this on necessity.
    pthread_t thread;
    pthread_mutex_t lock;

    typedef IntrusiveDoubleList<Reactor,0> EventedReactorList;
    EventedReactorList eventedReactors, pendingReactors;
    IntrusiveDoubleList<Reactor,1> fileReactors;
    IntrusiveDoubleList<Reactor,2> timeoutReactors; // sorted by timeout

    int sleepFds[2];
    int nestCount;
    unsigned int waitPhase; // -- odd number -- select-like -- even number --

    // select specifics
    fd_set inFds, outFds;
    int fdEnd;

    int64_t baseClock;
    Log log;

    void sendEvents ();
    void handleEvent (int64_t usec = 0);
    void tryUnbind (Reactor *r);
};

struct FileReactor: Reactor {

    FileReactor (bool s = true): Reactor(s),
            processingReadWrite(false) {}

    virtual void read () {}
    virtual void write () {}
    virtual void close () {
        int fd = getFd();
        if (fd >= 0) {
            getEventDriver()->unwatchFile(this);
            ::close(fd);
        }
    }

    /** Utility API which ensure close() call to be called in event thread. */
    void abort (const char *msg = "") {
        EventDriver *ed = getEventDriver();
        if (!ed) return;
        if (ed->isEventThread() && !processingReadWrite) {
            log.warn("closing fd:%d. %s\n", getFd(), msg);
            close();
        } else {
            log.warn("scheduled closing fd:%d. %s\n", getFd(), msg);
            ed->watchFile(getFd(), 0, this);
            ed->setTimeout(0, this);
        }
    }

    void setReadable (bool r) {
        if (getFd() == -1) return;
        int fmask = getMask();
        if (r) {
            fmask |= EVENT_READ;
        } else {
            fmask &= ~EVENT_READ;
        }
        getEventDriver()->watchFile(getFd(), fmask, this);
    }

    void setWritable (bool w) {
        if (getFd() == -1) return;
        int fmask = getMask();
        if (w) {
            fmask |= EVENT_WRITE;
        } else {
            fmask &= ~EVENT_WRITE;
        }
        getEventDriver()->watchFile(getFd(), fmask, this);
    }

    void processEvent (int events) {
        if ((events & EVENT_TIMEOUT) != 0) {
            // assumes that timeout occurred by abort()
            close();
            return;
        }
        processingReadWrite = true;
        if ((getMask() & EVENT_FILE) && (events & EVENT_WRITE)) {
            write();
        }
        if ((getMask() & EVENT_FILE) && (events & EVENT_READ)) {
            read();
        }
        processingReadWrite = false;
    }

private:

    bool processingReadWrite;
};

}
#endif
