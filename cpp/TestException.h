#ifndef TESTEXCEPTION_H
#define TESTEXCEPTION_H
#include "Remote.h"

namespace test {
struct TestException: rop::RemoteException {
    int32_t i;
    TestException () {}
    TestException (const int32_t &arg0): i(arg0) {}
};
}

namespace rop {
template<>
struct Reader <test::TestException>: base::Frame {
    test::TestException &object;
    Reader (test::TestException &o): object(o) {}
    STATE run (base::Stack *stack) {
        switch (step) {
        case 0: stack->push(new(stack->allocate(sizeof(Reader<int32_t>))) Reader<int32_t>(object.i)); step++; return CONTINUE;
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
        case 0: stack->push(new(stack->allocate(sizeof(Writer<int32_t>))) Writer<int32_t>(object.i)); step++; return CONTINUE;
        default: return COMPLETE;
        }
    }
};
}
#endif
