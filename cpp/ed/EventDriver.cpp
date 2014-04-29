#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include "EventDriver.h"
#include "Log.h"

#ifndef ROP_USE_EVENTFD
#ifdef __linux__
#define ROP_USE_EVENTFD 1
#else
#define ROP_USE_EVENTFD 0
#endif
#endif

using namespace acba;

#define TIMEOUT_MAX 0x7fffffffffffffffll

#if ROP_USE_EVENTFD

#include <sys/eventfd.h>
static void initSleepFds (int *fds) {
    fds[0] = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
}
static void destroySleepFds (int *fds) {
    close(fds[0]);
}
static void wakeUp (int *fds) {
    uint64_t c = 1;
    write(fds[0], &c, sizeof(c));
}
static void prepareSleep (int *fds) {
    // consume async fd event
    uint64_t c;
    read(fds[0], &c, sizeof(c));
}

#else

#include <fcntl.h>
static void initSleepFds (int *fds) {
    pipe(fds);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
}
static void destroySleepFds (int *fds) {
    close(fds[0]);
    close(fds[1]);
}
static void wakeUp (int *fds) {
    char c = 1;
    write(fds[1], &c, sizeof(c));
}
static void prepareSleep (int *fds) {
    // consume async fd event
    char c;
    while (read(fds[0], &c, sizeof(c)) > 0);
}

#endif

#ifdef __linux__
#include <time.h>
static int64_t currentClock ()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
#else
static int64_t currentClock ()
{
    timeval t;
    gettimeofday(&t,0);
    return (int64_t)t.tv_sec * 1000000 + t.tv_usec;
}
#endif

EventDriver::EventDriver (): nestCount(0), waitPhase(0), log("ed ")
{
    thread = pthread_self();
    pthread_mutex_init(&lock, 0);

    FD_ZERO(&inFds);
    FD_ZERO(&outFds);
    initSleepFds(sleepFds);
    fdEnd = sleepFds[0] + 1;
    baseClock = currentClock();
}

EventDriver::~EventDriver ()
{
    destroySleepFds(sleepFds);
    pthread_mutex_destroy(&lock);
}

void EventDriver::watchFile (int fd, int fmask, Reactor *r)
{
    if (!r->eventDriver) r->eventDriver = this;
    assert(r->eventDriver == this);
    assert(fd >= 0);

    pthread_mutex_lock(&lock);

    // add to file reactor list
    if (r->fd == -1) {
        fileReactors.addBack(r);
        r->fd = fd;
        log.debug("watch fd:%d r:%08x\n", fd, r);
    } else if (r->fd != fd) {
        log.error("Cannot change fd from:%d to:%d.\n", r->fd, fd);
        goto exit;
    }

    if (fd >= fdEnd) {
        fdEnd = fd + 1;
    }

    r->mask &= ~(EVENT_READ | EVENT_WRITE);
    r->mask |= EVENT_FILE | (fmask & (EVENT_READ | EVENT_WRITE));

    if (waitPhase & 1) {
        assert(!isEventThread());
        wakeUp(sleepFds);
    } else {
        if (fmask & EVENT_READ) {
            FD_SET(fd, &inFds);
        } else {
            FD_CLR(fd, &inFds);
        }
        if (fmask & EVENT_WRITE) {
            FD_SET(fd, &outFds);
        } else {
            FD_CLR(fd, &outFds);
        }
    }

exit:
    pthread_mutex_unlock(&lock);
}

void EventDriver::tryUnbind (Reactor *r) {
    // unbind reactor if no events are in interest nor scheduled.
    if (!r->mask && !r->events) {
        r->eventDriver = 0;
    }
}

void EventDriver::unwatchFile (Reactor *r)
{
    if (!r->eventDriver) return;
    assert(r->eventDriver == this);
    assert(isEventThread());

    pthread_mutex_lock(&lock);

    if (r->fd == -1) {
        goto exit;
    }

    if (r->events & (EVENT_READ | EVENT_WRITE)) {
        r->events &= ~(EVENT_READ | EVENT_WRITE);
        if (!r->events) {
            EventedReactorList::remove(r);
        }
    }

    log.debug("unwatch fd:%d r:%08x\n", r->fd, r);

    FD_CLR(r->fd, &inFds);
    FD_CLR(r->fd, &outFds);
    r->mask &= ~(EVENT_FILE | EVENT_READ | EVENT_WRITE);
    fileReactors.remove(r);
    r->fd = -1;
    tryUnbind(r);

exit:
    pthread_mutex_unlock(&lock);
}

void EventDriver::setTimeout (int64_t usec, Reactor *r)
{
    if (!r->eventDriver) r->eventDriver = this;
    assert(r->eventDriver == this);

    pthread_mutex_lock(&lock);

    if (r->events & EVENT_TIMEOUT) {
        r->events &= ~EVENT_TIMEOUT;
        if (!r->events) {
            EventedReactorList::remove(r);
        }
    }

    if (r->mask & EVENT_TIMEOUT) {
        timeoutReactors.remove(r);
    } else {
        r->mask |= EVENT_TIMEOUT;
    }

    int64_t when = currentClock() - baseClock + usec;
    r->timeout = when;
    Reactor *last = timeoutReactors.last();
    while (last && last->timeout > when) {
        last = timeoutReactors.prev(last);
    }
    if (last) {
        timeoutReactors.addAfter(r, last);
    } else {
        timeoutReactors.addFront(r);
    }

    if (waitPhase & 1) {
        assert(!isEventThread());
        wakeUp(sleepFds);
    }

    pthread_mutex_unlock(&lock);
}

void EventDriver::unsetTimeout (Reactor *r)
{
    if (!r->eventDriver) return;
    assert(r->eventDriver == this);
    assert(isEventThread());

    pthread_mutex_lock(&lock);

    if (r->events & EVENT_TIMEOUT) {
        r->events &= ~EVENT_TIMEOUT;
        if (!r->events) {
            EventedReactorList::remove(r);
        }
    }

    if (r->mask & EVENT_TIMEOUT) {
        r->mask &= ~EVENT_TIMEOUT;
        timeoutReactors.remove(r);
    }
    tryUnbind(r);

    pthread_mutex_unlock(&lock);
}

void EventDriver::unbind (Reactor *r)
{
    if (!r->eventDriver) return;
    assert(r->eventDriver == this);

    int flags = r->mask | r->events;
    if (flags & EVENT_FILE) unwatchFile(r);
    if (flags & EVENT_TIMEOUT) unsetTimeout(r);

    r->eventDriver = 0;
}

void EventDriver::sendEvents ()
{
    Reactor *r;
    while (true) {
        pthread_mutex_lock(&lock);
        if (nestCount == 1) {
            r = pendingReactors.removeFront();
            if (!r) {
                r = eventedReactors.removeFront();
            }
        } else {
            while (true) {
                r = eventedReactors.removeFront();
                if (!r || r->safe) break;
                pendingReactors.addBack(r);
            }
        }
        if (!r) {
            // gather timed-out reactors.
            bool evented = false;
            int64_t now = currentClock() - baseClock;
            while (true) {
                r = timeoutReactors.first();
                if (!r || r->timeout > now ) break;
                r->mask &= ~EVENT_TIMEOUT;
                timeoutReactors.remove(r);
                r->events |= EVENT_TIMEOUT;
                eventedReactors.addBack(r);
                evented = true;
            }
            pthread_mutex_unlock(&lock);
            if (evented) continue;
            break;
        }
        int events = r->events;
        r->events = 0;
        tryUnbind(r);

        // resume file listening
        if (r->mask & EVENT_READ) {
            FD_SET(r->fd, &inFds);
        }
        if (r->mask & EVENT_WRITE) {
            FD_SET(r->fd, &outFds);
        }

        pthread_mutex_unlock(&lock);

        log.debug("process event:%d mask:%d fd:%d r:%08x nestCount:%d\n",
                events, r->mask, r->getFd(), r, nestCount);

        r->processEvent(events);
    }
}

void EventDriver::handleEvent (int64_t usec)
{
    // Need to flush evented reactors in case of nested call.
    {
        unsigned int wp = waitPhase;
        sendEvents();
        if (wp != waitPhase) return;
    }

    pthread_mutex_lock(&lock);

    // calculate timeout
    int64_t timeout = (usec == 0) ? TIMEOUT_MAX: usec;
    Reactor *tr = timeoutReactors.first();
    if (tr) {
        int64_t est = tr->timeout - (currentClock() - baseClock);
        if (est <= 0) {
            timeout = 0;
        } else if (est < timeout) {
            timeout = est;
        }
    }

    waitPhase++; // becomes odd number, ready for waiting
    prepareSleep(sleepFds);
    FD_SET(sleepFds[0], &inFds);

#if ROP_DEBUG
    for (int i = 0; i < fdEnd; i++) {
        if (i % 16 == 0) printf(" ");
        printf("%d", FD_ISSET(i, &inFds));
    }
    printf("\n");
    for (int i = 0; i < fdEnd; i++) {
        if (i % 16 == 0) printf(" ");
        printf("%d", FD_ISSET(i, &outFds));
    }
    printf("\n");
#endif

    pthread_mutex_unlock(&lock);

    // do wait
    int n;
    if (timeout == TIMEOUT_MAX) {
        n = select(fdEnd, &inFds, &outFds, 0, 0);
    } else {
        timeval tv = { timeout / 1000000, timeout % 1000000 };
        n = select(fdEnd, &inFds, &outFds, 0, &tv);
    }
    if (n == -1) {
        log.error("select() returned errno:%s.\n", strerror(errno));
        waitPhase++; // need monitor
        return;
    }

    pthread_mutex_lock(&lock);

    // gather file reactors
    for (Reactor *r = fileReactors.first(); r; r = fileReactors.next(r)) {
        int fd = r->fd;
        int e = 0;
        if (FD_ISSET(fd, &inFds)) {
            e |= EVENT_READ;
            FD_CLR(fd, &inFds);
        } else if (r->mask & EVENT_READ) {
            FD_SET(fd, &inFds);
        }
        if (FD_ISSET(fd, &outFds)) {
            e |= EVENT_WRITE;
            FD_CLR(fd, &outFds);
        } else if (r->mask & EVENT_WRITE) {
            FD_SET(fd, &outFds);
        }
        if (!e) {
            continue;
        }

        log.trace("file event on fd:%d evt:%d\n", fd, e);
        if (!r->events) {
            eventedReactors.addBack(r);
        }
        r->events |= e;
    }

    waitPhase++; // becomes even number, mark wait complete.

    pthread_mutex_unlock(&lock);

    sendEvents();
}

void EventDriver::run ()
{
    thread = pthread_self();
    nestCount++;
    while (true) {
        handleEvent();
    }
    nestCount++;
}

