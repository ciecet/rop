#ifndef ECHO_H
#define ECHO_H
#include "Remote.h"
#include "TestException.h"
#include "EchoCallback.h"
#include "Person.h"
namespace test {
struct Echo: rop::Interface {
    virtual std::string echo (std::string msg) = 0;
    virtual std::string concat (std::vector<std::string>  msgs) = 0;
    virtual void touchmenot () = 0;
    virtual void recursiveEcho (std::string msg, base::Ref<test::EchoCallback>  cb) = 0;
    virtual void hello (test::Person &p) = 0;
};
}
namespace rop {
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
        Transport *trans = remote->registry->transport;
        Port *__p = trans->getPort();

        Writer<std::string> __arg_msg(msg);
        RequestWriter<1> req(0<<6, remote->id, 0);
        req.args[0] = &__arg_msg;
        __p->writer.push(&req);

        ReturnReader<std::string> ret;
        __p->sendAndWait(&ret);
        switch(ret.index) {
        case 0: return ret.get<std::string>(0);
        default: throw rop::RemoteException();
        }
    }
    std::string concat (std::vector<std::string>  msgs) {
        Transport *trans = remote->registry->transport;
        Port *__p = trans->getPort();

        Writer<std::vector<std::string> > __arg_msgs(msgs);
        RequestWriter<1> req(0<<6, remote->id, 1);
        req.args[0] = &__arg_msgs;
        __p->writer.push(&req);

        ReturnReader<std::string> ret;
        __p->sendAndWait(&ret);
        switch(ret.index) {
        case 0: return ret.get<std::string>(0);
        default: throw rop::RemoteException();
        }
    }
    void touchmenot () {
        Transport *trans = remote->registry->transport;
        Port *__p = trans->getPort();

        RequestWriter<0> req(0<<6, remote->id, 2);
        __p->writer.push(&req);

        ReturnReader<void, test::TestException> ret;
        __p->sendAndWait(&ret);
        switch(ret.index) {
        case 0: return;
        case 1: throw ret.get<test::TestException>(1);
        default: throw rop::RemoteException();
        }
    }
    void recursiveEcho (std::string msg, base::Ref<test::EchoCallback>  cb) {
        Transport *trans = remote->registry->transport;
        Port *__p = trans->getPort();

        Writer<std::string> __arg_msg(msg);
        Writer<base::Ref<test::EchoCallback> > __arg_cb(cb);
        RequestWriter<2> req(0<<6, remote->id, 3);
        req.args[0] = &__arg_msg;
        req.args[1] = &__arg_cb;
        __p->writer.push(&req);

        ReturnReader<void> ret;
        __p->sendAndWait(&ret);
        switch(ret.index) {
        case 0: return;
        default: throw rop::RemoteException();
        }
    }
    void hello (test::Person &p) {
        Transport *trans = remote->registry->transport;
        Port *__p = trans->getPort();

        Writer<test::Person> __arg_p(p);
        RequestWriter<1> req(0<<6, remote->id, 4);
        req.args[0] = &__arg_p;
        __p->writer.push(&req);

        ReturnReader<void> ret;
        __p->sendAndWait(&ret);
        switch(ret.index) {
        case 0: return;
        default: throw rop::RemoteException();
        }
    }
};
template<>
struct Skeleton<test::Echo>: SkeletonBase {
    Skeleton (rop::Interface *o): SkeletonBase(o) {}
    struct __req_echo: Request {
        test::Echo *object;
        ArgumentsReader<std::string> args;
        ReturnWriter<std::string> ret;
        __req_echo (test::Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }
        void call () {
            try {
                ret.get<std::string>(0) = object->echo(args.get<std::string>(0));
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
        __req_concat (test::Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }
        void call () {
            try {
                ret.get<std::string>(0) = object->concat(args.get<std::vector<std::string> >(0));
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };
    struct __req_touchmenot: Request {
        test::Echo *object;
        ReturnWriter<void, test::TestException> ret;
        __req_touchmenot (test::Echo *o): object(o) {
            argumentsReader = 0;
            returnWriter = &ret;
        }
        void call () {
            try {
                object->touchmenot();
                ret.index = 0;
            } catch (test::TestException &e) {
                ret.get<test::TestException>(1) = e;
                 ret.index = 1;
            } catch (...) {
                ret.index = -1;
            }
        }
    };
    struct __req_recursiveEcho: Request {
        test::Echo *object;
        ArgumentsReader<std::string, base::Ref<test::EchoCallback> > args;
        ReturnWriter<void> ret;
        __req_recursiveEcho (test::Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }
        void call () {
            try {
                object->recursiveEcho(args.get<std::string>(0), args.get<base::Ref<test::EchoCallback> >(1));
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };
    struct __req_hello: Request {
        test::Echo *object;
        ArgumentsReader<test::Person> args;
        ReturnWriter<void> ret;
        __req_hello (test::Echo *o): object(o) {
            argumentsReader = &args;
            returnWriter = &ret;
        }
        void call () {
            try {
                object->hello(args.get<test::Person>(0));
                ret.index = 0;
            } catch (...) {
                ret.index = -1;
            }
        }
    };
    Request *createRequest (int mid) {
        test::Echo *o = static_cast<test::Echo*>(object.get());
        switch (mid) {
        case 0: return new __req_echo(o);
        case 1: return new __req_concat(o);
        case 2: return new __req_touchmenot(o);
        case 3: return new __req_recursiveEcho(o);
        case 4: return new __req_hello(o);
        default: return 0;
        }
    }
};
}
#endif
