#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "rop.h"
#include "HttpTransport.h"
#include "test/Test.h"
#include "test/Callback.h"
#include "Log.h"
#include "RopServer.h"

using namespace std;
using namespace acba;
using namespace rop;
using namespace test;

static Ref<Callback> callback;

static void *notifyCallback (void *arg)
{
    printf("sending hi1...\n");
    callback->run("hi1");
    printf("sending hi2...\n");
    callback->run("hi2");
    return 0;
}

struct TestImpl: Exportable<Test> {
    // @rop async setCallback(Callback cb);
    int32_t setCallback (const Ref<Callback> &cb) {
        callback = cb;
        pthread_t t;
        pthread_create(&t, 0, notifyCallback, 0);
        return 0;
    }

    // @rop String echo (String msg);
    int32_t echo (std::string &ret, const std::string &msg) {
        printf("echo %s\n", msg.c_str());
        ret = msg;
        return 0;
    }
};

int main ()
{
    Log l("server ");

    // server
    EventDriver ed;
    RopServer srv;
    srv.add("test.Test", new TestImpl());
    srv.start(1304, 1305, &ed);
    ed.run();
    return 0;
}

