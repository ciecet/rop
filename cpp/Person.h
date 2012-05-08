#ifndef PERSON_H
#define PERSON_H
#include "Remote.h"
#include "EchoCallback.h"

namespace test {
struct Person {
    std::string name;
    base::Ref<test::EchoCallback>  callback;
};
}

namespace rop {
template<>
struct Reader <test::Person>: base::Frame {
    test::Person &object;
    Reader (test::Person &o): object(o) {}
    STATE run (base::Stack *stack) {
        switch (step) {
        case 0: stack->push(new(stack->allocate(sizeof(Reader<std::string>))) Reader<std::string>(object.name)); step++; return CONTINUE;
        case 1: stack->push(new(stack->allocate(sizeof(Reader<base::Ref<test::EchoCallback> >))) Reader<base::Ref<test::EchoCallback> >(object.callback)); step++; return CONTINUE;
        default: return COMPLETE;
        }
    }
};
template<>
struct Writer <test::Person>: base::Frame {
    test::Person &object;
    Writer (test::Person &o): object(o) {}
    STATE run (base::Stack *stack) {
        switch (step) {
        case 0: stack->push(new(stack->allocate(sizeof(Writer<std::string>))) Writer<std::string>(object.name)); step++; return CONTINUE;
        case 1: stack->push(new(stack->allocate(sizeof(Writer<base::Ref<test::EchoCallback> >))) Writer<base::Ref<test::EchoCallback> >(object.callback)); step++; return CONTINUE;
        default: return COMPLETE;
        }
    }
};
}
#endif
