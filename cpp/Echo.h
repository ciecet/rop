#include <string>
#include "Remote.h"

using namespace base;
using namespace rop;

struct Echo: Interface {
    virtual string echo (string msg) = 0;
    virtual string concat (vector<string> msgs) = 0;
};

template<>
struct Stub<Echo>: Echo {

    string echo (string msg) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        Writer<string> arg0(msg);
        SequenceWriter<1> sw;
        sw.frames[0] = &arg0;
        p->writer.push(&sw);

        struct _:ReturnReader<1> {
            string value0;
            Reader<string> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p->addReturn(&ret);
        p->flushAndWait();
        return ret.value0;
    }

    string concat (vector<string> msgs) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        Writer<vector<string> > arg0(msgs);
        SequenceWriter<1> sw;
        sw.frames[0] = &arg0;
        p->writer.push(&sw);

        struct _:ReturnReader<1> {
            string value0;
            Reader<string> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p->addReturn(&ret);
        p->flushAndWait();
        return ret.value0;
    }
};

template<>
struct Skeleton<Echo>: SkeletonBase {

    Skeleton (Interface *o): SkeletonBase(o) {}

    struct __req_echo: Request {
        Echo *object;

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
            ret_t(): frame0(ret0) {
                frames[0] = &frame0;
            }
        } ret;

        __req_echo(Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            try {
                ret.ret0 = object->echo(args.arg0);
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };

    struct __req_concat: Request {
        Echo *object;

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
            ret_t(): frame0(ret0) {
                frames[0] = &frame0;
            }
        } ret;

        __req_concat(Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            try {
                ret.ret0 = object->concat(args.arg0);
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };

    Request *createRequest (int mid) {
        Echo *o = reinterpret_cast<Echo*>(object.get());
        switch (mid) {
        case 0: return new __req_echo(o);
        case 1: return new __req_concat(o);
        default: return 0;
        }
    }
};
