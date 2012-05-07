#include <pthread.h>
#include <map>
#include "Remote.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;

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

Port *Transport::getPort ()
{
    static int nextId = 1;

    if (!rpcThreadId) {
        rpcThreadId = nextId++;
    }
    return getPort(rpcThreadId);
}

Port *Transport::getPort (int pid)
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

    MessageReader (Port *p): port(p), request(0), log("msgrdr ") {}
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
                // fail
                return ABORTED;
            }
            stack->push(ret);
            CALL();
            // TODO: notify that return value is available.
            port->returns.pop_front();
            if (port->returns.empty()) {
                pthread_cond_signal(&port->wakeCondition);
            }
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

            if (port->isWaiting) {
                log.debug("wake up waiting thread to execute the request.\n");
                pthread_cond_signal(&port->wakeCondition);
            } else {
                log.debug("notifyUnhandledRequest...\n");
                port->transport->notifyUnhandledRequest(port);
            }
        }

        // keep reading if buffer is not empty. (optional)
        if (static_cast<PortStack*>(stack)->buffer->size) {
            step = 0;
        }
        END_STEP();
    }
};

Frame::STATE Port::read (Buffer *buf) {
    if (!reader.frame) {
        reader.push(new(reader.allocate(sizeof(MessageReader))) MessageReader(this));
    }

    reader.buffer = buf;
    return reader.run();
}

Frame::STATE Port::write (Buffer *buf) {
    writer.buffer = buf;
    return writer.run();
}

void Port::flush ()
{
    transport->flushPort(this);
}

void Port::addReturn (Return *ret) {
    returns.push_back(ret);
}

void Port::flushAndWait ()
{
    Log l("f&w ");
    pthread_mutex_t *m = &transport->monitor;
    
    isWaiting = true;
    transport->flushPort(this);
    
    l.debug("waiting for reply\n");
    pthread_mutex_lock(m);
    while (true) {
        if (!requests.empty()) {
            l.info("running reenterant request...\n");
            handleRequest();
            continue;
        }
        if (returns.empty()) {
            break;
        }

        l.debug("calling waitPort in thread:%d\n", rpcThreadId);
        transport->waitPort(this);
    }
    pthread_mutex_unlock(m);
    isWaiting = false;
}

bool Port::handleRequest () {
    if (requests.empty()) {
        return false;
    }

    Log l("exec ");

    Request *req = requests.front();
    Request *lreq = lastRequest;
    requests.pop_front();
    lastRequest = 0;
    
    pthread_mutex_unlock(&transport->monitor);
    int ortid = rpcThreadId;
    rpcThreadId = id; // attach to rpc thread while executing the request.
    l.debug("calling!!! %08x\n", req);
    req->call();
    rpcThreadId = ortid;

    if (lreq) {
        delete lreq;
    }
    lastRequest = req;

    if (req->returnWriter) {
        l.debug("pushing return writer %08x (previous frame:%08x)\n", req->returnWriter, writer.frame);
        writer.push(req->returnWriter);
        transport->flushPort(this);
    }
    pthread_mutex_lock(&transport->monitor);
    return true;
}
