#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <string>
#include <vector>
#include "rop.h"
#include "EventDriver.h"
#include "SocketTransport.h"
#include "test/Echo.h"
#include "Log.h"

#define IS_MULTI_THREADED false

#ifndef SOCK_NONBLOCK
#define SOCK_NONBLOCK 0
#endif

using namespace std;
using namespace acba;
using namespace rop;
using namespace test;

struct EchoCallbackImpl: Exportable<EchoCallback> {
    std::string lastMessage;
    int32_t call (const std::string &msg) {
        lastMessage = msg;
        printf("GOT ECHO - %s\n", msg.c_str());
        return 0;
    }

    int32_t call2 (const std::string &msg, int32_t ttl,
            const acba::Ref<test::EchoCallback>  &cb) {
        printf("GOT ECHO2(ttl:%d) - %s\n", ttl, msg.c_str());
        if (ttl-- > 1) {
            cb->call2(msg, ttl, this);
        }
        return 0;
    }
};

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
        printf("SERVER: echo2 begins...\n");
        printf("SERVER: echo2 got msg(ttl:%d): %s\n", ttl, msg.c_str());
        if (ttl-- > 1) {
            cb->call2(msg, ttl, new EchoCallbackImpl());
        }
        return 0;
    }
};

static void test1_server (int sock) {
    Log l("server ");
    EventDriver ed;

    // export
    Registry::add("Echo", new EchoImpl());

    struct _:RegistryListener {
        void registryDisposed (Registry *reg) {
            Log l("server ");
            l.info("registry disposed:%08x\n", reg);
        }
    } lnr;;

    Ref<SocketTransport> trans = new SocketTransport(IS_MULTI_THREADED);
    trans->setOutBufferLimit(1);
    trans->registry->addRegistryListener(&lnr);
    ed.watchFile(sock, acba::EVENT_READ, trans);

    l.info("enter loop...\n");
    ed.run();
}

static void test1_client (int sock) {
    Log l("client ");

    EventDriver ed;

    Ref<SocketTransport> trans = new SocketTransport(IS_MULTI_THREADED);
    trans->setOutBufferLimit(1);
    ed.watchFile(sock, acba::EVENT_READ, trans);

    { // isolate lifecycle of objects

    l.info("getting remote Echo...\n");
    Ref<Echo> e = new Stub<Echo>();
    e->remote = trans->registry->getRemote("Echo");
    l.info("using Echo(remote:%08x)...\n", e->remote.get());

    EchoCallbackImpl *cbp = new EchoCallbackImpl();
    Ref<EchoCallback> cb = cbp;

    l.info("BEGIN echo\n");
    {
        string ret;
        if (e->echo(ret, new RefCounted<string>("hi"))) {
            l.error("echo failed.\n");
        }
        l.info("%s\n", ret.c_str());
    }

    l.info("BEGIN concat\n");
    {
        vector<string> args;
        args.push_back("a");
        args.push_back("b");
        args.push_back("c");
        string ret;
        if (e->concat(ret, args)) {
            l.error("concat failed.\n");
        }
        l.info("%s\n", ret.c_str());
    }

    l.info("BEGIN touchmenot\n");
    {
        Ref<TestException> ex1;
        printf("Invoking touchmenot()\n");
        switch (e->touchmenot(ex1)) {
        default:
            l.error("Unexpected return\n");
            break;
        case 0:
            printf("Silently returned.\n");
            break;
        case 1:
            printf("Got exception :%d\n", ex1->i);
            break;
        }
    }

    l.info("BEGIN recursiveEcho\n");

    cbp->lastMessage.clear();
    string m = "rrrrrrrrrrrrrrrrrrrrrrrrrrrrr";
    if (e->recursiveEcho(m, cb)) {
        l.error("recursiveEcho failed\n");
    }
    if (cbp->lastMessage != m) {
        l.error("recursiveEcho failed. expected callback before return\n");
    }

    l.info("BEGIN hello\n");
    {
        Person p;
        p.name = "Sooin";
        p.callback = cb;

        if (e->hello(p)) {
            l.error("hello failed.\n");
        }
    }

    l.info("BEGIN asyncEcho\n");
    // callback request will be handled at the end.
    for (int i = 0; i < 10; i++) {
        if (e->asyncEcho("hi again", cb)) {
            l.error("asyncEcho failed.\n");
        }
    }

    l.info("BEGIN doubleit\n");
    {
        int32_t ret;
        if (e->doubleit(ret, 3) == 0) {
            printf("doubleit(3) returns %d\n", ret);
        } else {
            l.error("doubleit failed.\n");
        }
    }

    l.info("BEGIN echoVariant\n");
    {
        Variant ret;
        Variant obj("hi");
        obj = new Variant::Map();
        obj.asMap()->operator[]("msg") = new Variant::List();
        obj.asMap()->operator[]("msg").asList()->push_back("say \"Hello world!\"");
        obj.asMap()->operator[]("ABC") = 123;
        obj.asMap()->operator[](123) = "ABC";
        obj.asMap()->operator[]("ABCDEF") = 123456789;
        obj.asMap()->operator[](123456789) = "ABCDEF";
        obj.asMap()->operator[](true) = false;
        obj.asMap()->operator[](false) = true;
        if (e->echoVariant(ret, obj)) {
            l.error("echoVariant failed.\n");
        }
        l.info("returned same? %s\n",
                (obj.toString() == ret.toString()) ? "true" : "false");
    }

    l.info("BEGIN echo2\n");
    {
        e->echo2("yahooooooooooooooooooo", 15, new EchoCallbackImpl());
    }

    l.info("END all tests.\n");

    } // delete all objects

    trans->close(); // must be called in event thread.
}

void test1 ()
{
    int socks[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, socks) < 0) {
        perror("socketpair error");
        exit(1);
    }

    if (fork()) {
        close(socks[0]);
        test1_server(socks[1]);
    } else {
        close(socks[1]);
        test1_client(socks[0]);
    }
}

int main ()
{
    test1();
    return 0;
}

