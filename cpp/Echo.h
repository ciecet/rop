#include <string>
#include <vector>
#include "Remote.h"
#include "Log.h"

struct Echo: rop::Interface {
    virtual std::string echo (std::string msg) = 0;
    virtual std::string concat (std::vector<std::string> msgs) = 0;
    virtual void touchmenot () throw(int) = 0;
};

namespace rop {

template<>
struct Stub<Echo>: Echo {

    std::string echo (std::string msg) {
        Log l("stub ");
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        l.debug("sending %d.%d...\n", remote->id, 0);
        RequestWriter<1> req(0, remote->id, 0);
        Writer<std::string> arg0(msg);
        req.args[0] = &arg0;
        p->writer.push(&req);

        struct _:ReturnReader<1> {
            std::string value0;
            Reader<std::string> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p->addReturn(&ret);
        p->flushAndWait();
        return ret.value0;
    }

    std::string concat (std::vector<std::string> msgs) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        RequestWriter<1> req(0, remote->id, 1);
        Writer<std::vector<std::string> > arg0(msgs);
        req.args[0] = &arg0;
        p->writer.push(&req);

        struct _:ReturnReader<1> {
            std::string value0;
            Reader<std::string> frame0;
            _(): frame0(value0) {
                values[0] = &value0;
                frames[0] = &frame0;
            }
        } ret;
        p->addReturn(&ret);
        p->flushAndWait();
        return ret.value0;
    }

    void touchmenot () throw(int) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        RequestWriter<0> req(0, remote->id, 2);
        p->writer.push(&req);

        struct _:ReturnReader<2> {
            Reader<void> frame0;
            int value1;
            Reader<int> frame1;
            _(): frame0(), frame1(value1) {
                values[0] = 0;
                frames[0] = &frame0;
                values[1] = &value1;
                frames[1] = &frame1;
            }
        } ret;
        p->addReturn(&ret);
        p->flushAndWait();
        switch(ret.index) {
        case 0:
            return;
        case 1:
            throw ret.value1;
        }
    }
};

template<>
struct Skeleton<Echo>: SkeletonBase {

    Skeleton (rop::Interface *o): SkeletonBase(o) {}

    struct __req_echo: Request {
        Echo *object;

        struct args_t: SequenceReader<1> {
            std::string arg0;
            Reader<std::string> frame0;
            args_t(): frame0(arg0) {
                values[0] = &arg0;
                frames[0] = &frame0;
            }
        } args;

        struct ret_t: ReturnWriter<1> {
            std::string ret0;
            Writer<std::string> frame0;
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
            std::vector<std::string> arg0;
            Reader<std::vector<std::string> > frame0;
            args_t(): frame0(arg0) {
                values[0] = &arg0;
                frames[0] = &frame0;
            }
        } args;

        struct ret_t: ReturnWriter<1> {
            std::string ret0;
            Writer<std::string> frame0;
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

    struct __req_touchmenot: Request {
        Echo *object;

        struct ret_t: ReturnWriter<2> {
            Writer<void> frame0;
            int value1;
            Writer<int> frame1;
            ret_t(): frame0(), frame1(value1) {
                frames[0] = &frame0;
                frames[1] = &frame1;
            }
        } ret;

        __req_touchmenot(Echo *o): object(o) {
            argumentsReader = 0;
            returnWriter = &ret;
        }

        void call () {
            try {
                object->touchmenot();
                ret.index = 0;
            } catch (int &ret1) {
                ret.value1 = ret1;
                ret.index = 1;
            }
        }
    };

    Request *createRequest (int mid) {
        Echo *o = reinterpret_cast<Echo*>(object.get());
        switch (mid) {
        case 0: return new __req_echo(o);
        case 1: return new __req_concat(o);
        case 2: return new __req_touchmenot(o);
        default: return 0;
        }
    }
};

}
