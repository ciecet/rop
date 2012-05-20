#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "Remote.h"
#include "HttpTransport.h"
#include "Echo.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;
using namespace test;

struct EchoImpl: Exportable<Echo> {
    string echo (string msg) {
        printf("ECHO MESSAGE - %s\n", msg.c_str());
        return msg;
    }
    string concat (vector<string> msgs) {
        printf("CONCAT MESSAGE - %d\n", msgs.size());
        string ret;
        for (vector<string>::iterator i = msgs.begin(); i != msgs.end(); i++) {
            ret += *i;
        }
        return ret;
    }
    void touchmenot () {
        printf("THROW 3!\n");
        ContainerRef<int> i = new Container<int>();
        *i = 3;
        throw TestException(i);
    }
    void recursiveEcho (string msg, Ref<EchoCallback> cb) {
        printf("RECURSIVE ECHO - %s\n", msg.c_str());
        cb->call(msg);
    }
    void hello (test::Person &p) {
        string greeting("Hello, ");
        p.callback->call(greeting + p.name);
    }
};

void serv ()
{
    Log l("server ");
    // server
    Registry reg;
    reg.registerExportable("Echo", new EchoImpl());
    HttpTransport trans(8080);
    reg.setTransport(&trans);
    l.info("enter loop...\n");
    trans.loop();
}

int main ()
{
    serv();
    return 0;
}