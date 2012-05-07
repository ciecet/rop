#include <string>
#include <vector>
#include "Remote.h"
#include "Log.h"
#include "TestException.h"

namespace test {

struct EchoCallback: rop::Interface {
    virtual void call (std::string msg) = 0;
};

struct Echo: rop::Interface {
    virtual std::string echo (std::string msg) = 0;
    virtual std::string concat (std::vector<std::string> msgs) = 0;
    // may throws test::TestException
    virtual void touchmenot () = 0;
    virtual void recursiveEcho (std::string msg, base::Ref<test::EchoCallback> cb) = 0;
};

}

namespace rop {

template<>
struct Reader<base::Ref<test::EchoCallback> >: InterfaceReader<test::EchoCallback> {
    Reader (base::Ref<test::EchoCallback> &o): InterfaceReader<test::EchoCallback>(o) {}
};

template<>
struct Writer<base::Ref<test::EchoCallback> >: InterfaceWriter<test::EchoCallback> {
    Writer (base::Ref<test::EchoCallback> &o): InterfaceWriter<test::EchoCallback>(o) {}
};

template<>
struct Stub<test::EchoCallback>: test::EchoCallback {
    void call (std::string msg) {
        Log l("test::EchoCallback ");
        
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        Writer<std::string> arg0(msg);
        RequestWriter<1> req(0, remote->id, 0);
        req.args[0] = &arg0;
        p->writer.push(&req);
        p->flush();
    }
};

template<>
struct Skeleton<test::EchoCallback>: SkeletonBase {

    Skeleton (rop::Interface *o): SkeletonBase(o) {}

    struct __req_call: Request {
        test::EchoCallback *object;
        ArgumentsReader<std::string> args;
        __req_call(test::EchoCallback *o): object(o) {
            argumentsReader = &args;
            returnWriter = 0;
        }
        void call () {
            object->call(args.get<std::string>(0));
        }
    };

    Request *createRequest (int mid) {
        test::EchoCallback *o = reinterpret_cast<test::EchoCallback*>(object.get());
        switch (mid) {
        case 0: return new __req_call(o);
        default: return 0;
        }
    }
};

template<>
struct Reader<base::Ref<test::Echo> >: InterfaceReader<test::Echo> {
    Reader (base::Ref<test::Echo> &o): InterfaceReader<test::Echo>(o) {}
};

template<>
struct Writer<base::Ref<test::Echo> >: InterfaceWriter<test::Echo> {
    Writer (base::Ref<test::Echo> &o): InterfaceWriter<test::Echo>(o) {}
};

template<>
struct Stub<test::Echo>: test::Echo {

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

        ReturnReader<void,test::TestException> ret;
        p->addReturn(&ret);
        p->flushAndWait();
        switch(ret.index) {
        case 0:
            return;
        case 1:
            throw ret.get<test::TestException>(1);
        }
    }

    void recursiveEcho (std::string msg, base::Ref<test::EchoCallback> cb) {
        Transport *trans = remote->registry->transport;
        Port *p = trans->getPort();

        Writer<std::string> arg0(msg);
        InterfaceWriter<test::EchoCallback> arg1(cb);
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
struct Skeleton<test::Echo>: SkeletonBase {

    Skeleton (rop::Interface *o): SkeletonBase(o) {}

    struct __req_echo: Request {
        test::Echo *object;
        ArgumentsReader<std::string> args;
        ReturnWriter<std::string> ret;
        __req_echo(test::Echo *o): object(o) {
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
        test::Echo *object;
        ArgumentsReader<std::vector<std::string> > args;
        ReturnWriter<std::string> ret;
        __req_concat(test::Echo *o): object(o) {
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
        test::Echo *object;
        ReturnWriter<void,test::TestException> ret;
        __req_touchmenot(test::Echo *o): object(o) {
            argumentsReader = 0;
            returnWriter = &ret;
        }

        void call () {
            try {
                object->touchmenot();
                ret.index = 0;
            } catch (test::TestException &e1) {
                ret.get<test::TestException>(1) = e1;
                ret.index = 1;
            }
        }
    };

    struct __req_recursiveEcho: Request {
        test::Echo *object;
        ArgumentsReader<std::string,base::Ref<test::EchoCallback> > args;
        ReturnWriter<void> ret;
        __req_recursiveEcho(test::Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            object->recursiveEcho(
                    args.get<std::string>(0),
                    args.get<base::Ref<test::EchoCallback> >(1));
            ret.index = 0;
        }
    };

    Request *createRequest (int mid) {
        test::Echo *o = reinterpret_cast<test::Echo*>(object.get());
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
