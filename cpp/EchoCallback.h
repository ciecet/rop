#ifndef ECHOCALLBACK_H
#define ECHOCALLBACK_H
#include "Remote.h"
namespace test {
struct EchoCallback: rop::Interface {
    virtual void call (std::string msg) = 0;
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
        Transport *trans = remote->registry->transport;
        Port *__p = trans->getPort();

        Writer<std::string> __arg_msg(msg);
        RequestWriter<1> req(1<<6, remote->id, 0);
        req.args[0] = &__arg_msg;
        __p->writer.push(&req);
        __p->send(0);
    }
};
template<>
struct Skeleton<test::EchoCallback>: SkeletonBase {
    Skeleton (rop::Interface *o): SkeletonBase(o) {}
    struct __req_call: Request {
        test::EchoCallback *object;
        ArgumentsReader<std::string> args;
        __req_call (test::EchoCallback *o): object(o) {
            argumentsReader = &args;
            returnWriter = 0;
        }
        void call () {
            object->call(args.get<std::string>(0));
        }
    };
    Request *createRequest (int mid) {
        test::EchoCallback *o = static_cast<test::EchoCallback*>(object.get());
        switch (mid) {
        case 0: return new __req_call(o);
        default: return 0;
        }
    }
};
}
#endif
