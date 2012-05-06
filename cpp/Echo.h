#include <string>
#include <vector>
#include "Remote.h"
#include "Log.h"
#include "TestException.h"

struct EchoCallback: rop::Interface {
    virtual void call (std::string msg) = 0;
};

struct Echo: rop::Interface {
    virtual std::string echo (std::string msg) = 0;
    virtual std::string concat (std::vector<std::string> msgs) = 0;
    // may throws TestException
    virtual void touchmenot () = 0;
    virtual void recursiveEcho (std::string msg, base::Ref<EchoCallback> cb) = 0;
};

namespace rop {

template<>
struct Reader<base::Ref<EchoCallback> >: InterfaceReader<EchoCallback> {
    Reader (base::Ref<EchoCallback> &o): InterfaceReader<EchoCallback>(o) {}
};

template<>
struct Writer<base::Ref<EchoCallback> >: InterfaceWriter<EchoCallback> {
    Writer (base::Ref<EchoCallback> &o): InterfaceWriter<EchoCallback>(o) {}
};

template<>
struct Stub<EchoCallback>: EchoCallback {
    void call (std::string msg) {
        Log l("echoCallback ");
        
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        Writer<std::string> arg0(msg);
        RequestWriter<1> req(0, remote->id, 0);
        req.args[0] = &arg0;
        p->writer.push(&req);

        ReturnReader<void> ret;
        p->addReturn(&ret);
        p->flushAndWait();
    }
};

template<>
struct Skeleton<EchoCallback>: SkeletonBase {

    Skeleton (rop::Interface *o): SkeletonBase(o) {}

    struct __req_call: Request {
        EchoCallback *object;
        ArgumentsReader<std::string> args;
        ReturnWriter<void> ret;
        __req_call(EchoCallback *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }
        void call () {
            object->call(args.get<std::string>(0));
            ret.index = 0;
        }
    };

    Request *createRequest (int mid) {
        EchoCallback *o = reinterpret_cast<EchoCallback*>(object.get());
        switch (mid) {
        case 0: return new __req_call(o);
        default: return 0;
        }
    }
};

template<>
struct Reader<base::Ref<Echo> >: InterfaceReader<Echo> {
    Reader (base::Ref<Echo> &o): InterfaceReader<Echo>(o) {}
};

template<>
struct Writer<base::Ref<Echo> >: InterfaceWriter<Echo> {
    Writer (base::Ref<Echo> &o): InterfaceWriter<Echo>(o) {}
};

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

        ReturnReader<std::string> ret;
        p->addReturn(&ret);
        p->flushAndWait();
        return ret.get<std::string>(0);
    }

    std::string concat (std::vector<std::string> msgs) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        RequestWriter<1> req(0, remote->id, 1);
        Writer<std::vector<std::string> > arg0(msgs);
        req.args[0] = &arg0;
        p->writer.push(&req);

        ReturnReader<std::string> ret;
        p->addReturn(&ret);
        p->flushAndWait();
        return ret.get<std::string>(0);
    }

    void touchmenot () {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        RequestWriter<0> req(0, remote->id, 2);
        p->writer.push(&req);

        ReturnReader<void,TestException> ret;
        p->addReturn(&ret);
        p->flushAndWait();
        switch(ret.index) {
        case 0:
            return;
        case 1:
            throw ret.get<TestException>(1);
        }
    }

    void recursiveEcho (std::string msg, base::Ref<EchoCallback> cb) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        Writer<std::string> arg0(msg);
        InterfaceWriter<EchoCallback> arg1(cb);
        RequestWriter<2> req(0, remote->id, 3);
        req.args[0] = &arg0;
        req.args[1] = &arg1;
        p->writer.push(&req);

        ReturnReader<void> ret;
        p->addReturn(&ret);
        p->flushAndWait();
    }
};

template<>
struct Skeleton<Echo>: SkeletonBase {

    Skeleton (rop::Interface *o): SkeletonBase(o) {}

    struct __req_echo: Request {
        Echo *object;
        ArgumentsReader<std::string> args;
        ReturnWriter<std::string> ret;
        __req_echo(Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            try {
                ret.get<std::string>(0) =
                        object->echo(args.get<std::string>(0));
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };

    struct __req_concat: Request {
        Echo *object;
        ArgumentsReader<std::vector<std::string> > args;
        ReturnWriter<std::string> ret;
        __req_concat(Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            try {
                ret.get<std::string>(0) =
                        object->concat(args.get<std::vector<std::string> >(0));
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };

    struct __req_touchmenot: Request {
        Echo *object;
        ReturnWriter<void,TestException> ret;
        __req_touchmenot(Echo *o): object(o) {
            argumentsReader = 0;
            returnWriter = &ret;
        }

        void call () {
            try {
                object->touchmenot();
                ret.index = 0;
            } catch (TestException &e1) {
                ret.get<TestException>(1) = e1;
                ret.index = 1;
            }
        }
    };

    struct __req_recursiveEcho: Request {
        Echo *object;
        ArgumentsReader<std::string,base::Ref<EchoCallback> > args;
        ReturnWriter<void> ret;
        __req_recursiveEcho(Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            object->recursiveEcho(
                    args.get<std::string>(0),
                    args.get<base::Ref<EchoCallback> >(1));
            ret.index = 0;
        }
    };

    Request *createRequest (int mid) {
        Echo *o = reinterpret_cast<Echo*>(object.get());
        switch (mid) {
        case 0: return new __req_echo(o);
        case 1: return new __req_concat(o);
        case 2: return new __req_touchmenot(o);
        case 3: return new __req_recursiveEcho(o);
        default: return 0;
        }
    }
};

}
