#include <stdio.h>
#include <string>
#include <vector>
#include <map>
#include "rop.h"

using namespace std;
using namespace acba;
using namespace rop;

int main ()
{
    map<string,Variant> props;
    props["prop1"] = 1;
    props["prop2"] = "hi";

    Buffer buf;
    Writer<map<string,Variant> >::run(props, buf);

    map<string,Variant> props2;
    Reader<map<string,Variant> >::run(props2, buf);

    printf("prop1=%d\n", props2["prop1"].asI32());
    printf("prop2=%s\n", props2["prop2"].asString().c_str());
    return 0;
}

