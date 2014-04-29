#include <assert.h>
#include <unistd.h>
#include <sched.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/select.h>
#include "EventDriver.h"
#include "MZResult.h"
#include "mz_server.h"

using namespace acba;

EventDriver::EventDriver (MZEventDispatcher *edt): log("evt ") {
    m_edt = (edt) ? edt : mz_getUserEventDispatcher();
    m_group = m_edt->openGroup();

    pipe(timeoutFds);
    fcntl(timeoutFds[0], F_SETFL, O_NONBLOCK);
    m_edt->addModule(timeoutFds[0], MZEventModule::EVENT_READ |
            MZEventModule::EVENT_SAFE, this);
}

EventDriver::~EventDriver ()
{
    m_edt->removeModule(timeoutFds[0]);
    close(timeoutFds[0]);
    close(timeoutFds[1]);
    m_edt->closeGroup(m_group);
}

static int toMZMask(int evDriverMask, bool safe) {
    int mask = 0;
    if(evDriverMask & acba::EVENT_WRITE) {
        mask |= MZEventModule::EVENT_WRITE;
    }
    if(evDriverMask & acba::EVENT_READ) {
        mask |= MZEventModule::EVENT_READ;
    }
    mask |= MZEventModule::EVENT_ERROR;
    if (!safe) {
        mask |= MZEventModule::EVENT_SAFE;
    }
    return mask;
}

static int toEDMask(int mzMask) {
    int mask = 0;
    if(mzMask & MZEventModule::EVENT_WRITE) {
        mask |= acba::EVENT_WRITE;
    }
    if(mzMask & MZEventModule::EVENT_READ) {
        mask |= acba::EVENT_READ;
    }
    return mask;
}

void EventDriver::watchFile (int fd, int fmask, Reactor *r) {
    if (!r->eventDriver) r->eventDriver = this;
    assert(r->eventDriver == this);
    assert(fd >= 0);

    if (r->fd == -1) {
        r->fd = fd;
        m_edt->addModule(fd, toMZMask(fmask, r->safe), r);
        m_edt->addFDToGroup(m_group, fd);
    } else {
        assert(r->fd == fd);
        m_edt->setModuleEvents(fd, toMZMask(fmask, r->safe));
    }

    r->mask &= ~(EVENT_READ | EVENT_WRITE);
    r->mask |= EVENT_FILE | (fmask & (EVENT_READ | EVENT_WRITE));
}

void EventDriver::tryUnbind (Reactor *r) {
    // unbind reactor if no events are in interest nor scheduled.
    if (!r->mask) {
        r->eventDriver = 0;
    }
}

void EventDriver::unwatchFile (Reactor *r)
{
    if (!r->eventDriver) return;
    assert(r->eventDriver == this);
    assert(isEventThread());

    if (r->fd == -1) return;

    log.debug("unwatch fd:%d r:%08x\n", r->fd, r);

    m_edt->removeFDFromGroup(m_group, r->fd);
    m_edt->removeModule(r->fd);

    r->mask &= ~(EVENT_FILE | EVENT_READ | EVENT_WRITE);
    r->fd = -1;
    tryUnbind(r);
}

void EventDriver::setTimeout (int64_t usec, Reactor *r)
{
    if (!r->eventDriver) r->eventDriver = this;
    assert(r->eventDriver == this);

    if (r->mask & EVENT_TIMEOUT) {
        timeoutReactors.remove(r);
    }
    r->mask |= EVENT_TIMEOUT;
    timeoutReactors.addBack(r);

    {
        char c = 0;
        write(timeoutFds[1], &c, 1);
    }
}

void EventDriver::unsetTimeout (Reactor *r)
{
    if (!r->eventDriver) return;
    assert(r->eventDriver == this);
    assert(isEventThread());

    if (!(r->mask & EVENT_TIMEOUT)) return;

    r->mask &= ~EVENT_TIMEOUT;
    timeoutReactors.remove(r);

    tryUnbind(r);
}

void EventDriver::unbind (Reactor *r)
{
    if (!r->eventDriver) return;
    assert(r->eventDriver == this);

    if (r->mask & EVENT_FILE) unwatchFile(r);
    if (r->mask & EVENT_TIMEOUT) unsetTimeout(r);

    r->eventDriver = 0;
}

void EventDriver::dispatchEvent (MZEventDispatcher* dispatcher, int events)
{
    Reactor *r;
    while ((r = timeoutReactors.removeFront())) {
        r->mask &= ~EVENT_TIMEOUT;
        tryUnbind(r);

        log.debug("process timeout event. mask:%d r:%08x\n", r->mask, r);
        r->processEvent(EVENT_TIMEOUT);
    }

    {
        // consume event
        char c;
        while (read(timeoutFds[0], &c, sizeof(c)) > 0);
    }
}

void EventDriver::wait (int64_t usec)
{
    bool timerWasEnabled = m_edt->enableTimer(false);

    MZEvent event;
    MZ_RESULT ret;
    if((ret = m_edt->waitForGroup(m_group, &event,
            (usec == 0) ? -1 : (int)(usec / 1000))) != MZ_OK) {
        log.error("Returned error(%d) from mzEdt::waitForGroup()!\n", ret);
        goto exit;
    }
    if(m_edt->dispatch(&event) != MZ_OK) {
        log.error("Error on dispatching MZEvent\n");
    }

exit:
    m_edt->enableTimer(timerWasEnabled);
}

void EventDriver::run ()
{
    m_edt->loop();
}

void Reactor::dispatchEvent (MZEventDispatcher* dispatcher, int events)
{
    EventDriver *ed = getEventDriver();
    assert(ed);

    events = toEDMask(events);
    log.debug("process file event. ev:%d mask:%d r:%08x\n", events, mask,
            this);
    processEvent(events);
}
