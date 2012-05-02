#include <stdio.h>
#include "Ref.h"

ContainerRef<int> func () {
    return new Container<int>();
}

int main () {
    ContainerRef<int> r = func();
    return 0;
}

