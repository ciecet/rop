#include <string>
#include "Remote.h"

struct Echo: Interface {
    virtual string echo (string msg) = 0;
    virtual string concat (vector<string> msgs) = 0;
};

template<>
struct Stub<Echo>: Echo {

    string echo (string msg) {
        Transport *trans = remote->reg->transport;
        PortRef p = trans->getPort();

        Writer<string> arg0(msg);
        SequenceWriter<1> sw;
        sw.frames[0] = &arg0;
        p->out.pushFrame(&sw);

        struct _:ReturnReader<1> {
            string value0;
            Reader<string> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p.addReturn(&ret);
        trans->sendAndWait(p);
        return ret.value0;
    }

    string concat (vector<string> msgs) {
        Transport *trans = remote->reg->transport;
        PortRef p = trans->getPort();

        Writer<vector<string> > arg0(msgs);
        SequenceWriter<1> sw;
        sw.frames[0] = &arg0;
        p->out.pushFrame(&sw);

        struct _:ReturnReader<1> {
            string value0;
            Reader<string> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p.addReturn(&ret);
        trans->sendAndWait(p);
        return ret.value0;
    }
};

template<>
struct Skeleton<Echo>: SkeletonBase {

    Ref<Echo> object;

    struct __processor_echo: Processor {
        Ref<Echo> object;

        struct args_t: SequenceReader<1> {
            string arg0;
            Reader<string> frame0;
            args_t(): frame0(arg0) {
                values[0] = &arg0;
                frames[0] = &frame0;
            }
        } args;

        struct ret_t: ReturnWriter<1> {
            string ret0;
            Writer<string> frame0;
            ret_t(): frame(ret0) {
                frames[0] = &ret0;
            }
        } ret;

        __processor_echo(Ref<Echo> objRef): object(objRef) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call (Port *p) {
            try {
                ret.ret0 = object->echo(args.arg0);
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
            p->outStack.pushFrame(&ret);
        }
    };

    struct __processor_concat: Processor {
        Ref<Echo> object;

        struct args_t: SequenceReader<1> {
            vector<string> arg0;
            Reader<vector<string> > frame0;
            args_t(): frame0(arg0) {
                values[0] = &arg0;
                frames[0] = &frame0;
            }
        } args;

        struct ret_t: ReturnWriter<1> {
            string ret0;
            Writer<string> frame0;
            ret_t(): frame(ret0) {
                frames[0] = &ret0;
            }
        } ret;

        __processor_concat(Ref<Echo> objRef): object(objRef) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call (Port *p) {
            try {
                ret.ret0 = object->concat(args.arg0);
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
            p->outStack.pushFrame(&ret);
        }
    };

    ProcessorRef createProcessor (int mid) {
        switch (mid) {
        case 0: return new __processor_echo(object);
        case 1: return new __processor_concat(object);
        default: return 0;
        }
    }
};

struct EchoImpl: Exportable<Interface> {
    string echo (string msg);
    string concat (vector<string> msgs);
};

