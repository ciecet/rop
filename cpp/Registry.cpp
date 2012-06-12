#include <pthread.h>
#include <map>
#include "Remote.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;

static __thread int localThreadId;
static __thread int rpcThreadId;

Port::~Port ()
{
    pthread_cond_destroy(&updated);
    for (deque<Request*>::iterator i = requests.begin();
            i != requests.end(); i++) {
        delete *i;
    }
    if (lastRequest) {
        delete lastRequest;
    }
}

static int currentThreadId ()
{
    static int nextId = 1;
    if (!localThreadId) {
        localThreadId = nextId++;
    }
    return localThreadId;
}


struct RegistrySkeleton: SkeletonBase {
    Registry *registry;

    RegistrySkeleton (Registry *r): SkeletonBase(0), registry(r) {}

    struct __req_getRemote: Request {
        Registry *registry;
        ArgumentsReader<string> args;
        ReturnWriter<int32_t> ret;
        __req_getRemote(Registry *reg): registry(reg) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            Log l("getRemote ");
            l.debug("finding %s\n", args.get<string>(0).c_str());

            registry->lock();

            map<string,InterfaceRef>::iterator iter =
                    registry->exportables.find(args.get<string>(0));
            if (iter == registry->exportables.end()) {
                ret.index = -1;
                l.debug("not found\n");
                registry->unlock();
                return;
            }
            SkeletonBase *skel = registry->getSkeleton(iter->second.get());
            skel->count++;
            ret.get<int32_t>(0) = skel->id;
            ret.index = 0;
            l.debug("found %08x(%d)\n", skel, skel->id);

            registry->unlock();
        }
    };

    struct __req_notifyRemoteDestroy: Request {
        Registry *registry;
        ArgumentsReader<int32_t, int32_t> args;
        __req_notifyRemoteDestroy(Registry *reg): registry(reg) {
            argumentsReader = &args;
            returnWriter = 0;
        }

        void call () {
            Log l("notifyRemoteDestroy ");
            int32_t id = -args.get<int32_t>(0); // reverse local<->remote
            int32_t count = args.get<int32_t>(1);

            registry->lock();

            SkeletonBase *skel = registry->skeletons[id];
            skel->count -= count;
            if (skel->count) {
                l.debug("Skipped skeleton deletion.\n");
                registry->unlock();
                return;
            }

            l.debug("deleting skeleton:%d\n", id);
            registry->skeletons.erase(id);
            registry->skeletonByExportable.erase(skel->object.get());

            registry->unlock();
            delete skel;
        }
    };

    Request *createRequest (int methodIndex) {
        switch (methodIndex) {
        case 0: return new __req_getRemote(registry);
        case 1: return new __req_notifyRemoteDestroy(registry);
        default: return 0;
        }
    }
};

void Registry::setTransport (Transport *trans) {
    transport = trans;
    if (trans) {
        trans->registry = this;
    }
}

SkeletonBase *Registry::createSkeleton ()
{
    return new RegistrySkeleton(this);
}

Remote *Registry::getRemote (string objname)
{
    Port* p = getPort();

    RequestWriter<1> req(0, 0, 0);
    Writer<string> arg0(objname);
    req.args[0] = &arg0;
    p->writer.push(&req);

    ReturnReader<int32_t> ret;
    p->sendAndWait(&ret); // may throw exception

    lock();
    Remote *r = getRemote(-ret.get<int32_t>(0)); // reverse local<->remote
    r->count++;
    unlock();
    return r;
}

void Registry::notifyRemoteDestroy (int32_t id, int32_t count)
{
    remotes.erase(id);

    Port *p = getPort(0);

    Writer<int32_t> arg0(id);
    Writer<int32_t> arg1(count);
    RequestWriter<2> req(0x1<<6, 0, 1);
    req.args[0] = &arg0;
    req.args[1] = &arg1;
    p->writer.push(&req);
    p->send(0);
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

SkeletonBase *Registry::getSkeleton (int id)
{
    map<int,SkeletonBase*>::iterator i = skeletons.find(id);
    if (i == skeletons.end()) {
        return 0;
    }
    return i->second;
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

Registry::~Registry ()
{
    Log l("~reg ");

    for (map<int,Port*>::iterator i = ports.begin();
            i != ports.end(); i++) {
        delete i->second;
    }
    for (vector<Port*>::iterator i = freePorts.begin();
            i != freePorts.end(); i++) {
        delete *i;
    }

    setTransport(0);

    // Don't send notifyRemoteDestroy because peer's skeletons will be deleted
    // anyway after connection-loss detection.
    for (map<int,Remote*>::iterator i = remotes.begin();
            i != remotes.end(); i++) {
        i->second->registry = 0;
    }
    for (map<int,SkeletonBase*>::iterator i = skeletons.begin();
            i != skeletons.end(); i++) {
        l.debug("removing skel:%d\n", i->second->id);
        delete i->second;
    }

    pthread_mutex_destroy(&monitor);
}

Port *Registry::getPort ()
{
    return getPort(rpcThreadId ? rpcThreadId : currentThreadId());
}

Port *Registry::getPort (int pid)
{
    lock();
    return unsafeGetPort(pid);
}

Port *Registry::unsafeGetPort (int pid)
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
    return p;
}

void Registry::releasePort (Port *p)
{
    unsafeReleasePort(p);
    unlock();
}

void Registry::unsafeReleasePort (Port *p)
{
    if (p->isActive()) {
        return;
    }
    ports.erase(p->id);
    freePorts.push_back(p);
}

// switch (MSB of messageHead)
// 00: sync call (reenterant)
// 01: async call (including call variation using future pattern)
// 10: chained call
// 11: return
struct MessageReader: Frame {

    int8_t messageHead;
    int32_t objectId;
    int16_t methodIndex;
    Request *request;
    Return *ret;
    Port* port;
    Log log;

    MessageReader (Port *p): request(0), port(p), log("msgrdr ") {}
    ~MessageReader () {
        if (request) {
            delete request;
        }
    }

    STATE run (Stack *stack) {
        log.debug("run step:%d\n", step);

        BEGIN_STEP();
        TRY_READ(int8_t, messageHead, stack);

        if ((messageHead & (0x3 << 6)) == (0x3 << 6)) {
            log.debug("Getting return value...\n");
            // getting return message
            ret = port->returns.front();
            if (!ret) {
                // no waiting return!
                return ABORTED;
            }
            port->returns.pop_front();
            if ((messageHead & 63) == 63) {
                // initially -1. ret->index = -1;
            } else {
                ret->index = messageHead & 63;
                stack->push(ret);
                CALL();
            }
            ret->isValid = true;
            // TODO: notify that return value is available. (for future)
            pthread_cond_signal(&port->updated);
        } else {
            log.debug("Getting request...\n");
            // getting request message
            NEXT_STEP();
            TRY_READ(int32_t, objectId, stack);
            objectId = -objectId; // reverse local <-> remote
            if (objectId < 0) {
                log.error("Aborting... expected non-negative object id:%d\n",
                        objectId);
                return ABORTED;
            }

            NEXT_STEP();
            TRY_READ(int16_t, methodIndex, stack);
            log.debug("Requesting on oid:%d mid:%d\n", objectId, methodIndex);

            {
                SkeletonBase *skel = port->registry->getSkeleton(
                        objectId);
                if (!skel) {
                    // fail
                    log.error("Aborting... skeleton not found\n");
                    return ABORTED;
                }
                request = skel->createRequest(methodIndex);
                request->messageHead = messageHead;
                log.debug("created a request instance. %08x\n", request);
            }

            if (request->argumentsReader) {
                stack->push(request->argumentsReader);
                log.debug("reading arguments...\n");
                CALL();
                log.debug("reading arguments...done \n");
            }
            port->requests.push_back(request);
            request = 0;

            if (port->waitingSyncReturn) {
                log.debug("wake up waiting thread to execute the request.\n");
                pthread_cond_signal(&port->updated);
            } else {
                log.debug("notifyUnhandledRequest...\n");
                port->registry->transport->notifyUnhandledRequest(port);
            }
        }
        END_STEP();
    }
};

Frame::STATE Port::encode (Buffer *buf) {
    writer.buffer = buf;
    return writer.run();
}

Frame::STATE Port::decode (Buffer *buf) {
    if (!reader.frame) {
        reader.push(new(reader.allocate(sizeof(MessageReader))) MessageReader(this));
    }

    reader.buffer = buf;
    return reader.run();
}

void Port::send (Return *ret)
{
    if (ret) {
        returns.push_back(ret);
    }
    registry->transport->send(this);
    registry->releasePort(this);
}

void Port::sendAndWait (Return *ret)
{
    Log l("f&w ");
    
    bool wasWaiting = waitingSyncReturn;
    waitingSyncReturn = true;
    returns.push_back(ret);

    registry->transport->send(this);
    
    l.debug("waiting for reply\n");
    while (true) {
        if (!requests.empty()) {
            l.info("running reenterant request...\n");
            processRequest();
            continue;
        }
        if (ret->isValid) {
            break;
        }
    
        l.debug("waiting to receive from the port:%d\n", id);
        registry->transport->receive(this);
    }
    waitingSyncReturn = wasWaiting;

    registry->releasePort(this);
}

bool Port::processRequest () {
    if (requests.empty()) {
        return false;
    }

    Log l("exec ");

    Request *req = requests.front();
    Request *lreq = lastRequest;
    requests.pop_front();
    lastRequest = 0;
    
    registry->unlock();

    l.debug("calling %08x\n", req);
    int otid = rpcThreadId;
    // attach to rpc thread only when processing sync methods.
    rpcThreadId = (req->messageHead & (0x1<<6)) ? currentThreadId() : id;
    req->call();
    rpcThreadId = otid;
    if (lreq) {
        delete lreq;
    }

    registry->lock();

    lastRequest = req;

    if (req->returnWriter) {
        l.debug("pushing return writer %08x (previous frame:%08x)\n", req->returnWriter, writer.frame);
        writer.push(req->returnWriter);
        registry->transport->send(this);
    }

    return true;
}

bool Port::isActive ()
{
    if (!returns.empty()) {
        return true;
    }
    if (!requests.empty()) {
        return true;
    }
    if (writer.frame || reader.frame) {
        return true;
    }
    if (waitingSyncReturn) {
        return true;
    }

    return false;
}
