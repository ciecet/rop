#ifndef TESTEXCEPTION_H
#define TESTEXCEPTION_H
#include "Remote.h"

namespace test {
struct TestException: rop::RemoteException {
    base::ContainerRef<int32_t>  i;
    TestException () {}
    TestException (const base::ContainerRef<int32_t>  &arg0): i(arg0) {}
    ~TestException () throw() {}
};
}

namespace rop {
template<>
struct Reader <test::TestException>: base::Frame {
    test::TestException &object;
    Reader (test::TestException &o): object(o) {}
    STATE run (base::Stack *stack) {
        switch (step) {
        case 0: stack->push(new(stack->allocate(sizeof(Reader<base::ContainerRef<int32_t> >))) Reader<base::ContainerRef<int32_t> >(object.i)); step++; return CONTINUE;
        default: return COMPLETE;
        }
    }
};
template<>
struct Writer <test::TestException>: base::Frame {
    test::TestException &object;
    Writer (test::TestException &o): object(o) {}
    STATE run (base::Stack *stack) {
        switch (step) {
        case 0: stack->push(new(stack->allocate(sizeof(Writer<base::ContainerRef<int32_t> >))) Writer<base::ContainerRef<int32_t> >(object.i)); step++; return CONTINUE;
        default: return COMPLETE;
        }
    }
};
}
#endif
