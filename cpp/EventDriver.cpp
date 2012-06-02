#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <sys/select.h>
#include <sys/eventfd.h>
#include "EventDriver.h"

using namespace base;

EventDriver::EventDriver ():
        pendingReactors(0), 
        asyncReactors(0),
        log("evt ") {
    FD_ZERO(&inFds);
    FD_ZERO(&outFds);
    asyncFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    FD_SET(asyncFd, &inFds);
    thread = pthread_self();
}

EventDriver::~EventDriver ()
{
    close(asyncFd);
}

void EventDriver::updateFileMask (int fd) {
    ReactorGroup &rg = files[fd];
    int m = 0;
    for (Reactor *r = rg.list; r; r = r->next) {
        m |= r->mask;
    }

    if (m & EVENT_READ) {
        FD_SET(fd, &inFds);
    } else {
        FD_CLR(fd, &inFds);
    }

    if (m & EVENT_WRITE) {
        FD_SET(fd, &outFds);
    } else {
        FD_CLR(fd, &outFds);
    }

    rg.mask = m;
}

void EventDriver::add (Reactor *r) {
    r->eventDriver = this;
    switch (r->type) {
    case REACTOR_FILE:
        {
            FileReactor *fr = static_cast<FileReactor*>(r);
            if (fr->fd >= files.size()) {
                files.resize(fr->fd + 1);
            }
            ReactorGroup &rg = files[fr->fd];
            fr->next = static_cast<FileReactor*>(rg.list);
            rg.list = fr;
            updateFileMask (fr->fd);
            break;
        }
    case REACTOR_ASYNC:
        {
            Reactor *ar;
            log.fatal("adding async. new:%08x ar:%08x\n", r, asyncReactors);
            while (true) {
                ar = asyncReactors;
                r->next = ar;
                if (__sync_bool_compare_and_swap(&asyncReactors, ar, r)) break;
                pthread_yield();
            }
            log.fatal("added async. ar:%08x ar->next:%08x\n", asyncReactors, asyncReactors->next);
            if (!ar) {
                uint64_t c = 1;
                write(asyncFd, &c, sizeof(c));
            }
            break;
        }

    default:
        log.error("Unsupported reactor type:%d\n", r->type);
    }
}

void EventDriver::remove (Reactor *r) {
    r->eventDriver = 0;
    // remove if events are pending
    if (r->events) {
        for (Reactor **pr = &pendingReactors; *pr; pr = &(*pr)->nextPending) {
            if ((*pr) == r) {
                (*pr) = r->nextPending;
                break;
            }
        }
    }

    switch (r->type) {
    case REACTOR_FILE:
        {
            FileReactor *fr = static_cast<FileReactor*>(r);
            ReactorGroup &rg = files[fr->fd];
            for (Reactor **pr = &rg.list; *pr; pr = &(*pr)->next) {
                if (*pr == r) {
                    *pr = r->next;
                    break;
                }
            }
            updateFileMask(fr->fd);
            break;
        }

    default:
        log.error("Cannot remove unknown reactor:%d\n", r->type);
    }
}

void EventDriver::flushPendingReactors ()
{
    while (pendingReactors) {
        Reactor *r = pendingReactors;
        log.fatal("running:%08x\n", r);
        int e = r->events;
        pendingReactors = r->nextPending;
        r->events = 0;
        r->processEvent(e);
    }
}

void EventDriver::handleEvent ()
{
    log.debug("handleEvent...\n");


    // Need to flush pending reactors in case of nested call.
    flushPendingReactors();

    // wait for event
    int n = select(files.size(), &inFds, &outFds, 0, 0);
    if (n == -1) {
        switch (errno) {
        default:
            log.error("select() returned errno:%d.\n", errno);
            return;
        }
    }

    // gather file reactors
    for (int i = 0; i < files.size(); i++) {
        ReactorGroup &rg = files[i];
        if (!rg.mask) {
            continue;
        }
        int m = 0;
        if (FD_ISSET(i, &inFds)) m |= EVENT_READ;
        if (FD_ISSET(i, &outFds)) m |= EVENT_WRITE;
        if (rg.mask & EVENT_READ) {
            FD_SET(i, &inFds);
        }
        if (rg.mask & EVENT_WRITE) {
            FD_SET(i, &outFds);
        }
        if (!m || !(rg.mask & m)) {
            continue;
        }

        for (Reactor *r = rg.list; r; r = r->next) {
            if (r->events) {
                r->events |= m & r->mask;
                log.warn("already in pending list (possible?)\n");
            } else {
                log.fatal("gathered file %08x\n", r);
                r->events |= m & r->mask;
                r->nextPending = pendingReactors;
                pendingReactors = r;
            }
        }
    }

    // gather async reactors
    if (FD_ISSET(asyncFd, &inFds)) {
        Reactor *r = 0;
        uint64_t c;
        read(asyncFd, &c, sizeof(c));

        while (true) {
            r = asyncReactors;
            if (__sync_bool_compare_and_swap(&asyncReactors, r, 0)) break;
            pthread_yield();
        }

        while (r) {
            log.fatal("gathered async %08x\n", r);
            r->events |= EVENT_ASYNC;
            r->nextPending = pendingReactors;
            pendingReactors = r;
            r = r->next;
        }
    } else {
        FD_SET(asyncFd, &inFds);
    }

    flushPendingReactors();
}

void EventDriver::loop ()
{
    while (true) {
        handleEvent();
    }
}

