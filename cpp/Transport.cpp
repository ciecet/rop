#include <pthread.h>
#include <map>
#include "Remote.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;

static __thread int localThreadId;
static __thread int rpcThreadId;

Transport::~Transport ()
{
    for (map<int,Port*>::iterator i = ports.begin();
            i != ports.end(); i++) {
        delete i->second;
    }
    for (vector<Port*>::iterator i = freePorts.begin();
            i != freePorts.end(); i++) {
        delete *i;
    }
    registry->setTransport(0);
    pthread_mutex_destroy(&monitor);
}

Port::~Port ()
{
    pthread_cond_destroy(&wakeCondition);
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

Port *Transport::getPort ()
{
    return getPort(rpcThreadId ? rpcThreadId : currentThreadId());
}

Port *Transport::getPort (int pid)
{
    pthread_mutex_lock(&monitor);
    return getPortWithLock(pid);
}

Port *Transport::getPortWithLock (int pid)
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

void Transport::releasePort (Port *p)
{
    releasePortWithLock(p);
    pthread_mutex_unlock(&monitor);
}

void Transport::releasePortWithLock (Port *p)
{
    if (p->isActive()) {
        return;
    }
    //ports.erase(p->id);
    //freePorts.push_back(p);
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
            stack->push(ret);
            CALL();
            ret->isValid = true;
            // TODO: notify that return value is available. (for future)
            port->returns.pop_front();
            pthread_cond_signal(&port->wakeCondition);
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
                SkeletonBase *skel = port->transport->registry->getSkeleton(
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

            if (port->canProcessRequests) {
                log.debug("wake up waiting thread to execute the request.\n");
                pthread_cond_signal(&port->wakeCondition);
            } else {
                log.debug("notifyUnhandledRequest...\n");
                port->transport->notifyUnhandledRequest(port);
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
    transport->send(this);
    transport->releasePort(this);
}

void Port::sendAndWait (Return *ret)
{
    Log l("f&w ");
    
    canProcessRequests = true;
    returns.push_back(ret);

    transport->send(this);
    
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
        transport->receive(this);
    }
    canProcessRequests = false;

    transport->releasePort(this);
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
    
    pthread_mutex_unlock(&transport->monitor);

    l.debug("calling!!! %08x\n", req);
    int otid = rpcThreadId;
    // attach to rpc thread only when processing sync methods.
    rpcThreadId = (req->messageHead & (0x1<<6)) ? currentThreadId() : id;
    req->call();
    rpcThreadId = otid;

    pthread_mutex_lock(&transport->monitor);

    if (lreq) {
        delete lreq;
    }
    lastRequest = req;

    if (req->returnWriter) {
        l.debug("pushing return writer %08x (previous frame:%08x)\n", req->returnWriter, writer.frame);
        writer.push(req->returnWriter);
        transport->send(this);
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

    return false;
}
