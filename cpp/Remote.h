#ifndef REMOTE_H
#define REMOTE_H

#include <map>
#include <string>
#include <queue>
#include "Ref.h"
#include "Stack.h"

using namespace std;

struct Transport;
struct Port;
struct Registry;
struct Interface;
struct Remote;
typedef Ref<Port> PortRef;
typedef Ref<Interface> InterfaceRef;
typedef Ref<Remote> RemoteRef;
typedef Ref<SkeletonBase> SkeletonRef;

struct Transport {
    Registry &registry;
    map<int,PortRef> activePorts;
    vector<PortRef> freePorts;

    Transport (Registry &pt): registry(pt) {
        registry.setTransport(this);
    }

    virtual ~Transport () {
        registry.setTransport(0);
    }

    // Returns a default port bound for current thread.
    virtual PortRef getPort () = 0;
    virtual PortRef createPort () { return new Port(); }

    virtual void sendAndWait (PortRef p) = 0;
    virtual void notify (PortRef p) = 0;
    virtual void send (PortRef p) = 0;
    virtual void handleProcessor (PortRef p) {
        if (p->isWaiting) {
            p->notify();
            return;
        }

        // remainings will be executed by a single pumping thread by default.
        // NOTE: any blocker will halt the whole process of rpc.
    }

    // Returns a port and activate it.
    PortRef getPort (int pid) {
        PortRef &p = activePorts[pid];
        if (!p) {
            if (freePorts.empty()) {
                p = createPort();
            } else {
                p = freePorts.back();
                freePorts.pop_back();
            }
            p->id = pid;
        }
        return p;
    }
};

// Holds skeletons and remote handles.
struct Registry {
    map<string,InterfaceRef> exportables; // root objects exported to peer.
    map<int,RemoteRef> remotes; // remote handles shared by stub objects
    int nextSkeletonId;
    map<int,SkeletonRef> skeletons; // skeletons locating actual local objects.
    map<InterfaceRef,SkeletonRef> skeletonByExportable; // local obj -> skel

    Transport *transport; // connection to peer.

    struct RegistrySkeleton: SkeletonBase {
        Registry *registry;

        struct __processor_getRemote: Processor {
            Registry *registry;
            __processor_getRemote(Registry *reg): registry(reg) {}

            struct args_t: SequenceReader<1> {
                string arg0;
                Reader<string> frame0;
                args_t(): frame0(arg0) {
                    values[0] = &arg0;
                    frames[0] = &frame0;
                }
            } args;

            struct ret_t: ReturnWriter<1> {
                int ret0;
                Writer<int> frame0;
                ret_t(): frame(ret0) {
                    frames[0] = &ret0;
                }
            } ret;

            void call (Port *p) {
                map<string,InterfaceRef>:iterator i =
                        registry->exportables.find(args.arg0);
                if (i == registry->exportables.end()) {
                    ret.ret0 = 0;
                    ret.index = 0;
                    return;
                }
                SkeletonRef skel = getSkeleton(i.second());
                ret.ret0 = skel->id;
                ret.index = 0;
            }
        };

        ProcessorRef createProcessor (int methodIndex) {
            switch (mid) {
            case 0: return new __processor_getRemote(registry);
            default: return 0;
            }
        }
    };

    Registry (): nextSkeletonId(1) {
        SkeletonRef rs = new RegistrySkeleton();
        rs->id = 0;
        skeletons[0] = rs;
    }
    void setTransport (Transport *trans) {
        transport = trans;
    }

    void registerExportable (string objname, InterfaceRef exp) {
        exportables[objname] = exp;
    }

    // Get a remote object exported by the peer.
    // acts as if this was a stub of peer's registry
    RemoteRef getRemote (string objname) {
        PortRef p = transport->getPort();

        struct _:ReturnReader<1> {
            int value0;
            Reader<int> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p->addReturn(&ret);

        SequenceWriter<1> wr
        Writer<string> arg0(objname);
        wr.frames[0] = &arg0;
        p->out.pushFrame(&wr);

        transport->sendAndWait(p, &ret); // may throw exception
        return getRemote(ret.value0);
    }

    RemoteRef getRemote (int id) {
        RemoteRef &rr = remotes[id];
        if (!rr) {
            Remote *r = new Remote();
            r->index = ret.value0;
            r->registry = this;
            rr = r;
        }
        return rr;
    }

    SkeletonRef getSkeleton (int id) {
        map<int,SkeletonRef>:iterator i = skeletons.find(id);
        if (!i) {
            return 0;
        }
        return i->second;
    }

    SkeletonRef getSkeleton (InterfaceRef obj) {
        SkeletonRef &skel = skeletonByExportable[obj];
        if (!skel) {
            skel = obj->createSkeleton();
            skel->id = registry->nextSkeletonId++;
            skeletons[skel->id] = skel;
        }
        return skel;
    }
};

template <typename T> struct Reader {};
template <typename T> struct Writer {};

#define BUFFER_SIZE (64*1024)
struct Buffer {
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

struct Processor: RefCounted<Processor> {
    Processor *next;
    Frame *argumentsReader;
    Frame *returnWriter;
    virtual ~Processor () {}
    virtual void call (Port *p) {}
};

typedef Ref<Processor> ProcessorRef;

// Either skeleton or stub.
// Skeleton must have null remote.
// Stub must return null on createSkeleton().
struct Interface: RefCounted<Interface> {
    RemoteRef remote;
    virtual SkeletonRef createSkeleton () { return 0; }
    virtual ~Interface () {}
};

struct SkeletonBase: RefCounted<SkeletonBase> {
    int id;
    virtual ~SkeletonBase () {}
    virtual ProcessorRef createProcessor (int methodIndex);
};

// Super class of idl interface implementation.
// T is any interface defined by idl.
// Not mandatory. Provided as convenience.
template <typename T>
struct Exportable<T>: T {
    SkeletonRef createSkeleton () { return new Skeleton<T>(this); }
};

struct Remote: RefCounted<Remote> {
    int id;
    Registry *registry;
};

// switch (MSB of messageHead)
// 00: sync call (reenterant)
// 01: async call (including call variation using future pattern)
// 10: chained call
// 11: return
struct MessageReader: Frame {

    int8_t messageHead;
    int16_t objectId;
    int16_t methodIndex;
    ProcessorRef processor;
    Return *ret;
    PortRef port;

    MessageReader (PortRef &p): port(p) {}
    RESULT run (Stack *stack) {
        BEGIN_STEP();
        TRY_READ(int8_t, messageHead, stack);

        if (((messageHead >> 6) & 0x3) == 0x3) {
            // handle return
            ret = p->getNextReturn();
            if (!ret) {
                // fail
            }
            stack->pushFrame(ret);
            CALL();
            port.transport->notify(port);
        } else {
            // handle method call
            NEXT_STEP();
            TRY_READ(int16_t, objectId, stack);
            NEXT_STEP();
            TRY_READ(int16_t, methodIndex, stack);

            {
                SkeletonRef skel = port.transport.registry.getSkeleton(objectId);
                if (!skel) {
                    // fail
                }
                processor = skel->createProcessor(messageHead, methodIndex);
            }
            stack->pushFrame(processor.argumentsReader);
            CALL();
            port->appendProcessor(processor);
        }

        // keep reading if buffer is not empty.
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->size) {
            step = 0;
        }
        END_STEP();
    }
};

struct Port: RefCounted<Port> {
    int id;
    Transport &transport;
    queue<Return*> returns;
    queue<ProcessorRef> processors;
    // ready to serve incoming reenterant request
    // until all return value is received.
    bool isWaiting;

    Stack reader;
    Stack writer;

    Port (Transport &trans): transport(trans), isWaiting(false) {
        pthread_cond_init(&condv, 0);
    }
    virtual ~Port () {
        pthread_cond_destroy(&condv, 0);
    }

    // read incoming byte streams
    RESULT read (Buffer *buf) {
        if (!reader.frame) {
            reader.pushFrame(new(reader.allocate(sizeof(MessageReader))) MessageReader(this));
        }

        reader.env = buf;
        return run(&reader);
    }

    // write out-going data to the buffer
    RESULT write (Buffer *buf) {
        writer.env = buf;
        return run(&writer);
    }

    // private
    RESULT run (Stack *stack) {
        while (stack.frame) {
            switch (stack.frame->run(&stack)) {
            case STOPPED:
                return STOPPED;
            case CONTINUE:
                break;
            case COMPLETE:
                stack.popFrame();
                break;
            default:
                // should never occur
            }
        }
        return COMPLETE;
    }

    void addReturn (Return *ret) {
        queue.add(ret);
    }

    Return *getNextReturn () {
        Return *ret = returns.front();
        returns.pop_front();
        return ret;
    }
};

struct Return: Frame {
    int index;
    void *value;
    ReturnFrame (): index(-1) {}
};

template <const int S>
struct ReturnReader: Return {
    void *values[S];
    Frames *frames[S];

    RESULT run (Stack *stack) {
        BEGIN_STEP();
        TRY_READ(int32_t, index, stack);

        NEXT_STEP();
        stack->pushFrame(frames[index]);
        value = values[index];
        CALL();

        END_STEP();
    }
};

template <const int S>
struct ReturnWriter: Return {
    Frames *frames[S];

    RESULT run (Stack *stack) {
        BEGIN_STEP();
        TRY_WRITE(int32_t, index, stack);

        NEXT_STEP();
        stack->pushFrame(frames[index]);
        CALL();

        END_STEP();
    }
};

template <const int S>
struct SequenceWriter: Frame {
    Frame *frames[S];
    SequenceWriter () {}

    RESULT run (Stack *stack) {
        if (step >= S) {
            return COMPLETE;
        }
        stack->pushFrame(frames[step++]);
        return CONTINUE;
    }
};

template <const int S>
struct SequenceReader: Frame {
    void *values[S];
    Frames *frames[S];
    RESULT run (Stack *stack) {
        if (step >= size) {
            // notify that arguments were completely loaded.
            ...
            return COMPLETE;
        }
        stack->pushFrame(frames[step++]);
        return CONTINUE;
    }
};

template <typename T>
struct InterfaceReader: Frame {
    Ref<T> &object;
    InterfaceReader (Ref<T> &o): object(o) {}
    RESULT run (Stack *stack) {
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

        if (id > 0) {
            object = registry->getSkeleton(id);
        } else {
            object = new Stub<T>();
            object->remote = registry->getRemote(id);
        }
    }
};

struct InterfaceWriter: Frame {

    Ref<Interface> &object;
    int id;

    Writer (Ref<Interface> &o): object(o) {}

    RESULT run (Stack *stack) {
        int i;

        BEGIN_STEP();
        if (object) {
            i = 0;
            TRY_WRITE(int8_t, i, stack);
            return COMPLETE;
        } else {
            i = 1;
            TRY_WRITE(int8_t, i, stack);
        }

        NEXT_STEP();
        if (object->remote) {
            id = object->remote->id;
        } else {
            id = registry->getSkeleton(object)->id;
        }
        TRY_WRITE(int32_t, i, stack);

        END_STEP();
    }
};

#endif
