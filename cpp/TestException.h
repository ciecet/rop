#ifndef TEST_EXCEPTION_H
#define TEST_EXCEPTION_H

#include <exception>
#include "Remote.h"
#include "Stack.h"

namespace test {

struct TestException: std::exception {
    int32_t i;
    TestException(): i(0) {}
    TestException(int32_t v): i(v) {}
};

}

namespace rop {

template<> struct Reader<test::TestException>: base::Frame {
    test::TestException &obj;
    Reader (test::TestException &o): obj(o) {}
    STATE run (base::Stack *stack) {
        BEGIN_STEP();
        stack->push(new(stack->allocate(sizeof(Reader<int32_t>)))
                Reader<int32_t>(obj.i));
        CALL();

        END_STEP();
    }
};

template<> struct Writer<test::TestException>: base::Frame {
    test::TestException &obj;
    Writer (test::TestException &o): obj(o) {}
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
