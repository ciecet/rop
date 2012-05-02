#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include "Remote.h"
#include "Unix.h"
#include "Types.h"

void test1 ()
{
    int p0[2], p1[2];
    pipe2(p0, O_NONBLOCK);
    pipe2(p1, O_NONBLOCK);

    Registry reg;

    if (fork()) {
        // client
        UnixTransport trans(reg, p0[0], p1[1]);
        sleep(1);

        Stub<Echo> e;
        e.remote = reg.getRemote("Echo");

        printf("%s\n", e.echo("hi!").c_str());
        vector<string> args;
        args.push_back("a");
        args.push_back("b");
        args.push_back("c");
        printf("%s\n", e.concat(args).cstr());
    } else {
        // server
        UnixTransport trans(reg, p1[0], p0[1]);
        reg.registerExportable("Echo", new EchoImpl());
        trans.loop();
    }
}

int main ()
{
    test1();
    return 0;
}

