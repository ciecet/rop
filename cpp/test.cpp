#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "Remote.h"
#include "UnixTransport.h"
#include "Echo.h"
#include "Log.h"

using namespace std;
using namespace rop;

struct EchoImpl: Exportable<Echo> {
    string echo (string msg);
    string concat (vector<string> msgs);
};

string EchoImpl::echo (string msg)
{
    printf("ECHO MESSAGE - %s\n", msg.c_str());
    return msg;
}

string EchoImpl::concat (vector<string> msgs)
{
    printf("CONCAT MESSAGE - %d\n", msgs.size());
    string ret;
    for (vector<string>::iterator i = msgs.begin(); i != msgs.end(); i++) {
        ret += *i;
    }
    return ret;
}

void test1 ()
{
    int p0[2], p1[2];
    pipe2(p0, O_NONBLOCK);
    pipe2(p1, O_NONBLOCK);

    Registry reg;

    if (fork()) {
        Log l("client ");
        // client
        UnixTransport trans(reg, p0[0], p1[1]);

        l.info("getting remote Echo...\n");
        Stub<Echo> e;
        e.remote = reg.getRemote("Echo");

        l.info("using Echo...\n");
        printf("%s\n", e.echo("hi!").c_str());
        vector<string> args;
        args.push_back("a");
        args.push_back("b");
        args.push_back("c");
        printf("%s\n", e.concat(args).c_str());
    } else {
        Log l("server ");
        // server
        UnixTransport trans(reg, p1[0], p0[1]);
        reg.registerExportable("Echo", new EchoImpl());
        l.info("enter loop...\n");
        trans.loop();
    }
}

int main ()
{
    test1();
    return 0;
}

