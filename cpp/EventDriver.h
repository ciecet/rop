#ifndef EVENT_DRIVER_H
#define EVENT_DRIVER_H

#include <sys/select.h>
#include <pthread.h>
#include <vector>
#include <map>
#include "Log.h"

namespace base {

typedef enum {
    REACTOR_FILE,
    REACTOR_ASYNC,
    REACTOR_END
} REACTOR_TYPE;

typedef enum {
    EVENT_READ = 1<<0,
    EVENT_WRITE = 1<<1,
    EVENT_ASYNC = 1<<2
} EVENT_TYPE;

struct EventDriver;
struct FileReactor;

struct Reactor {

    REACTOR_TYPE type;
    int mask; // set of interested events
    EventDriver *eventDriver; // non-null iff this reactor is active.

    // internal states used by event driver. (DON'T ACCESS DIRECTLY)
    Reactor *next;
    Reactor *nextPending;
    int events; // set of triggered events

    Reactor (REACTOR_TYPE t, int m = 0): type(t), mask(m), eventDriver(0),
            events(0) {}

    virtual void processEvent (int events) = 0;
};

struct ReactorGroup {
    int mask;
    Reactor *list;
    ReactorGroup(): mask(0), list(0) {}
};

/**
 * Reenterant event handler
 */
struct EventDriver {

    vector<ReactorGroup> files;
    Reactor *pendingReactors;
    Reactor *asyncReactors;
    int asyncFd;

    // thread running this event driver. 
    // initialized on creation as calling thread.
    // modify this on necessity.
    pthread_t thread;

    Log log;

    // select specifics
    fd_set inFds, outFds;

    EventDriver ();
    ~EventDriver ();

    /**
     * Add reactor.
     * Must be called within event thread.
     */
    void add (Reactor *r);

    /**
     * Remove reactor.
     * Must be called within event thread.
     */
    void remove (Reactor *r);

    void updateFileMask (int fd);

    void handleEvent ();
    void loop ();
    bool isRunningThread () {
        return pthread_equal(thread, pthread_self());
    }

private:
    void flushPendingReactors ();
};

struct FileReactor: Reactor {

    int fd;

    virtual void read () {}
    virtual void write () {}
    void processEvent (int events) {
        if (events & EVENT_READ) {
            read();
        }
        if (events & EVENT_WRITE) {
            write();
        }

    }

    FileReactor (): Reactor(REACTOR_FILE), fd(-1) {}

    void changeMask (int m) {
        mask = m;
        if (eventDriver) {
            eventDriver->updateFileMask(fd);
        }
    }
};

struct AsyncReactor: Reactor {
    AsyncReactor (): Reactor(REACTOR_ASYNC, EVENT_ASYNC) {}
};

}
#endif
