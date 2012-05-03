#include <pthread.h>
#include "Remote.h"

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

// private
static Frame::STATE run (Stack *stack) {

    while (stack->frame) {
        switch (stack->frame->run(stack)) {
        case Frame::STOPPED:
            return Frame::STOPPED;
        case Frame::CONTINUE:
            break;
        case Frame::COMPLETE:
            stack->pop();
            break;
        case Frame::ABORTED:
            while (stack->frame) stack->pop();
            return Frame::ABORTED;
        default:
            // should never occur
            ;
        }
    }
    return Frame::COMPLETE;
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

    MessageReader (Port *p): port(p), request(0) {}
    ~MessageReader () {
        if (request) {
            delete request;
        }
    }

    STATE run (Stack *stack) {
        Buffer *buf;

        BEGIN_STEP();
        TRY_READ(int8_t, messageHead, stack);

        if ((messageHead & (0x3 << 6)) == (0x3 << 6)) {
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
            // getting request message
            NEXT_STEP();
            TRY_READ(int32_t, objectId, stack);
            if (objectId >= 0) {
                // id value must be negative in the view of the remote peer.
                return ABORTED;
            }

            NEXT_STEP();
            TRY_READ(int16_t, methodIndex, stack);

            {
                SkeletonBase *skel = port->transport->registry->getSkeleton(
                        -objectId);
                if (!skel) {
                    // fail
                    return ABORTED;
                }
                request = skel->createRequest(methodIndex);
                request->messageHead = messageHead;
            }
            stack->push(request->argumentsReader);
            CALL();
            port->requests.push_back(request);
            request = 0;

            if (port->isWaiting) {
                pthread_cond_signal(&port->wakeCondition);
            } else {
                port->transport->notifyUnhandledRequest(port);
            }
        }

        // keep reading if buffer is not empty. (optional)
        buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->size) {
            step = 0;
        }
        END_STEP();
    }
};

Frame::STATE Port::read (Buffer *buf) {
    if (!reader.frame) {
        reader.push(new(reader.allocate(sizeof(MessageReader))) MessageReader(this));
    }

    reader.env = buf;
    return run(&reader);
}

Frame::STATE Port::write (Buffer *buf) {
    writer.env = buf;
    return run(&writer);
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
    pthread_mutex_t *m = &transport->monitor;
    
    isWaiting = true;
    transport->flushPort(this);
    
    pthread_mutex_lock(m);
    while (!returns.empty()) {
        if (requests.empty()) {
            pthread_cond_wait(&wakeCondition, m);
        }

        pthread_mutex_unlock(m);
        // handle reenterant request
        // TODO: implement
        pthread_mutex_lock(m);
        continue;
    }
    pthread_mutex_unlock(m);
    isWaiting = false;
}

bool Port::handleRequest () {
    if (requests.empty()) {
        return false;
    }

    Request *req = requests.front();
    int ortid = rpcThreadId;
    rpcThreadId = id; // attach to rpc thread while executing the request.
    req->call();
    rpcThreadId = ortid;
    if (lastRequest) {
        delete lastRequest;
    }
    lastRequest = req;
    requests.pop_front();

    writer.push(req->returnWriter);
    flush();
}
