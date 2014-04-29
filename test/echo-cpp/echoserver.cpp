#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "rop.h"
#include "HttpTransport.h"
#include "test/Echo.h"
#include "Log.h"

using namespace std;
using namespace acba;
using namespace rop;
using namespace test;

struct EchoImpl: Exportable<Echo> {
    int32_t echo (std::string &ret, const Ref<RefCounted<string> > &msg) {
        if (msg) {
            printf("SERVER: echo message - %s\n", msg->c_str());
            ret = *msg;
        } else {
            printf("SERVER: got emtpy message\n");
            ret = "bye";
        }

        printf("SERVER: echo returns %s.\n", ret.c_str());
        return 0;
    }

    int32_t concat (std::string &ret, const std::vector<std::string> &msgs) {
        string s;
        for (vector<string>::const_iterator i = msgs.begin(); i != msgs.end(); i++) {
            s += *i;
        }
        ret = s;
        printf("SERVER: concat returns %s.\n", ret.c_str());
        return 0;
    }

    int32_t touchmenot (acba::Ref<test::TestException> &ex1) {
        printf("SERVER: touchmenot throws exception.\n");
        {
            ex1 = new TestException();
            ex1->i = 3;
        }
        return 1;
    }

    int32_t recursiveEcho (const std::string &msg, const Ref<EchoCallback> &cb) {
        printf("SERVER: recursiveEcho(%s) calls callback.\n", msg.c_str());
        return cb->call(msg);
    }

    int32_t hello (const test::Person &p) {
        printf("SERVER: hello(name:%s) calls callback.\n", p.name.c_str());
        string greeting("Hello, ");
        return p.callback->call(greeting + p.name);
    }

    int32_t asyncEcho (const std::string &msg, const acba::Ref<test::EchoCallback> &cb) {
        printf("SERVER: asyncEcho(%s) calls callback\n", msg.c_str());
        return cb->call(msg);
    }

    int32_t doubleit (int32_t &ret, int32_t i) {
        ret = i * 2;
        printf("SERVER: doubleit(%d) returns... %d\n", i, ret);
        return 0;
    }

    int32_t echoVariant (acba::Variant &ret, const acba::Variant &obj) {
        printf("SERVER: echoVariant got an obj: %s\n", obj.toString().c_str());
        ret = obj;
        printf("SERVER: returns: %s\n", ret.toString().c_str());
        return 0;
    }

    int32_t echo2 (const std::string &msg, int32_t ttl,
            const acba::Ref<test::EchoCallback> &cb) {
        printf("SERVER: echo2 not supported.\n");
        return 0;
    }
};

void serv ()
{
    Log l("server ");

    Registry::add("Echo", new EchoImpl());

    // server
    EventDriver ed;
    HttpServer srv;
    srv.start(&ed, 8091);
    ed.run();
}

int main ()
{
    serv();
    return 0;
}
