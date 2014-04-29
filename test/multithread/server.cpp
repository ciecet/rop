#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "rop.h"
#include "Log.h"
#include "SocketTransport.h"
#include "test/Test.h"

using namespace std;
using namespace acba;
using namespace rop;
using namespace test;

struct TestImpl: Exportable<Test> {
    int32_t sleep (int32_t sec) {
        ::sleep(sec);
        return 0;
    }
};

int main ()
{
    Log l("server ");

    Registry::add("Test", new TestImpl());

    // server
    EventDriver ed;
    SocketServer srv;
    srv.start(8091, &ed);
    ed.run();
    return 0;
}

