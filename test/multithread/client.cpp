#include <signal.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

#include <string>
#include <vector>
#include <iostream>

#include "rop.h"
#include "Log.h"
#include "SocketTransport.h"
#include "test/Test.h"

using namespace std;
using namespace acba;
using namespace rop;
using namespace test;

#define ROP_REMOTEMAP_HOST "localhost"
#define ROP_REMOTEMAP_PORT "8091"

int connect () {
    int ret = -1;
    int sfd = -1;

    // construct params for addr resolving
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    // get addr
    struct addrinfo *srv = NULL;
    if (getaddrinfo(ROP_REMOTEMAP_HOST, ROP_REMOTEMAP_PORT, &hints, &srv)) {
        goto exit;
    }

    // connect
    sfd = socket(srv->ai_family, srv->ai_socktype, srv->ai_protocol);
    if (sfd == -1) goto exit;
    if (connect(sfd, srv->ai_addr, srv->ai_addrlen)) goto exit;

    // success
    fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL) | O_NONBLOCK);
    ret = sfd;

exit:
    if (ret == -1 && sfd != -1) close(sfd);
    if (srv) freeaddrinfo(srv);
    return ret;
}

Ref<SocketTransport> trans;
Ref<Test> testObj;

static void *requestSleep (void *arg)
{
    testObj->sleep(1);
    testObj->sleep(1);
    testObj->sleep(1);
    return 0;
}

static void *run (void *arg)
{
    testObj = new Stub<Test>;
    testObj->remote = trans->registry->getRemote("Test");

    for (int i = 0; i < 1000; i++) {
        while (1) {
            pthread_t t;
            if (pthread_create(&t, 0, requestSleep, 0)) {
                printf("too many threads...\n");
                sleep(1);
                continue;
            }
            pthread_detach(t);
            break;
        }
        usleep(1000);
    }
    return 0;
}


int main ()
{
    Log l("client ");
    int fd = connect();

    EventDriver ed;
    trans = new SocketTransport();
    ed.watchFile(fd, EVENT_READ, trans.get());

    pthread_t t;
    assert(!pthread_create(&t, 0, run, 0));

    ed.run();
    return 0;
}

