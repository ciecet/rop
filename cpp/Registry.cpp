#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <map>
#include "rop.h"
#include "Log.h"
#include "ThreadLocal.h"

using namespace std;
using namespace acba;
using namespace rop;

THREAD_LOCAL(int, originalPortId);
THREAD_LOCAL(LocalContext, currentContext);

static int getNextPortId () {
    static int nextPortId = 0;
    return __sync_add_and_fetch(&nextPortId, 1);
}

static int getOriginalPortId () {
    int &pid = THREAD_GET(originalPortId);

    if (!pid) {
        pid = getNextPortId();
    }
    return pid;
}

int rop::currentPortId ()
{
    LocalContext &ctx = THREAD_GET(currentContext);
    if (ctx.portId) {
        return ctx.portId;
    }

    return getOriginalPortId();
}

Registry *rop::currentRegistry ()
{
    return THREAD_GET(currentContext).registry;
}

////////////////////////////////////////////////////////////////////////////////
// Registry implementations

Registry::~Registry ()
{
    log.debug("~reg()\n");
    for (map<int,Port*>::iterator i = ports.begin();
            i != ports.end(); i++) {
        delete i->second;
    }
    for (vector<Port*>::iterator i = freePorts.begin();
            i != freePorts.end(); i++) {
        delete *i;
    }

    pthread_mutex_destroy(&monitor);
}

map<string,InterfaceRef> Registry::global_exportables;

SkeletonBase *Registry::createSkeleton ()
{
    struct RegistrySkeleton: SkeletonBase {

        RegistrySkeleton (Registry *r): SkeletonBase(0) {}

        typedef Tuple<string> __args_t_0;
        typedef Tuple<int32_t,int32_t> __args_t_1;

        void readRequest (LocalCall &lc, Buffer &buf) {
            switch (buf.readI16()) {
            default: return;
            case 0:
                Reader<__args_t_0>::run(*lc.allocate<__args_t_0>(), buf);
                lc.function = (LocalCall::Function)__call_getRemote;
                break;
            case 1:
                Reader<__args_t_1>::run(*lc.allocate<__args_t_1>(), buf);
                lc.function = (LocalCall::Function)__call_notifyRemoteDestroy;
                break;
            }
        }

        static void __call_getRemote (Registry *reg, LocalCall &lc) {
            reg = lc.registry;

            __args_t_0 *args = lc.getArguments<__args_t_0>();

            ExportableMap::iterator iter =
                    reg->exportables.find(*args->at0());
            if (iter == reg->exportables.end()) {
                reg->log.debug("getRemote failed to find %s\n",
                        args->at0()->c_str());
                lc.returnIndex = -1;
                return;
            }
            SkeletonBase *skel = reg->getSkeleton(iter->second.get());
            skel->count++;
            Return<int32_t> *ret = new Return<int32_t>();
            lc.returnValue = ret;
            ret->value = skel->id;
            lc.returnIndex = 0;
        }

        static void __call_notifyRemoteDestroy (Registry *reg, LocalCall &lc) {
            reg = lc.registry;

            __args_t_1 *args = lc.getArguments<__args_t_1>();
            int32_t id = - *args->at0();
            int32_t cnt = *args->at1();

            SkeletonBase *skel = reg->skeletons[id];
            if (!skel) {
                reg->log.error("No skeleton for remove - id:%d\n", id);
                return;
            }
            skel->count -= cnt;
            if (skel->count) {
                reg->log.debug("Skipped skeleton removal. "
                        "remaining_count:%d\n", skel->count);
                return;
            }

            reg->log.debug("removing skeleton:%d\n", id);
            InterfaceRef iref = skel->object;
            reg->skeletonByExportable.erase(iref.get());
            reg->skeletons.erase(id);
            {
                ScopedUnlock ul(reg->monitor);
                iref = 0;
            }
        }
    };
    return new RegistrySkeleton(this);
}

Remote *Registry::getRemote (int id)
{
    Remote *&r = remotes[id];
    if (!r) {
        r = new Remote();
        r->id = id;
        r->registry = this;
    }
    return r;
}

Remote *Registry::getRemote (string objname)
{
    ScopedLock l(monitor);

    RemoteCall rc(0<<6, peer, 0);
    Writer<string>::run(objname, rc);

    int32_t nid;
    ReturnReadAgent<int32_t> a0(nid);
    ReadAgent *agents[] = { &a0 };
    rc.setReadAgents(agents, 1);

    if (syncCall(rc)) return 0;

    Remote *r = getRemote(-nid);
    r->count++;
    return r;
}

void Registry::notifyRemoteDestroy (int32_t id, int32_t count)
{
    ScopedLock sl(monitor);

    remotes.erase(id);

    RemoteCall rc(1<<6, peer, 1);
    rc.writeI32(id);
    rc.writeI32(count);
    asyncCall(rc, false);
}

SkeletonBase *Registry::getSkeleton (int id)
{
    map<int,SkeletonRef>::iterator i = skeletons.find(id);
    if (i == skeletons.end()) {
        return 0;
    }
    return i->second.get();
}

SkeletonBase *Registry::getSkeleton (Interface *obj)
{
    SkeletonBase *&skel = skeletonByExportable[obj];
    if (!skel) {
        skel = obj->createSkeleton();
        skel->id = nextSkeletonId++;
        skel->count = 0;
        skeletons[skel->id] = skel;
    }
    return skel;
}

Port *Registry::getPort (int pid)
{
    Port *&p = ports[pid];
    if (!p) {
        if (freePorts.empty()) {
            p = new Port(this);
        } else {
            p = freePorts.back();
            freePorts.pop_back();
        }
        p->id = pid;
    }
    if (p->id != pid) {
        log.error("PortId was broken. expected:%d got:%d\n", pid, p->id);
    }
    return p;
}

Port *Registry::getPort ()
{
    int pid;
    LocalContext &ctx = THREAD_GET(currentContext);
    if (ctx.portId > 0 || (ctx.portId < 0 && ctx.registry == this)) {
        pid = ctx.portId;
    } else {
        pid = getOriginalPortId();
    }
    return getPort(pid);
}

void Registry::tryReleasePort (Port *p)
{
    if (p->remoteCalls) return;
    if (p->id < 0) {
        if (p->hasProcessingThread) return;
        if (p->localCalls) return;
        if (p->deferredReturn) return;
    }

    if (!ports.erase(p->id)) {
        log.error("releasing invalid port.\n");
    }
    freePorts.push_back(p);
}

int32_t Registry::asyncCall (RemoteCall &rc, bool hasRet)
{
    Port *p = getPort();
    if (hasRet) {
        p->addRemoteCall(rc);
    }

    int32_t ret = transport->send(p, rc);

    tryReleasePort(p);
    return ret;
}

int Registry::syncCall (RemoteCall &rc)
{
    Port *p = getPort();
    p->addRemoteCallFront(rc);

    bool hadThread = p->hasProcessingThread;
    p->hasProcessingThread = true;

    if (transport->send(p, rc) == -1) {
        goto exit;
    }

    while (true) {
        while (p->processRequest());
        if (rc.gotReturn) break;
        if (transport->wait(p)) break; // false on disposed
    }

exit:
    p->hasProcessingThread = hadThread;
    tryReleasePort(p);
    return rc.returnIndex;
}

void Registry::run ()
{
    ScopedLock l(monitor);
    while (runningPorts) {
        Port *p = runningPorts;
        runningPorts = p->next;
        p->next = 0;
        if (!runningPorts) {
            runningPortsTail = &runningPorts;
        }

        while (p->processRequest());
        p->hasProcessingThread = false;

        tryReleasePort(p);
    }
}

////////////////////////////////////////////////////////////////////////////////
// Port implementations

Port::~Port ()
{
    pthread_cond_destroy(&updated);
    while (localCalls) {
        LocalCall *lc = localCalls;
        localCalls = lc->next;
        delete lc;
    }
}

static void *__run (void *arg)
{
    Port *p = reinterpret_cast<Port*>(arg);

    {
        ScopedLock l(p->registry->monitor);
        while (p->localCalls) {
            while (p->processRequest());
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_sec += Port::PROCESSING_THREAD_TIMEOUT / 1000;
            ts.tv_nsec += (Port::PROCESSING_THREAD_TIMEOUT % 1000) * 1000000;
            pthread_cond_timedwait(&p->updated, &p->registry->monitor, &ts);
            if (!p->localCalls) break;
        }

        p->log.info("processing thread ended. port:%d\n", p->id);
        p->hasProcessingThread = false;
        p->registry->tryReleasePort(p);
    }

    p->registry->deref();
    return 0;
}

void Port::addLocalCall (LocalCall &rc)
{
    // enqueue
    *localCallsTail = &rc;
    localCallsTail = &rc.next;;

    // process requests
    if (registry->multiThreaded) {
        if (hasProcessingThread) {
            pthread_cond_signal(&updated);
            return;
        }

        hasProcessingThread = true;
        registry->ref();
        pthread_t t;
        while (pthread_create(&t, 0, __run, this)) {
            log.warn("failed to create new thread. waiting a second...\n");

            // condvar updated will not be signalled.
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            ts.tv_sec += 1;
            pthread_cond_timedwait(&updated, &registry->monitor, &ts);
        }
        pthread_detach(t);

        log.info("processing thread started. port:%d", id);

    } else {
        if (hasProcessingThread) {
            return;
        }
        hasProcessingThread = true;
        *registry->runningPortsTail = this;
        registry->runningPortsTail = &next;
        registry->transport->requestRun();
    }
}

bool Port::processRequest () {
    LocalCall *lc = removeLocalCall();
    if (!lc) return false;

    log.debug("processing local call:%08x in port:%d\n", lc, id);

    // attach to rpc thread only when processing sync methods.
    LocalContext &ctx = THREAD_GET(currentContext);
    LocalContext oldctx = ctx;
    if (lc->messageType == MESSAGE_SYNC_CALL) {
        ctx.portId = id;
    } else {
        if (!otherId) {
            otherId = getNextPortId();
        }
        ctx.portId = otherId;
    }
    ctx.registry = registry;

    lc->function(lc->skeleton->object, *lc);

    ctx = oldctx;

    switch (lc->messageType) {
    case MESSAGE_ASYNC_CALL:
        if (lc->returnIndex == -2) break;
        // fall through (async call on sync method)

    case MESSAGE_SYNC_CALL:
        // exception was serialized already.
        lc->preWriteI8((Port::MESSAGE_RETURN << 6) |
                (lc->returnIndex & 63));
        if (lc->returnIndex == 0 && lc->returnValue) {
            lc->returnValue->write(*lc);
        }
        registry->transport->send(this, *lc);
        deferredReturn = 0;
        break;
    case MESSAGE_CHAINED_CALL:
        deferredReturn = lc->returnValue;
        break;
    }

    {
        ScopedUnlock su(registry->monitor);
        delete lc;
    }
    return true;
}

int Registry::recv (Port *p, Buffer &buf)
{
    int8_t h = buf.readI8();

    if (((h>>6) & 3) == Port::MESSAGE_RETURN) {
        RemoteCall *rc = p->removeRemoteCall();
        if (!rc) {
            log.error("No remote call waiting.\n");
            return -1;
        }

        rc->readReturn(h, buf);
        if (buf.size()) {
            log.warn("Return message has trailing %d bytes.\n", buf.size());
        }
        buf.clear();

        rc->gotReturn = true;
        pthread_cond_signal(&p->updated);
        log.debug("got return of RemoteCall:%08x index:%d.\n",
                rc, rc->returnIndex);
    } else {
        int32_t oid = -buf.readI32();
        SkeletonBase *skel = getSkeleton(oid);
        if (!skel) {
            log.error("No such skeleton.\n");
            return -1;
        }

        LocalCall *lc = new LocalCall(h, skel, *this);
        skel->readRequest(*lc, buf);
        if (buf.size()) {
            log.warn("Request message has trailing %d bytes.\n", buf.size());
        }
        buf.clear();

        log.debug("got a request of LocalCall:%08x oid:%d h:%d port:%d\n",
                lc, skel->id, h, p->id);
        p->addLocalCall(*lc);
    }
    return 0;
}

void Registry::dispose () {
    log.info("dispose()\n");

    // flush listeners
    set<RegistryListener*> lnrs;
    lnrs.swap(listeners);
    {
        ScopedUnlock su(monitor);
        for (set<RegistryListener*>::iterator i = lnrs.begin();
                i != lnrs.end(); i++) {
            (*i)->registryDisposed(this);
        }
    }

    // remove skeletons
    map<int,SkeletonRef> skels;
    skels.swap(skeletons);
    skeletonByExportable.clear();
    {
        ScopedUnlock su(monitor);
        skels.clear();
    }

    // notify port threads
    for (map<int,Port*>::iterator i = ports.begin(); i != ports.end(); i++) {
        Port *p = i->second;
        if (p->hasProcessingThread) {
            pthread_cond_signal(&p->updated);
        }
    }

    peer.registry = 0;
}

void *Registry::getLocal (std::string var) {
    ScopedLock sl(monitor);
    map<std::string,void*>::iterator i = localData.find(var);
    if (i == localData.end()) {
        return 0;
    }
    return i->second;
}

void Registry::setLocal (std::string var, void *val) {
    ScopedLock sl(monitor);
    localData[var] = val;
}

void Registry::addRegistryListener (RegistryListener *dl) {
    ScopedLock sl(monitor);
    listeners.insert(dl);
}

void Registry::removeRegistryListener (RegistryListener *dl) {
    ScopedLock sl(monitor);
    listeners.erase(dl);
}

Remote::~Remote ()
{
    if (registry) { // could be null in peer object.
        registry->notifyRemoteDestroy(id, count);
    }
}

Context::Context ()
{
    override.portId = getNextPortId();
    override.registry = 0;
    pthread_mutex_init(&mutex, 0);
}

Context::~Context ()
{
    pthread_mutex_destroy(&mutex);
}

Context::Lock::Lock (Context *ctx): context(ctx)
{
    pthread_mutex_lock(&ctx->mutex);

    LocalContext &current = THREAD_GET(currentContext);

    ctx->backup = current;
    current = ctx->override;
}

Context::Lock::~Lock ()
{
    THREAD_GET(currentContext) = context->backup;

    pthread_mutex_unlock(&context->mutex);
}
