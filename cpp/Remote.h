#ifndef REMOTE_H
#define REMOTE_H

#include <stdint.h>
#include <pthread.h>
#include <exception>
#include <new>
#include <map>
#include <vector>
#include <string>
#include <deque>
#include "Ref.h"
#include "Stack.h"
#include "Tuple.h"
#include "Buffer.h"
#include "Log.h"

namespace rop {

class RemoteException: public std::exception {
};

/**
 * Stack extension for reader/writer frames
 */
struct Port;
struct PortStack: base::Stack {
    Port *port;
    base::Buffer *buffer;
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

// Utility macros for easy management of step variable.
// TRY_READ/TRY_WRITE should be used only for primitive types
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

template<typename T>
struct IntReader: base::Frame {
    T &obj;
    IntReader (T &o): obj(o) {}
    STATE run (base::Stack *stack) {
        base::Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->size() < sizeof(T)) return STOPPED;
        T t = buf->peek(0);
        for (int i = 1; i < sizeof(T); i++) {
            t = (t << 8) + buf->peek(i);
        }
        buf->drop(sizeof(T));
        obj = t;
        return COMPLETE;
    }
};

template<typename T>
struct IntWriter: base::Frame {
    T &obj;
    IntWriter (T &o): obj(o) {}
    STATE run (base::Stack *stack) {
        base::Buffer *const buf = static_cast<PortStack*>(stack)->buffer;
        if (buf->margin() < sizeof(T)) return STOPPED;
        T t = obj;
        for (int i = (sizeof(T) - 1) * 8; i > 0; i -= 8) {
            buf->write(t >> i);
        }
        buf->write(t);
        return COMPLETE;
    }
};

template<> struct Reader<int8_t>: IntReader<int8_t> {
    Reader (int8_t &o): IntReader<int8_t>(o) {}
};
template<> struct Writer<int8_t>: IntWriter<int8_t> {
    Writer (int8_t &o): IntWriter<int8_t>(o) {}
};
template<> struct Reader<int16_t>: IntReader<int16_t> {
    Reader (int16_t &o): IntReader<int16_t>(o) {}
};
template<> struct Writer<int16_t>: IntWriter<int16_t> {
    Writer (int16_t &o): IntWriter<int16_t>(o) {}
};
template<> struct Reader<int32_t>: IntReader<int32_t> {
    Reader (int32_t &o): IntReader<int32_t>(o) {}
};
template<> struct Writer<int32_t>: IntWriter<int32_t> {
    Writer (int32_t &o): IntWriter<int32_t>(o) {}
};
template<> struct Reader<int64_t>: IntReader<int64_t> {
    Reader (int64_t &o): IntReader<int64_t>(o) {}
};
template<> struct Writer<int64_t>: IntWriter<int64_t> {
    Writer (int64_t &o): IntWriter<int64_t>(o) {}
};

template<> struct Reader<std::string>: base::Frame {

    std::string &obj;
    int remain;

    Reader (std::string &o): obj(o) {}

    STATE run (base::Stack *stack) {
        base::Buffer *buf = static_cast<PortStack*>(stack)->buffer;

        // read size
        BEGIN_STEP();
        TRY_READ(int32_t, remain, stack);
        obj.clear();
        obj.reserve(remain);

        // read chars
        NEXT_STEP();
        {
            int n = (remain > buf->size()) ? buf->size() : remain;
            for (int i = 0; i < n; i++) {
                obj.push_back(static_cast<char>(buf->peek(i)));
            }
            buf->drop(n);
            if ((remain -= n) > 0) {
                return STOPPED;
            }
        }

        END_STEP();
    }
};

template<> struct Writer<std::string>: base::Frame {

    std::string &obj;
    int i;

    Writer (std::string &o): obj(o) {}

    STATE run (base::Stack *stack) {
        base::Buffer *buf = static_cast<PortStack*>(stack)->buffer;

        // write size 
        BEGIN_STEP();
        i = obj.length();
        TRY_WRITE(int32_t, i, stack);
        i = 0;

        // write chars
        NEXT_STEP();
        {
            int n = (obj.length() - i > buf->margin()) ?
                    i + buf->margin() : obj.length();
            while (i < n) buf->write(obj.at(i++));
            if (i < obj.length()) {
                return STOPPED;
            }
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
        obj.clear();
        n *= 2;

        NEXT_STEP();
        if (n) {
            if ((n & 1) == 0) {
                // read key
                stack->push(new(stack->allocate(sizeof(Reader<T>))) Reader<T>(key));
                n--;
                return CONTINUE;
            } else {
                // read value
                stack->push(new(stack->allocate(sizeof(Reader<U>))) Reader<U>(obj[key]));
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
        {
            int size = obj.size();
            TRY_WRITE(int32_t, size, stack);
        }
        iter = obj.begin();
        first = true;

        // write key and value
        NEXT_STEP();
        if (iter != obj.end()) {
            if (first) {
                stack->push(new(stack->allocate(sizeof(Writer<T>))) Writer<T>(
                        const_cast<T&>(iter->first)));
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
        int8_t i;
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
        int8_t i;
        BEGIN_STEP();
        if (obj) {
            i = 1;
            TRY_WRITE(int8_t, i, stack);
            stack->push(new (stack->allocate(sizeof(Writer<T>))) Writer<T>(*obj));
            CALL();
        } else {
            i = 0;
            TRY_WRITE(int8_t, i, stack);
        }

        END_STEP();
    }
};

template <typename T>
struct Reader<base::ContainerRef<T> >: base::Frame {
    base::ContainerRef<T> &obj;
    Reader (base::ContainerRef<T> &o): obj(o) {}
    STATE run (base::Stack *stack) {
        int8_t i;
        BEGIN_STEP();
        TRY_READ(int8_t, i, stack);
        if (i) {
            obj = new base::Container<T>();
            stack->push(new (stack->allocate(sizeof(Reader<T>))) Reader<T>(*obj));
            CALL();
        } else {
            obj = 0;
        }
        END_STEP();
    }
};

template <typename T>
struct Writer<base::ContainerRef<T> >: base::Frame {
    base::ContainerRef<T> &obj;
    Writer (base::ContainerRef<T> &o): obj(o) {}

    STATE run (base::Stack *stack) {
        int8_t i;
        BEGIN_STEP();
        if (obj) {
            i = 1;
            TRY_WRITE(int8_t, i, stack);
            stack->push(new (stack->allocate(sizeof(Writer<T>))) Writer<T>(*obj));
            CALL();
        } else {
            i = 0;
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
    int32_t id; // positive.
    int32_t count;
    InterfaceRef object;
    SkeletonBase (Interface *o): count(0), object(o) {}
    virtual ~SkeletonBase () {}
    virtual Request *createRequest (int methodIndex) = 0;
};

/**
 * Container of whole objects which are related to remote peer.
 * This holds skeletons and remotes and ports.
 */
struct Registry {
    int nextSkeletonId;
    // root objects exported to peer.
    std::map<std::string,InterfaceRef> exportables;
    std::map<int,Remote*> remotes; // handles shared by stub objects
    std::map<int,SkeletonBase*> skeletons; // locates local objects.
    std::map<Interface*,SkeletonBase*> skeletonByExportable; // skel cache
    std::map<int,Port*> ports; // active ports
    std::vector<Port*> freePorts; // free ports for reuse
    pthread_mutex_t monitor;

    Transport *transport; // connection to peer.

    Registry (): nextSkeletonId(1) {
        pthread_mutex_init(&monitor, 0);
        SkeletonBase *skel = createSkeleton();
        skel->id = 0;
        skeletons[0] = skel;
    }

    ~Registry ();

    void lock () {
        pthread_mutex_lock(&monitor);
    }

    void unlock () {
        pthread_mutex_unlock(&monitor);
    }

    void setTransport (Transport *trans);

    /**
     * Create skeleton of this registry.
     * serves getRemote(id), notifyRemoteDestroy(id, count)
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
    Remote *getRemote (int32_t id);

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
    void notifyRemoteDestroy (int32_t id, int32_t count);

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

    /** Returns a port with monitor lock held. */
    Port *getPort (int pid);

    /**
     * Same as getPort(pid) except that this returns a port bound to
     * current thread.
     */
    Port *getPort ();

    /** Release monitor and finish using the port */
    void releasePort (Port *p);

//protected:
    /**
     * Derived class may need to use getPort() while holding the lock already.
     * This utility is provided for the case recursive lock is not available.
     */
    Port *unsafeGetPort (int pid);

    /**
     * Derived class may need to use releasePort() while holding the lock yet.
     * Provided as utility method.
     */
    void unsafeReleasePort (Port *p);
};

/**
 * Virtual duplex channel which deserialize on reading and serialize on sending.
 * This also retains call request objects for processing,
 * and return objects to be notified.
 */
struct Port {
    /**
     * Port number. positive value represents that the port was initiated by
     * local thread. negative value means that the port was initiated by 
     * the remote thread.
     */
    int32_t id;

    /**
     * The registry where this port belongs to.
     */
    Registry *registry;

    /** waiting returns for remote call. not owned. */
    std::deque<Return*> returns;

    /** Pending call request. owned. */
    std::deque<Request*> requests;

    /**
     * Last request which was just completed.
     * Also used for call chaining.
     */
    Request *lastRequest;

    PortStack writer; // serialize messages to transport stream
    PortStack reader; // deserialize messages from transport stream

    // ready to serve incoming reenterant request
    // until all return value is received.
    bool waitingSyncReturn;
    pthread_cond_t updated;

    // waiting line for sending.
    Port *next;

    // attached thread proccessing the requests.  
    bool isProcessingRequests;
    pthread_t processingThread;


    Port (Registry *reg): registry(reg), lastRequest(0),
            reader(this), writer(this), waitingSyncReturn(false),
            next(0),
            isProcessingRequests(false) {
        pthread_cond_init(&updated, 0);
    }

    virtual ~Port ();

    /**
     * Encode out-going messages to the buffer.
     * Called from the transport with the monitor lock held.
     * Returns either STOPPED or COMPLETE or ABORT.
     */
    base::Frame::STATE encode (base::Buffer *buf);

    /*
     * Decode incoming messages from the buffer.
     * Called from the transport with the monitor lock held.
     * Returns either STOPPED or COMPLETE or ABORT.
     */
    base::Frame::STATE decode (base::Buffer *buf);

    /**
     * Blocks until the transport finishes to send out-going messages,
     * and then release monitor lock.
     * @arg ret is used for registering future object. (not implemented yet)
     */
    void send (Return *ret);

    /**
     * Send messages and then wait for the return value.
     * This releases monitor lock on return.
     * NOTE that this may serve recursive rpc request until the return arrive.
     */
    void sendAndWait (Return *ret);

    /**
     * Handle a request added into this port and shift internal request queue.
     * Returns true if a request was handled.
     */
    bool processRequest ();

    /**
     * Returns false if the port is no longer used at this moment.
     * Should be called with monitor lock held. 
     */
    bool isActive ();
};

/**
 * Abstraction of rpc io & thread model.
 * Subclass of this should implement missing methods which are
 * platform-dependent & policy-dependent.
 */
struct Transport {
    Registry *registry;
    Transport (): registry(0) {}
    virtual ~Transport () {}

    /** Wait until messages are fully sent from the port. */
    virtual void send (Port *p) = 0;

    /** Wait until the port gets messages. May return on partial message */
    virtual void receive (Port *p) = 0;

    /**
     * Notifies that new request was added in the given port but no thread
     * was scheduled to run it. Transport is responsible for executing the
     * requests as soon as possible.
     */
    virtual void notifyUnhandledRequest (Port *p) = 0;
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
    int32_t id; // negative
    int32_t count;
    Registry *registry;
    Remote (): count(0) {}
    ~Remote () {
        if (registry) {
            registry->notifyRemoteDestroy(id, count);
        }
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
    int index;
    void *value;
    bool isValid;
    Return (): index(-1), isValid(false) {}
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
    typename A = base::TupleEnd, typename B = base::TupleEnd,
    typename C = base::TupleEnd, typename D = base::TupleEnd,
    typename E = base::TupleEnd, typename F = base::TupleEnd,
    typename G = base::TupleEnd, typename H = base::TupleEnd,
    typename I = base::TupleEnd, typename J = base::TupleEnd,
    typename K = base::TupleEnd, typename L = base::TupleEnd,
    typename M = base::TupleEnd, typename N = base::TupleEnd,
    typename O = base::TupleEnd, typename P = base::TupleEnd,
    typename Q = base::TupleEnd, typename R = base::TupleEnd,
    typename S = base::TupleEnd, typename T = base::TupleEnd,
    typename U = base::TupleEnd, typename V = base::TupleEnd,
    typename W = base::TupleEnd, typename X = base::TupleEnd,
    typename Y = base::TupleEnd, typename Z = base::TupleEnd
>
struct ReturnReader: Return {
    base::TupleChain<OwnReader, OwnFrame, 0,
            A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> tuple;

    template <typename AS>
    AS &get (int i) {
        return *reinterpret_cast<AS*>(tuple.items[i]->value);
    }

    STATE run (base::Stack *stack) {
        if (index == -1) {
            return COMPLETE;
        }

        BEGIN_STEP();
        stack->push(tuple.items[index]->frame);
        CALL();
        value = tuple.items[index]->value;

        END_STEP();
    }
};

/**
 * Return value holder which sends the value to stream.
 */
template <
    typename A = base::TupleEnd, typename B = base::TupleEnd,
    typename C = base::TupleEnd, typename D = base::TupleEnd,
    typename E = base::TupleEnd, typename F = base::TupleEnd,
    typename G = base::TupleEnd, typename H = base::TupleEnd,
    typename I = base::TupleEnd, typename J = base::TupleEnd,
    typename K = base::TupleEnd, typename L = base::TupleEnd,
    typename M = base::TupleEnd, typename N = base::TupleEnd,
    typename O = base::TupleEnd, typename P = base::TupleEnd,
    typename Q = base::TupleEnd, typename R = base::TupleEnd,
    typename S = base::TupleEnd, typename T = base::TupleEnd,
    typename U = base::TupleEnd, typename V = base::TupleEnd,
    typename W = base::TupleEnd, typename X = base::TupleEnd,
    typename Y = base::TupleEnd, typename Z = base::TupleEnd
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
            int8_t messageHeader = 0x3<<6 | (index&63);
            TRY_WRITE(int8_t, messageHeader, stack);
        }
        if (index == -1) {
            return COMPLETE;
        }

        NEXT_STEP();
        stack->push(tuple.items[index]->frame);
        CALL();

        END_STEP();
    }
};

template <
    typename A = base::TupleEnd, typename B = base::TupleEnd,
    typename C = base::TupleEnd, typename D = base::TupleEnd,
    typename E = base::TupleEnd, typename F = base::TupleEnd,
    typename G = base::TupleEnd, typename H = base::TupleEnd,
    typename I = base::TupleEnd, typename J = base::TupleEnd,
    typename K = base::TupleEnd, typename L = base::TupleEnd,
    typename M = base::TupleEnd, typename N = base::TupleEnd,
    typename O = base::TupleEnd, typename P = base::TupleEnd,
    typename Q = base::TupleEnd, typename R = base::TupleEnd,
    typename S = base::TupleEnd, typename T = base::TupleEnd,
    typename U = base::TupleEnd, typename V = base::TupleEnd,
    typename W = base::TupleEnd, typename X = base::TupleEnd,
    typename Y = base::TupleEnd, typename Z = base::TupleEnd
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
    int32_t id;
    int32_t count;
    InterfaceReader (base::Ref<T> &o): object(o) {}
    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        TRY_READ(int32_t, id, stack);
        id = -id; // reverse local <-> remote on receive

        if (id > 0) {
            object = reinterpret_cast<T*>(
                    static_cast<PortStack*>(stack)->port->registry->
                            getSkeleton(id)->object.get());
        } else {
            Remote *r = static_cast<PortStack*>(stack)->port->registry->
                    getRemote(id);
            r->count++;
            object = new Stub<T>();
            object->remote = r;
        }

        END_STEP();
    }
};

template <typename T>
struct InterfaceWriter: base::Frame {

    base::Ref<T> &object;
    SkeletonBase *skeleton;

    InterfaceWriter (base::Ref<T> &o): object(o) {}

    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        if (object->remote) {
            TRY_WRITE(int32_t, object->remote->id, stack);
        } else {
            skeleton = static_cast<PortStack*>(stack)->port->registry->
                    getSkeleton(object.get());
            skeleton->count++;

            NEXT_STEP();
            TRY_WRITE(int32_t, skeleton->id, stack);
        }
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
