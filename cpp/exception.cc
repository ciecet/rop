#include <stdio.h>
#include "Ref.h"

struct A {
    A() { printf("exception was created. %08x\n", this); }
    ~A() { printf("exception was destroyed. %08x\n", this); }
};

int main () {
    try {
        ContainerRef<A> e = new Container<A>();
        throw e;
    } catch (ContainerRef<A> e) {
        printf("got exception.\n");
    }
    return 0;
}
