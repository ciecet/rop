#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "Remote.h"
#include "SocketTransport.h"
#include "Echo.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;
using namespace test;

struct EchoCallbackImpl: Exportable<EchoCallback> {
    void call (std::string msg) {
        printf("GOT ECHO - %s\n", msg.c_str());
    }
};

struct EchoImpl: Exportable<Echo> {
    string echo (string msg);
    string concat (vector<string> msgs);
    void touchmenot ();
    void recursiveEcho (string msg, Ref<EchoCallback> cb);
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

void EchoImpl::touchmenot ()
{
    printf("THROW 3!\n");
    throw TestException(3);
}

void EchoImpl::recursiveEcho (string msg, Ref<EchoCallback> cb)
{
    printf("RECURSIVE ECHO - %s\n", msg.c_str());
    cb->call(msg);
}

void test1 ()
{
    int p0[2], p1[2];
    pipe2(p0, O_NONBLOCK);
    pipe2(p1, O_NONBLOCK);

    if (fork()) {
        Log l("client ");
        // client
        Registry reg;
        SocketTransport trans(reg, p0[0], p1[1]);

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
        try {
            printf("Invoking touchmenot()\n");
            e.touchmenot();
            printf("Silently returned.\n");
        } catch (TestException &e) {
            printf("Got exception :%d\n", e.i);
        }
        e.recursiveEcho("rrrrrrrrrrrrrrrrrrrrrrrrrrr", new EchoCallbackImpl());
    } else {
        Log l("server ");
        // server
        Registry reg;
        SocketTransport trans(reg, p1[0], p0[1]);
        reg.registerExportable("Echo", new EchoImpl());
        l.info("enter loop...\n");
        trans.loop();
    }

    sleep(5);
}

int main ()
{
    test1();
    return 0;
}

