#ifndef EVENT_DRIVER_H
#define EVENT_DRIVER_H

#include <sys/select.h>
#include <pthread.h>
#include <vector>
#include <map>
#include "MZEventDispatcher.h"
#include "Log.h"
#include "dlink.h"
#include "ScopedLock.h"

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
} EVENT_TYPE;

struct EventDriver;

/**
 * Abstraction of event listener.
 * Every events generated from event driver is sent to reactor.
 */
struct Reactor: private MZEventModule, private DLinkArray<1> {


    Reactor (bool s = true): log("reactor "), safe(s),
            eventDriver(0), mask(0), fd(-1) {}
    virtual ~Reactor () {}

    inline int getMask () { return mask; }
    inline int getFd () { return fd; }
    inline EventDriver *getEventDriver () { return eventDriver; }

    virtual void processEvent (int events) = 0;

protected:

    Log log;

private:

    bool safe;

    // non-null iff reactor is bound to the event driver.
    EventDriver *eventDriver;

    int mask; // set of interested events
    int fd;

    Reactor *next; // used for timeout event

    void dispatchEvent (MZEventDispatcher *dispatcher, int events);

    friend struct EventDriver;
    friend struct DLinkPicker<Reactor,0>;
};

/**
 * Reenterant event handler
 */
struct EventDriver: private MZEventModule {

    EventDriver (MZEventDispatcher *edt = 0);
    ~EventDriver ();

    void watchFile (int fd, int fmask, Reactor *r);
    void unwatchFile (Reactor *r);
    void setTimeout (int64_t usec, Reactor *r);
    void unsetTimeout (Reactor *r);
    void unbind (Reactor *r);

    void wait (int64_t usec = 0);
    void run ();

    bool isEventThread () {
        return m_edt->isDispatchThread();
    }

private:

    Log log;

    MZEventDispatcher* m_edt;
    MZ_HANDLE m_group;

    int timeoutFds[2];
    IntrusiveDoubleList<Reactor,0> timeoutReactors;

    void tryUnbind (Reactor *r);
    // handle timeout
    void dispatchEvent (MZEventDispatcher *dispatcher, int events);
};

// copied from ed/EventDriver.
struct FileReactor: Reactor {

    FileReactor (bool s = true): Reactor(s) {}

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
        if (ed->isEventThread()) {
            log.warn("closing fd:%d. %s\n", getFd(), msg);
            close();
        } else {
            log.warn("scheduled closing fd:%d. %s\n", getFd(), msg);
            ed->setTimeout(0, this);
        }
    }

    void setReadable (bool r) {
        if (getFd() == -1) return;
        int fmask = getMask();
        if (r) {
            fmask |= acba::EVENT_READ;
        } else {
            fmask &= ~acba::EVENT_READ;
        }
        getEventDriver()->watchFile(getFd(), fmask, this);
    }

    void setWritable (bool w) {
        if (getFd() == -1) return;
        int fmask = getMask();
        if (w) {
            fmask |= acba::EVENT_WRITE;
        } else {
            fmask &= ~acba::EVENT_WRITE;
        }
        getEventDriver()->watchFile(getFd(), fmask, this);
    }

    void processEvent (int events) {
        if ((events & acba::EVENT_TIMEOUT) != 0) {
            // assumes that timeout occurred by abort()
            close();
            return;
        }
        if ((getMask() & acba::EVENT_FILE) && (events & acba::EVENT_READ)) {
            read();
        }
        if ((getMask() & acba::EVENT_FILE) && (events & acba::EVENT_WRITE)) {
            write();
        }
    }
};

}

#endif
