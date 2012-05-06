#ifndef REMOTE_H
#define REMOTE_H

#include <stdint.h>
#include <pthread.h>
#include <new>
#include <map>
#include <vector>
#include <string>
#include <deque>
#include "Ref.h"
#include "Stack.h"
#include "Tuple.h"
#include "Log.h"

namespace rop {

/**
 * Byte queue buffer with fixed size.
 * io operation is done against this structure.
 */
struct Buffer {

    static const int BUFFER_SIZE = 4*1024;

    int offset;
    int size;
    unsigned char buffer[BUFFER_SIZE];

    Buffer(): offset(0), size(0) { }

    unsigned char read () {
        unsigned char ret = buffer[offset];
        offset = (offset+1) % BUFFER_SIZE;
        size--;
        return ret;
    }

    void write (unsigned char d) {
        buffer[(offset+size) % BUFFER_SIZE] = d;
        size++;
    }

    int margin () {
        return sizeof(buffer) - size;
    }

    void reset () {
        offset = 0;
        size = 0;
    }

    void drop (int s) {
        offset = (offset+s) % BUFFER_SIZE;
        size -= s;
    }

    unsigned char *begin () {
        return &buffer[offset];
    }

    unsigned char *end () {
        return &buffer[(offset+size) % BUFFER_SIZE];
    }

    bool hasWrappedData () {
        return (offset + size) > BUFFER_SIZE;
    }

    bool hasWrappedMargin () {
        return (offset > 0) && (offset + size) < BUFFER_SIZE;
    }
};

struct Port;
struct PortStack: base::Stack {
    Port *port;
    Buffer *buffer;
    PortStack (Port *p): port(p), buffer(0) {}
};

/**
 * Default template class for Reader. (empty)
 */
template <typename T> struct Reader {
    Reader (T &o) {}
};

/**
 * Default template class for Writer. (empty)
 */
template <typename T> struct Writer {
    Writer (T &o) {}
};

// Utility macros for convenient management of step variable.
// TRY_READ/TRY_WRITE should be used only for primitive types.
#define BEGIN_STEP() switch (step) { case 0:
#define TRY_READ(type,var,stack) do {\
    if (Reader<type>(var).run(stack) == STOPPED) {\
        return STOPPED;\
    }\
} while (0)
#define TRY_WRITE(type,var,buf) do {\
    if (Writer<type>(var).run(stack) == STOPPED) {\
        return STOPPED;\
    }\
} while (0)
#define NEXT_STEP() step = __LINE__; case __LINE__: 
#define CALL() step = __LINE__; return CONTINUE; case __LINE__: 
#define END_STEP() default:; } return COMPLETE

template<>
struct Reader<void>: base::Frame {
    STATE run (base::Stack *stack) { return COMPLETE; }
};

template<>
struct Writer<void>: base::Frame {
    STATE run (base::Stack *stack) { return COMPLETE; }
};

template<>
struct Reader<int8_t>: base::Frame {
    int8_t &obj;
    Reader (int8_t &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->size < 1) return STOPPED;
        obj = buf->read();
        return COMPLETE;
    }
};

template<> struct Writer<int8_t>: base::Frame {
    int8_t &obj;
    Writer (int8_t &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->margin() < 1) return STOPPED;
        buf->write(obj);
        return COMPLETE;
    }
};

template<> struct Reader<int16_t>: base::Frame {
    int16_t &obj;
    Reader (int16_t &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->size < 2) return STOPPED;
        int i = buf->read();
        i = (i<<8) + buf->read();
        obj = i;
        return COMPLETE;
    }
};

template<> struct Writer<int16_t>: base::Frame {
    int16_t &obj;
    Writer (int16_t &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->margin() < 2) return STOPPED;
        int i = obj;
        buf->write(i>>8);
        buf->write(i);
        return COMPLETE;
    }
};

template<> struct Reader<int32_t>: base::Frame {
    int32_t &obj;
    Reader (int32_t &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->size < 4) return STOPPED;
        int i = buf->read();
        i = (i<<8) + buf->read();
        i = (i<<8) + buf->read();
        i = (i<<8) + buf->read();
        obj = i;
        return COMPLETE;
    }
};

template<> struct Writer<int32_t>: base::Frame {
    int32_t &obj;
    Writer (int32_t &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->margin() < 4) return STOPPED;
        int i = obj;
        buf->write(i>>24);
        buf->write(i>>16);
        buf->write(i>>8);
        buf->write(i);
        return COMPLETE;
    }
};

template<> struct Reader<std::string>: base::Frame {

    std::string &obj;
    int length;

    Reader (std::string &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *buf;

        // read size
        BEGIN_STEP();
        TRY_READ(int32_t, length, stack);
        obj.clear();
        obj.reserve(length);

        // read each char
        NEXT_STEP();
        buf = static_cast<PortStack*>(stack)->buffer;
        while (length) {
            if (!buf->size) {
                return STOPPED;
            }
            obj.push_back(static_cast<char>(buf->read()));
            length--;
        }

        END_STEP();
    }
};

template<> struct Writer<std::string>: base::Frame {

    std::string &obj;
    int i;

    Writer (std::string &o): obj(o) {}

    STATE run (base::Stack *stack) {
        Buffer *buf;

        // write size 
        BEGIN_STEP();
        i = obj.size();
        TRY_WRITE(int32_t, i, stack);
        i = 0;

        // write chars
        NEXT_STEP();
        buf = static_cast<PortStack*>(stack)->buffer;
        while (i < obj.length()) {
            if (!buf->margin()) {
                return STOPPED;
            }
            buf->write(obj.at(i++));
        }

        END_STEP();
    }
};

template<typename T> struct Reader<std::vector<T> >: base::Frame {

    std::vector<T> &obj;
    int length;
    int i;

    Reader (std::vector<T> &o): obj(o) {}

    STATE run (base::Stack *stack) {

        // read size
        BEGIN_STEP();
        TRY_READ(int32_t, length, stack);
        obj.clear();
        obj.resize(length);
        i = 0;

        // read items
        NEXT_STEP();
        if (i < length) {
            stack->push(new(stack->allocate(sizeof(Reader<T>))) Reader<T>(obj[i++]));
            return CONTINUE;
        }

        END_STEP();
    }
};

template<typename T> struct Writer<std::vector<T> >: base::Frame {

    std::vector<T> &obj;
    int i;

    Writer (std::vector<T> &o): obj(o) {}

    STATE run (base::Stack *stack) {

        // write size
        BEGIN_STEP();
        i = obj.size();
        TRY_WRITE(int32_t, i, stack);
        i = 0;

        // write items
        NEXT_STEP();
        if (i < obj.size()) {
            stack->push(new(stack->allocate(sizeof(Writer<T>))) Writer<T>(obj[i++]));
            return CONTINUE;
        }

        END_STEP();
    }
};

template<typename T, typename U> struct Reader<std::map<T,U> >: base::Frame {
    std::map<T,U> &obj;
    int n;
    T key;

    Reader (std::map<T,U> &d): obj(d) {}

    STATE run (base::Stack *stack) {

        BEGIN_STEP();
        // read size
        TRY_READ(int32_t, n, stack);
        obj->clear();
        n *= 2;

        NEXT_STEP();
        if (n) {
            if ((n & 1) == 0) {
                // read key
                stack->push(new(sizeof(Reader<T>)) Reader<T>(key));
                n--;
                return CONTINUE;
            } else {
                // read value
                stack->push(new (sizeof(Reader<U>)) Reader<U>(obj[key]));
                n--;
                return CONTINUE;
            }
        }

        END_STEP();
    }
};

template<typename T, typename U> struct Writer<std::map<T,U> >: base::Frame {
    std::map<T,U> &obj;
    typename std::map<T,U>::iterator iter;
    bool first;

    Writer (std::map<T,U> &obj): obj(obj) {}

    STATE run (base::Stack *stack) {

        // write size
        BEGIN_STEP();
        int size = obj->size();
        TRY_WRITE(int32_t, size, stack);
        iter = obj->begin();
        first = true;

        // write key and value
        NEXT_STEP();
        if (iter != obj->end()) {
            if (first) {
                stack->push(new(stack->allocate(sizeof(Writer<T>))) Writer<T>(
                        const_cast<T*>(&iter->first)));
                first = false;
                return CONTINUE;
            } else {
                stack->push(new(stack->allocate(sizeof(Writer<U>))) Writer<U>(iter->second));
                iter++;
                first = true;
                return CONTINUE;
            }
        }

        END_STEP();
    }
};

template <typename T>
struct Reader<base::Ref<T> >: base::Frame {
    base::Ref<T> &obj;
    Reader (base::Ref<T> &o): obj(o) {}
    STATE run (base::Stack *stack) {
        int i;
        BEGIN_STEP();
        TRY_READ(int8_t, i, stack);
        if (i) {
            obj = new T();
            stack->push(new (stack->allocate(sizeof(Reader<T>))) Reader<T>(*obj));
            CALL();
        } else {
            obj = 0;
        }
        END_STEP();
    }
};

template <typename T>
struct Writer<base::Ref<T> >: base::Frame {
    base::Ref<T> &obj;
    Writer (base::Ref<T> &o): obj(o) {}

    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        if (obj) {
            stack->push(new (stack->allocate(sizeof(Writer<T>))) Writer<T>(*obj));
            CALL();
        } else {
            int i = 0;
            TRY_WRITE(int8_t, i, stack);
        }

        END_STEP();
    }
};

template <typename T>
struct Skeleton {};

template <typename T>
struct Stub {};

struct Transport;
struct Port;
struct Registry;
struct Interface;
struct Remote;
struct SkeletonBase;
struct Request;
struct Return;
typedef base::Ref<Interface> InterfaceRef;
typedef base::Ref<Remote> RemoteRef;

/**
 * Super class of every skeletons.
 * Each instance corresponds to a raw object. 
 * This is destroyed when notified by notifyRemoteDestroy().
 */
struct SkeletonBase: base::RefCounted<SkeletonBase> {
    int id; // positive.
    InterfaceRef object;
    SkeletonBase (Interface *o): object(o) {}
    virtual ~SkeletonBase () {}
    virtual Request *createRequest (int methodIndex) = 0;
};

/**
 * Container of whole objects which are related to remote peer.
 * This holds skeletons and remotes.
 */
struct Registry {
    int nextSkeletonId;
    std::map<std::string,InterfaceRef> exportables; // root objects exported to peer.
    std::map<int,Remote*> remotes; // remote handles shared by stub objects
    std::map<int,SkeletonBase*> skeletons; // skeletons locating actual local objects.
    std::map<Interface*,SkeletonBase*> skeletonByExportable; // local obj -> skel

    Transport *transport; // connection to peer.

    Registry (): nextSkeletonId(1) {
        SkeletonBase *skel = createSkeleton();
        skel->id = 0;
        skeletons[0] = skel;
    }

    ~Registry ();

    void setTransport (Transport *trans) {
        transport = trans;
    }

    /**
     * Create skeleton of this registry.
     * serves getRemote(id), notifyRemoteDestroy(id)
     */
    SkeletonBase *createSkeleton ();

    void registerExportable (std::string objname, InterfaceRef exp) {
        exportables[objname] = exp;
    }

    /**
     * Returns remote object addressed by id.
     * id value should be negative. (positive value corresponds to skeleton)
     * if the remote instance is missing, this creates the one.
     */
    Remote *getRemote (int id);

    /**
     * Get a remote object exported by the peer.
     * behaves as if this was a stub.
     * NOTE that Remote instance if base::Ref-counted. 
     */
    Remote *getRemote (std::string objname);

    /**
     * Notifies to the remote peer that given remote object was no longer
     * used by local.
     */
    void notifyRemoteDestroy (int id);

    /**
     * Returns skeleton object identified by id.
     * id value should be positive. (negative value corresponds to remote)
     * if the skeleton is missing, this returns null.
     */
    SkeletonBase *getSkeleton (int id);

    /**
     * Returns skeleton object corresponding to the given raw object.
     * If the skeleton is missing, this create the one.
     */
    SkeletonBase *getSkeleton (Interface *obj);
};

/**
 * Abstraction of rpc io & thread model.
 * Subclass of this should implement missing methods which are
 * platform-dependent & policy-dependent.
 */
struct Transport {
    Registry *registry;
    std::map<int,Port*> ports;
    std::vector<Port*> freePorts;
    pthread_mutex_t monitor;
    Transport (Registry &r): registry(&r) {
        registry->setTransport(const_cast<Transport*>(this));
        pthread_mutex_init(&monitor, 0);
    }
    virtual ~Transport ();

    /** Returns a port and activate it. */
    Port* getPort (int pid);

    /** Returns a default port bound for current thread. */
    Port* getPort ();

    /** Flush out-going messages synchronously. Called by port. */
    virtual void flushPort (Port *p) = 0;

    /** Wait until port is updated. */
    virtual void waitPort (Port *p) = 0;

    /**
     * Notifies that new request was added in the given port but no thread
     * was scheduled to run it.
     * Transport is responsible for executing the requests eventually.
     */
    virtual void notifyUnhandledRequest (Port *p) = 0;
};

/**
 * Virtual duplex channel which represents each queue of incoming/outgoing
 * messages.
 */
struct Port {
    Transport *transport;
    /**
     * Port number. positive value represents that the port was initiated by
     * local thread. negative value means that the port was initiated by 
     * the remote thread.
     */
    int32_t id;
    /** waiting returns for remote call. not owned. */
    std::deque<Return*> returns;
    /** Pending call request. owned. */
    std::deque<Request*> requests;
    /**
     * Last request which was just completed.
     * Also used for call chaining.
     */
    Request *lastRequest;

    PortStack reader;
    PortStack writer;

    // ready to serve incoming reenterant request
    // until all return value is received.
    bool isWaiting;
    pthread_cond_t wakeCondition;

    Port (Transport *trans): transport(trans), isWaiting(false),
            lastRequest(0), reader(this), writer(this) {
        pthread_cond_init(&wakeCondition, 0);
    }

    virtual ~Port ();

    /*
     * read incoming byte streams from the buffer.
     * Returns either STOPPED or COMPLETE or ABORT.
     */
    base::Frame::STATE read (Buffer *buf);

    /**
     * write out-going data to the buffer.
     * Returns either STOPPED or COMPLETE or ABORT.
     */
    base::Frame::STATE write (Buffer *buf);

    /** Flush out-going messages synchronously. */
    void flush ();

    /**
     * Enqueue the return object which will hold response in order.
     * On return value arrival, queued return object will be automatically
     * removed. You can wait on the return.
     */
    void addReturn (Return *ret);

    /**
     * Blocks until all return values are vailable.
     * If there're a thread waiting on sendAndWait(), it will run the request.
     * (i.e. reenterant call)
     */
    void flushAndWait ();

    /**
     * Handle a request added into this port and shift internal request queue.
     * Returns true if a request was handled.
     */
    bool handleRequest ();
};

/**
 * Abstrctation of incoming request which holds arguments and return value.
 * Managed by port.
 */
struct Request {
    int8_t messageHead;
    base::Frame *argumentsReader;
    base::Frame *returnWriter;
    virtual ~Request () {}
    virtual void call () {}
};

/**
 * Unique handle of each skeleton of remote peer.
 * This is base::Ref-counted by stub instances, and notifies to the peer on destroy.
 */
struct Remote: base::RefCounted<Remote> {
    int id; // negative
    Registry *registry;
    ~Remote () {
        registry->notifyRemoteDestroy(id);
    }
};

/**
 * Abstraction of every rpc interfaces this framework support.
 * The real instance could be either stub or raw object. 
 * Skeleton must have null remote.
 * Stub must return null on createSkeleton().
 */
struct Interface: base::RefCounted<Interface> {
    RemoteRef remote;
    virtual SkeletonBase *createSkeleton () { return 0; }
    virtual ~Interface () {}
};

/**
 * Utility template which provides automatic skeleton creation. (optional)
 * Raw class may extend this template instead of rpc interface directly.
 * T must be the rpc interface.
 */
template <typename T>
struct Exportable: T {
    SkeletonBase *createSkeleton () { return new Skeleton<T>(this); }
};

/**
 * Return value holder.
 */
struct Return: base::Frame {
    int index; /** type of return. 0 for normal type, 1+ for exceptions */
    void *value;
    Return (): index(-1) {}
};

struct OwnFrame {
    void *value;
    base::Frame *frame;
};
template <typename T> struct OwnReader: OwnFrame {
    T value;
    Reader<T> reader;
    OwnReader (): reader(value) {
        OwnFrame::value = &value;
        frame = &reader;
    }
};
template <typename T> struct OwnWriter: OwnFrame {
    T value;
    Writer<T> writer;
    OwnWriter (): writer(value) {
        OwnFrame::value = &value;
        frame = &writer;
    }
};
template <> struct OwnReader<void>: OwnFrame {
    Reader<void> reader;
    OwnReader () {
        OwnFrame::value = 0;
        frame = &reader;
    }
};
template <> struct OwnWriter<void>: OwnFrame {
    Writer<void> writer;
    OwnWriter () {
        OwnFrame::value = 0;
        frame = &writer;
    }
};

/**
 * Return value holder which reads the value from stream.
 */
template <
    typename A = base::TupleTerminator, typename B = base::TupleTerminator,
    typename C = base::TupleTerminator, typename D = base::TupleTerminator,
    typename E = base::TupleTerminator, typename F = base::TupleTerminator,
    typename G = base::TupleTerminator, typename H = base::TupleTerminator,
    typename I = base::TupleTerminator, typename J = base::TupleTerminator,
    typename K = base::TupleTerminator, typename L = base::TupleTerminator,
    typename M = base::TupleTerminator, typename N = base::TupleTerminator,
    typename O = base::TupleTerminator, typename P = base::TupleTerminator,
    typename Q = base::TupleTerminator, typename R = base::TupleTerminator,
    typename S = base::TupleTerminator, typename T = base::TupleTerminator,
    typename U = base::TupleTerminator, typename V = base::TupleTerminator,
    typename W = base::TupleTerminator, typename X = base::TupleTerminator,
    typename Y = base::TupleTerminator, typename Z = base::TupleTerminator
>
struct ReturnReader: Return {
    base::TupleChain<OwnReader, OwnFrame, 0,
            A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> tuple;

    template <typename AS>
    AS &get (int i) {
        return *reinterpret_cast<AS*>(tuple.items[i]->value);
    }

    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        TRY_READ(int32_t, index, stack);

        NEXT_STEP();
        stack->push(tuple.items[index]->frame);
        value = tuple.items[index]->value;
        CALL();

        END_STEP();
    }
};

/**
 * Return value holder which sends the value to stream.
 */
template <
    typename A = base::TupleTerminator, typename B = base::TupleTerminator,
    typename C = base::TupleTerminator, typename D = base::TupleTerminator,
    typename E = base::TupleTerminator, typename F = base::TupleTerminator,
    typename G = base::TupleTerminator, typename H = base::TupleTerminator,
    typename I = base::TupleTerminator, typename J = base::TupleTerminator,
    typename K = base::TupleTerminator, typename L = base::TupleTerminator,
    typename M = base::TupleTerminator, typename N = base::TupleTerminator,
    typename O = base::TupleTerminator, typename P = base::TupleTerminator,
    typename Q = base::TupleTerminator, typename R = base::TupleTerminator,
    typename S = base::TupleTerminator, typename T = base::TupleTerminator,
    typename U = base::TupleTerminator, typename V = base::TupleTerminator,
    typename W = base::TupleTerminator, typename X = base::TupleTerminator,
    typename Y = base::TupleTerminator, typename Z = base::TupleTerminator
>
struct ReturnWriter: Return {
    base::TupleChain<OwnWriter, OwnFrame, 0,
            A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> tuple;

    template <typename AS>
    AS &get (int i) {
        return *reinterpret_cast<AS*>(tuple.items[i]->value);
    }

    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        {
            int8_t messageHeader = 0x3<<6;
            TRY_WRITE(int8_t, messageHeader, stack);
        }

        NEXT_STEP();
        TRY_WRITE(int32_t, index, stack);

        NEXT_STEP();
        stack->push(tuple.items[index]->frame);
        CALL();

        END_STEP();
    }
};

template <
    typename A = base::TupleTerminator, typename B = base::TupleTerminator,
    typename C = base::TupleTerminator, typename D = base::TupleTerminator,
    typename E = base::TupleTerminator, typename F = base::TupleTerminator,
    typename G = base::TupleTerminator, typename H = base::TupleTerminator,
    typename I = base::TupleTerminator, typename J = base::TupleTerminator,
    typename K = base::TupleTerminator, typename L = base::TupleTerminator,
    typename M = base::TupleTerminator, typename N = base::TupleTerminator,
    typename O = base::TupleTerminator, typename P = base::TupleTerminator,
    typename Q = base::TupleTerminator, typename R = base::TupleTerminator,
    typename S = base::TupleTerminator, typename T = base::TupleTerminator,
    typename U = base::TupleTerminator, typename V = base::TupleTerminator,
    typename W = base::TupleTerminator, typename X = base::TupleTerminator,
    typename Y = base::TupleTerminator, typename Z = base::TupleTerminator
>
struct ArgumentsReader: base::Frame {
    base::TupleChain<OwnReader, OwnFrame, 0,
            A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> tuple;

    template <typename AS>
    AS &get (int i) {
        return *reinterpret_cast<AS*>(tuple.items[i]->value);
    }

    STATE run (base::Stack *stack) {
        if (step >= tuple.SIZE) {
            return COMPLETE;
        }
        stack->push(tuple.items[step++]->frame);
        return CONTINUE;
    }
};

/**
 * Object reader.
 * T must be rpc interface.
 * Read object could be either stub or raw object.
 */
template <typename T>
struct InterfaceReader: base::Frame {
    base::Ref<T> &object;
    InterfaceReader (base::Ref<T> &o): object(o) {}
    STATE run (base::Stack *stack) {
        int8_t i;
        int id;
        BEGIN_STEP();
        TRY_READ(int8_t, i, stack);
        if (!i) {
            object = 0;
            return COMPLETE;
        }

        NEXT_STEP();
        TRY_READ(int32_t, id, stack);
        id = -id; // reverse local <-> remote on receive

        if (id > 0) {
            object = reinterpret_cast<T*>(
                    static_cast<PortStack*>(stack)->port->transport->registry->
                            getSkeleton(id)->object.get());
        } else {
            object = new Stub<T>();
            object->remote = static_cast<PortStack*>(stack)->port->transport->
                    registry->getRemote(id);
        }

        END_STEP();
    }
};

template <typename T>
struct InterfaceWriter: base::Frame {

    base::Ref<T> &object;
    int id;

    InterfaceWriter (base::Ref<T> &o): object(o) {}

    STATE run (base::Stack *stack) {
        Log l("infwrite ");
        int8_t i;

        BEGIN_STEP();
        if (object) {
            i = 1;
            TRY_WRITE(int8_t, i, stack);
        } else {
            i = 0;
            TRY_WRITE(int8_t, i, stack);
            return COMPLETE;
        }

        NEXT_STEP();
        if (object->remote) {
            id = object->remote->id;
        } else {
            id = static_cast<PortStack*>(stack)->port->transport->registry->
                    getSkeleton(object.get())->id;
        }
        TRY_WRITE(int32_t, id, stack);

        END_STEP();
    }
};

template <const int S>
struct RequestWriter: base::Frame {
    static const int SIZE = S+3;
    base::Frame *frames[SIZE];
    base::Frame **args;
    OwnWriter<int8_t> messageHead;
    OwnWriter<int32_t> objectId;
    OwnWriter<int16_t> methodIndex;

    RequestWriter (int8_t h, int32_t o, int16_t m) {
        messageHead.value = h;
        objectId.value = o;
        methodIndex.value = m;
        this->frames[0] = messageHead.frame;
        this->frames[1] = objectId.frame;
        this->frames[2] = methodIndex.frame;
        args = this->frames+3;
    }

    STATE run (base::Stack *stack) {
        if (step >= SIZE) {
            return COMPLETE;
        }
        stack->push(frames[step++]);
        return CONTINUE;
    }
};

}
#endif
