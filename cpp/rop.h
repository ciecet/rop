#ifndef ROP_H
#define ROP_H

#include <stdint.h>
#include <pthread.h>
#include <exception>
#include <new>
#include <map>
#include <set>
#include <vector>
#include <string>
#include "Ref.h"
#include "Tuple.h"
#include "Buffer.h"
#include "Log.h"
#include "Variant.h"
#include "ScopedLock.h"

namespace rop {

struct RemoteException: acba::RefCounter<RemoteException> {
    virtual ~RemoteException () {}
};

/**
 * Default template class for Reader. (empty)
 */
template <typename T>
struct Reader {};

/**
 * Default template class for Writer. (empty)
 */
template <typename T>
struct Writer {};

template <>
struct Reader<int8_t> {
    static inline void run (int8_t &o, acba::Buffer &buf) {
        o = buf.readI8();
    }
};
template <>
struct Writer<int8_t> {
    static inline void run (const int8_t &o, acba::Buffer &buf) {
        buf.writeI8(o);
    }
};
template <>
struct Reader<int16_t> {
    static inline void run (int16_t &o, acba::Buffer &buf) {
        o = buf.readI16();
    }
};
template <>
struct Writer<int16_t> {
    static inline void run (const int16_t &o, acba::Buffer &buf) {
        buf.writeI16(o);
    }
};
template <>
struct Reader<int32_t> {
    static inline void run (int32_t &o, acba::Buffer &buf) {
        o = buf.readI32();
    }
};
template <>
struct Writer<int32_t> {
    static inline void run (const int32_t &o, acba::Buffer &buf) {
        buf.writeI32(o);
    }
};
template <>
struct Reader<int64_t> {
    static inline void run (int64_t &o, acba::Buffer &buf) {
        o = buf.readI64();
    }
};
template <>
struct Writer<int64_t> {
    static inline void run (const int64_t &o, acba::Buffer &buf) {
        buf.writeI64(o);
    }
};
template <>
struct Reader<bool> {
    static inline void run (bool &o, acba::Buffer &buf) {
        if (buf.readI8()) {
            o = true;
        } else {
            o = false;
        }
    }
};
template <>
struct Writer<bool> {
    static inline void run (const bool &o, acba::Buffer &buf) {
        if (o) {
            buf.writeI8(1);
        } else {
            buf.writeI8(0);
        }
    }
};
template <>
struct Reader<float> {
    static inline void run (float &o, acba::Buffer &buf) {
        o = buf.readF32();
    }
};
template <>
struct Writer<float> {
    static inline void run (const float &o, acba::Buffer &buf) {
        buf.writeF32(o);
    }
};
template <>
struct Reader<double> {
    static inline void run (double &o, acba::Buffer &buf) {
        o = buf.readF64();
    }
};
template <>
struct Writer<double> {
    static inline void run (const double &o, acba::Buffer &buf) {
        buf.writeF64(o);
    }
};
template <>
struct Reader<std::string> {
    static inline void run (std::string &o, acba::Buffer &buf) {
        int32_t s = buf.readI32();
        o.assign(reinterpret_cast<char*>(buf.begin()), s);
        buf.drop(s);
    }
};
template <>
struct Writer<std::string> {
    static inline void run (const std::string &o, acba::Buffer &buf) {
        int32_t s = o.length();
        buf.writeI32(s);
        buf.writeBytes(reinterpret_cast<const uint8_t*>(o.data()), s);
    }
};
template <typename T>
struct Reader<std::vector<T> > {
    static inline void run (std::vector<T> &o, acba::Buffer &buf) {
        int s = buf.readI32();
        o.clear();
        o.resize(s);
        for (int i = 0; i < s; i++) {
            Reader<T>::run(o[i], buf);
        }
    }
};
template <typename T>
struct Writer<std::vector<T> > {
    static inline void run (const std::vector<T> &o, acba::Buffer &buf) {
        int s = o.size();
        buf.writeI32(s);
        for (int i = 0; i < s; i++) {
            Writer<T>::run(o[i], buf);
        }
    }
};
template <typename T, typename U>
struct Reader<std::map<T,U> > {
    static inline void run (std::map<T,U> &o, acba::Buffer &buf) {
        int s = buf.readI32();
        o.clear();
        for (int i = 0; i < s; i++) {
            T k;
            Reader<T>::run(k, buf);
            Reader<U>::run(o[k], buf);
        }
    }
};
template <typename T, typename U>
struct Writer<std::map<T,U> > {
    static inline void run (const std::map<T,U> &o, acba::Buffer &buf) {
        int s = o.size();
        buf.writeI32(s);
        for (typename std::map<T,U>::const_iterator iter = o.begin();
                iter != o.end(); iter++) {
            Writer<T>::run(iter->first, buf);
            Writer<U>::run(iter->second, buf);
        }
    }
};
template <typename T>
struct Reader<acba::Ref<T> > {
    static inline void run (acba::Ref<T> &o, acba::Buffer &buf) {
        if (buf.readI8()) {
            o = new T();
            Reader<T>::run(*o, buf);
        } else {
            o = 0;
        }
    }
};
template <typename T>
struct Writer<acba::Ref<T> > {
    static inline void run (const acba::Ref<T> &o, acba::Buffer &buf) {
        if (o) {
            buf.writeI8(1);
            Writer<T>::run(*o, buf);
        } else {
            buf.writeI8(0);
        }
    }
};
template <typename T>
struct Reader<acba::RefCounted<T> > {
    static inline void run (acba::RefCounted<T> &o, acba::Buffer &buf) {
        Reader<T>::run(o, buf);
    }
};
template <typename T>
struct Writer<acba::RefCounted<T> > {
    static inline void run (const acba::RefCounted<T> &o, acba::Buffer &buf) {
        Writer<T>::run(o, buf);
    }
};
template <
    typename A, typename B, typename C, typename D, typename E, typename F,
    typename G, typename H, typename I, typename J, typename K, typename L,
    typename M, typename N, typename O, typename P, typename Q, typename R,
    typename S, typename T, typename U, typename V, typename W, typename X,
    typename Y, typename Z
>
struct Reader<acba::Tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> > {
    typedef acba::Tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z>
            tuple_t;
    static inline void run (tuple_t &o, acba::Buffer &buf) {
        Reader<A>::run(o.head, buf);
        Reader<typename tuple_t::tail_t>::run(o.tail, buf);
    }
};
template <typename A>
struct Reader<acba::Tuple<A,void> > {
    static inline void run (acba::Tuple<A,void> &o, acba::Buffer &buf) {
        Reader<A>::run(o.head, buf);
    }
};
template <
    typename A, typename B, typename C, typename D, typename E, typename F,
    typename G, typename H, typename I, typename J, typename K, typename L,
    typename M, typename N, typename O, typename P, typename Q, typename R,
    typename S, typename T, typename U, typename V, typename W, typename X,
    typename Y, typename Z
>
struct Writer<acba::Tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> > {
    typedef acba::Tuple<A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z>
            tuple_t;
    static inline void run (const tuple_t &o, acba::Buffer &buf) {
        Writer<A>::run(o.head, buf);
        Writer<typename tuple_t::tail_t>::run(o.tail, buf);
    }
};
template <typename A>
struct Writer<acba::Tuple<A,void> > {
    static inline void run (const acba::Tuple<A,void> &o, acba::Buffer &buf) {
        Writer<A>::run(o.head, buf);
    }
};
template <>
struct Reader<acba::Variant> {
    static inline void run (acba::Variant &o, acba::Buffer &buf) {
        int8_t h = buf.readI8();
        if (h >= -32) {
            o = h;
            return;
        }
        switch ((h >> 4) & 0x0f) {
        case 12: case 13: // string
            {
                int n = h & 0x1f;
                if (n == 0x1f) n = buf.readI32();
                std::string str;
                str.assign(reinterpret_cast<char*>(buf.begin()), n);
                buf.drop(n);
                o = str;
                return;
            }
        case 10: case 11: // list
            {
                int n = h & 0x1f;
                if (n == 0x1f) n = buf.readI32();
                o = new acba::Variant::List();
                acba::Variant::List &l = *o.asList();
                l.resize(n);
                for (int i = 0; i < n; i++) {
                    run(l[i], buf);
                }
                return;
            }
        case 9: // map
            {
                int n = h & 0x0f;
                if (n == 0x0f) n = buf.readI32();
                o = new acba::Variant::Map();
                acba::Variant::Map &m = *o.asMap();
                for (int i = 0; i < n; i++) {
                    acba::Variant k;
                    run(k, buf);
                    run(m[k], buf);
                }
                return;
            }
        }

        switch (h & 0x0f) {
        case 0: o.setNull(); break;
        case 2: o = false; break;
        case 3: o = true; break;
        case 4: o = buf.readI8(); break;
        case 5: o = buf.readI16(); break;
        case 6: o = buf.readI32(); break;
        case 7: o = buf.readI64(); break;
        case 8: o = buf.readF32(); break;
        case 9: o = buf.readF64(); break;
        default: o.setNull(); break; // TODO: erronous condition
        }
    }
};
template <>
struct Writer<acba::Variant> {
    static inline void run (const acba::Variant &o, acba::Buffer &buf) {
        switch (o.getType()) {
        case acba::VARIANT_TYPE_NULL:
            buf.writeI8(0x80); break;
        case acba::VARIANT_TYPE_BOOL:
            buf.writeI8(o.asBool() ? 0x83 : 0x82);
            break;
        case acba::VARIANT_TYPE_INT:
            {
                int64_t i = o.asI64();
                if (i == static_cast<int8_t>(i)) {
                    if (i >= -32 && i < 128) {
                        buf.writeI8(i);
                    } else {
                        buf.writeI8(0x84);
                        buf.writeI8(i);
                    }
                } else if (i == static_cast<int16_t>(i)) {
                    buf.writeI8(0x85);
                    buf.writeI16(i);
                } else if (i == static_cast<int32_t>(i)) {
                    buf.writeI8(0x86);
                    buf.writeI32(i);
                } else {
                    buf.writeI8(0x87);
                    buf.writeI64(i);
                }
                break;
            }
        case acba::VARIANT_TYPE_FLOAT:
            {
                double d = o.asF64();
                if (d == static_cast<float>(d)) {
                    buf.writeI8(0x88);
                    buf.writeF32(o.asF32());
                } else {
                    buf.writeI8(0x89);
                    buf.writeF64(o.asF64());
                }
                break;
            }
        case acba::VARIANT_TYPE_STRING:
            {
                int n = o.asString().size();
                if (n < 0x1f) {
                    buf.writeI8(0xc0 | n);
                } else {
                    buf.writeI8(0xc0 | 0x1f);
                    buf.writeI32(n);
                }
                buf.writeBytes(reinterpret_cast<const uint8_t*>(
                        o.asString().data()), n);
                break;
            }
        case acba::VARIANT_TYPE_LIST:
            {
                int n = o.asList()->size();
                if (n < 0x1f) {
                    buf.writeI8(0xa0 | n);
                } else {
                    buf.writeI8(0xa0 | 0x1f);
                    buf.writeI32(n);
                }
                for (int i = 0 ; i < n ; i++) {
                    run(o.asList()->at(i), buf);
                }
                break;
            }
        case acba::VARIANT_TYPE_MAP:
            {
                int n = o.asMap()->size();
                if (n < 0x0f) {
                    buf.writeI8(0x90 | n);
                } else {
                    buf.writeI8(0x90 | 0x0f);
                    buf.writeI32(n);
                }
                acba::Variant::Map::const_iterator iter;
                acba::Variant::Map &m = *o.asMap();
                for (iter = m.begin(); iter != m.end(); iter++) {
                    run(iter->first, buf);
                    run(iter->second, buf);
                }
                break;
            }
        }
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
struct RemoteCall;
struct LocalCall;
struct ReturnBase;
typedef acba::Ref<Interface> InterfaceRef;
typedef acba::Ref<Remote> RemoteRef;
typedef acba::Ref<SkeletonBase> SkeletonRef;

/**
 * Super class of every skeletons.
 * Each instance corresponds to a raw object.
 * This is destroyed when ROP session is closed or
 * notified as unused by notifyRemoteDestroy().
 */
struct SkeletonBase: acba::RefCounter<SkeletonBase> {

    int32_t id; // positive.
    int32_t count;
    InterfaceRef object;

    SkeletonBase (Interface *o): count(0), object(o) {}
    virtual ~SkeletonBase () {}

    virtual void readRequest (LocalCall &lc, acba::Buffer &buf) = 0;
};

/**
 * Unique handle for each remote object.
 * This is acba::Ref-counted by stub instances and notifies to the peer
 * on destroy.
 */
struct Remote: acba::RefCounter<Remote> {
    int32_t id; // negative
    int32_t count;
    acba::Ref<Registry> registry;
    Remote (): count(0) {}
    ~Remote ();
};

/**
 * Listener for registry disposal.
 * Can be used for clearing stub objects whose peer object would be no longer
 * accessible.
 */
struct RegistryListener {
    virtual void registryDisposed (Registry *reg) = 0;
};

/**
 * Container of whole objects which are related to a remote peer.
 * This holds skeletons and remotes and ports.
 */
struct Registry: acba::RefCounter<Registry> {

    typedef std::map<std::string,InterfaceRef> ExportableMap;

    // Global root objects which are exported to the peers.
    static ExportableMap global_exportables;
    static void add (std::string objname, InterfaceRef exp) {
        global_exportables[objname] = exp;
    }
    static void remove (std::string objname) {
        global_exportables.erase(objname);
    }

    int nextSkeletonId;
    ExportableMap exportables;
    std::map<int,Remote*> remotes; // handles shared by stub objects
    std::map<int,SkeletonRef> skeletons; // locates local objects.
    std::map<Interface*,SkeletonBase*> skeletonByExportable; // skel cache
    std::map<int,Port*> ports; // active ports
    std::vector<Port*> freePorts; // free ports for reuse
    std::map<std::string,void*> localData; // arbitrary app data
    std::set<RegistryListener*> listeners;
    Remote peer;

    acba::Ref<Transport> transport;
    pthread_mutex_t monitor;

    // whether requests are processed asynchronously or not in ::recv()
    bool multiThreaded;
    Port *runningPorts, **runningPortsTail; // for single-thread only.

    acba::Log log;

    Registry (Transport *trans, bool mt): nextSkeletonId(1),
            exportables(global_exportables),
            transport(trans),
            multiThreaded(mt),
            runningPorts(0), runningPortsTail(&runningPorts),
            log("reg ") {
        pthread_mutex_init(&monitor, 0);
        SkeletonBase *skel = createSkeleton();
        skel->id = 0;
        skeletons[0] = skel;
        peer.id = 0;
        peer.registry = this;
        log.debug("reg() this:%08x\n", this);
    }

    ~Registry ();

    void wait (pthread_cond_t &cvar) {
        pthread_cond_wait(&cvar, &monitor);
    }

    /**
     * Create skeleton of this registry.
     * serves getRemote(id), notifyRemoteDestroy(id, count)
     */
    SkeletonBase *createSkeleton ();

    /**
     * Returns remote object addressed by id.
     * id value should be negative. (positive value corresponds to skeleton)
     * if the remote instance is missing, this creates the one.
     */
    Remote *getRemote (int32_t id);

    /**
     * Get a remote object exported by the peer.
     * behaves as if this was a stub.
     * NOTE that Remote instance is acba::Ref-counted ahead.
     */
    Remote *getRemote (std::string objname);

    /**
     * Notifies the remote peer that given remote object was no longer
     * used by local.
     */
    void notifyRemoteDestroy (int32_t id, int32_t count);

    /**
     * Returns skeleton object by id.
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
    void tryReleasePort (Port *p);

    int asyncCall (RemoteCall &rc, bool hasRet);
    int syncCall (RemoteCall &rc);

    int recv (Port *p, acba::Buffer &buf);

    void run ();

    /**
     * Notify that bound transport was disconnected and thus this
     * instance is no longer usable.
     */
    void dispose ();

    void *getLocal (std::string var);
    void setLocal (std::string var, void *val);
    void addRegistryListener (RegistryListener *l);
    void removeRegistryListener (RegistryListener *l);

    void setExportables (const ExportableMap &exps) {
        exportables = exps;
    }
};

struct ReadAgent {
    virtual ~ReadAgent () {}
    virtual void read (acba::Buffer &buf) = 0;
};

template <typename T>
struct ReturnReadAgent: ReadAgent {
    T &object;
    ReturnReadAgent (T &r): object(r) {}
    void read (acba::Buffer &buf) {
        Reader<T>::run(object, buf);
    }
};

template <typename T>
struct ExceptionReadAgent: ReadAgent {
    acba::Ref<T> &exceptionRef;
    ExceptionReadAgent (acba::Ref<T> &r): exceptionRef(r) {}
    void read (acba::Buffer &buf) {
        exceptionRef = new T();
        Reader<T>::run(*exceptionRef, buf);
    }
};

struct RemoteCall: acba::Buffer {
    bool gotReturn;
    int returnIndex;
    ReadAgent **readAgents;
    int readAgentNum;

    RemoteCall *next;

    RemoteCall (int h, Remote &r, int m):
            gotReturn(false), returnIndex(-1), readAgentNum(0), next(0) {
        appData = r.registry;
        writeI8(h);
        writeI32(r.id);
        writeI16(m);
    }

    void setReadAgents (ReadAgent **as, int num) {
        readAgents = as;
        readAgentNum = num;
    }

    void readReturn (int h, acba::Buffer &buf) {
        returnIndex = h & 63;
        if (returnIndex == 63) {
            returnIndex = -1;
        }
        if (returnIndex >= 0 && returnIndex < readAgentNum &&
                readAgents[returnIndex]) {
            readAgents[returnIndex]->read(buf);
        }
    }
};

struct ArgumentsBase {
    virtual ~ArgumentsBase () {}
};

template <typename T>
struct Arguments: ArgumentsBase {
    T value;
};

/**
 * Abstrctation of incoming call request which holds arguments and
 * return value. Each one belongs to a port.
 */
struct LocalCall: acba::Buffer {
    Registry *registry;
    int32_t messageType;
    int32_t chainingPosition;
    SkeletonRef skeleton;
    acba::Buffer argsBuffer;
    ArgumentsBase *arguments;

    typedef void (*Function) (void *obj, LocalCall &lc);
    Function function;

    LocalCall *next;

    // return info
    acba::Ref<ReturnBase> returnValue;
    int returnIndex; // -2: initial, -1: error, 0+:return index

    LocalCall (int h, SkeletonBase *skel, Registry &reg):
            arguments(0),
            function(0),
            next(0),
            returnIndex(-2) {
        messageType = (h>>6) & 0x3;
        chainingPosition = h & 0x63;
        skeleton = skel;
        registry = &reg;
        appData = &reg;
    }

    ~LocalCall () {
        if (arguments) {
            arguments->~ArgumentsBase();
        }
    }

    template <typename T>
    T *allocate () {
        typedef Arguments<T> args_t;
        argsBuffer.ensureMargin(sizeof(args_t));
        args_t *args = new (argsBuffer.begin()) args_t();
        arguments = args;
        argsBuffer.grow(sizeof(args_t));
        return &args->value;
    }

    template <typename T>
    T *getArguments () {
        typedef Arguments<T> args_t;
        return &static_cast<args_t*>(arguments)->value;
    }
};

/**
 * Virtual duplex channel which deserializes/serializes requests or returns.
 * This also retains call LocalCall objects for processing,
 * and return objects to be notified.
 */
struct Port {

    static const int32_t PROCESSING_THREAD_TIMEOUT = 3000;
    enum MESSAGE_TYPE {
        MESSAGE_SYNC_CALL = 0,
        MESSAGE_ASYNC_CALL = 1,
        MESSAGE_CHAINED_CALL = 2,
        MESSAGE_RETURN = 3
    };

    /**
     * Port number. positive value represents that the port was initiated by
     * local thread. negative value means that the port was initiated by
     * the remote thread.
     */
    int32_t id;

    int32_t otherId;

    /**
     * The registry where this port belongs to.
     */
    Registry *registry;

    RemoteCall *remoteCalls, **remoteCallsTail;
    LocalCall *localCalls, **localCallsTail;
    acba::Ref<ReturnBase> deferredReturn;


    // indicate that a thread is processing requests in this port.
    bool hasProcessingThread;
    pthread_cond_t updated; // used only in multi-threaded mode
    Port *next; // used only in single-thread mode

    acba::Log log;

    Port (Registry *reg): id(0), otherId(0), registry(reg),
            remoteCalls(0), remoteCallsTail(&remoteCalls),
            localCalls(0), localCallsTail(&localCalls),
            hasProcessingThread(false), next(0),
            log("port ") {
        pthread_condattr_t attr;
        pthread_condattr_init(&attr);
        pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        pthread_cond_init(&updated, &attr);
    }

    virtual ~Port ();

    void addRemoteCall (RemoteCall &rc) {
        *remoteCallsTail = &rc;
        remoteCallsTail = &rc.next;
    }

    void addRemoteCallFront (RemoteCall &rc) {
        if (remoteCallsTail != &remoteCalls) {
            rc.next = remoteCalls;
            remoteCalls = &rc;
        } else {
            *remoteCallsTail = &rc;
            remoteCallsTail = &rc.next;
        }
    }

    RemoteCall *removeRemoteCall () {
        RemoteCall *rc = remoteCalls;
        if (!rc) return 0;
        remoteCalls = remoteCalls->next;
        rc->next = 0;
        if (!remoteCalls) {
            remoteCallsTail = &remoteCalls;
        }
        return rc;
    }

    void addLocalCall (LocalCall &rc);

    LocalCall *removeLocalCall () {
        LocalCall *rc = localCalls;
        if (!rc) return 0;
        localCalls = localCalls->next;
        rc->next = 0;
        if (!localCalls) {
            localCallsTail = &localCalls;
        }
        return rc;
    }

    /**
     * Handle a LocalCall added into this port and shift internal LocalCall queue.
     * Returns true if a LocalCall was handled.
     */
    bool processRequest ();
};

/**
 * Abstraction of rop connection io & thread model.
 * Subclass of this should implement missing methods which are
 * platform-dependent & policy-dependent.
 */
struct Transport: acba::RefCounter<Transport> {

    acba::Ref<Registry> registry;

    Transport (bool mt): registry(new Registry(this, mt)) {}
    virtual ~Transport () {}

    // Send a mesage to the peer. Returns -1 on error
    virtual int send (Port *p, acba::Buffer &buf) = 0;

    // Wait until the port gets some messages
    // assumes registry lock being held.
    virtual int wait (Port *p) {
        if (!registry) return -1;
        registry->wait(p->updated);
        return 0;
    }

    // Request io thread to call Registry::run() on idle time.
    // Once called, subsequent calls can be ignored until run().
    // Provided as hooking method for single-thread model support.
    virtual void requestRun () {
    }

    // Let this deleted on registry delete.
    // assumes registry lock being held.
    virtual void close () {
        registry->dispose();
        registry = 0;
    }
};

/**
 * Abstraction of every rpc interfaces this framework support.
 * The instance will be either stub or real object.
 * Skeleton must have null remote.
 * Stub must return null on createSkeleton().
 */
struct Interface: acba::RefCounter<Interface> {
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
 * Base class for template Return
 */
struct ReturnBase: acba::RefCounter<ReturnBase> {
    virtual ~ReturnBase () {}
    virtual void write (acba::Buffer &buf) = 0;
};

/**
 * Return value holder.
 */
template <typename T>
struct Return: ReturnBase {
    T value;
    void write (acba::Buffer &buf) {
        Writer<T>::run(value, buf);
    }
};

/**
 * Object reader.
 * T must be rpc interface.
 * Read object could be either stub or real object.
 */
template <typename T>
void readInterface (acba::Ref<T> &o, acba::Buffer &buf) {
    Registry *reg = static_cast<Registry*>(buf.appData);
    int32_t id = -buf.readI32();

    if (id < 0) {
        o = new Stub<T>();
        o->remote = reg->getRemote(id);
        o->remote->count++;
    } else {
        o = static_cast<T*>(reg->getSkeleton(id)->object.get());
    }
}

template <typename T>
void writeInterface (const acba::Ref<T> &o, acba::Buffer &buf) {
    Registry *reg = static_cast<Registry*>(buf.appData);

    if (o->remote) {
        // TOOD: check o->remote->registry.get() == reg
        buf.writeI32(o->remote->id);
    } else {
        SkeletonBase *skel = reg->getSkeleton(o.get());
        skel->count++;
        buf.writeI32(skel->id);
    }
}

int currentPortId ();

/**
 * Returns a registry object which holds a skeleton object currently
 * processing a request in the calling thread.
 * This function returns null if the calling thread is not processing
 * rop requests.
 */
Registry *currentRegistry ();

// internal structure
struct LocalContext {
    int portId;
    Registry *registry;
};

/**
 * Create a context which enables to use predefined port id on remote call
 * regardless of calling thread.
 * Context::Lock is a scoped lock which overrides port id of current thread
 * until destroyed.
 */
class Context: public acba::RefCounter<Context> {
    pthread_mutex_t mutex;

    LocalContext override;
    LocalContext backup;

    ~Context (); // prevent stack allocation
    friend class acba::RefCounter<Context>;

public:
    Context ();
    int getPortId () {
        return override.portId;
    }

    class Lock {
        acba::Ref<Context> context;
    public:
        Lock (Context*);
        ~Lock ();
    };
};

}
#endif
