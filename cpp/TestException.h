#ifndef TEST_EXCEPTION_H
#define TEST_EXCEPTION_H

#include "Remote.h"
#include "Stack.h"

struct TestException {
    int32_t i;
    TestException(): i(0) {}
    TestException(int32_t v): i(v) {}
};

namespace rop {

template<> struct Reader<TestException>: base::Frame {
    TestException &obj;
    Reader (TestException &o): obj(o) {}
    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        stack->push(new(stack->allocate(sizeof(Reader<int32_t>)))
                Reader<int32_t>(obj.i));
        CALL();

        END_STEP();
    }
};

template<> struct Writer<TestException>: base::Frame {
    TestException &obj;
    Writer (TestException &o): obj(o) {}
    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        stack->push(new (stack->allocate(sizeof(Writer<int32_t>)))
                Writer<int32_t>(obj.i));
        CALL();

        END_STEP();
    }
};

}

#endif
