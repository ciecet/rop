#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include "rop.h"
#include "EventDriver.h"
#include "SocketTransport.h"
#include "test/Echo.h"
#include "Log.h"

using namespace std;
using namespace acba;
using namespace rop;
using namespace test;

struct EchoCallbackImpl: Exportable<EchoCallback> {
    int32_t call (const std::string &msg) {
        printf("GOT ECHO - %s\n", msg.c_str());
        return 0;
    }
};

void test1 ()
{
    int sockfd;
    sockaddr_in serv_addr;
    hostent *server;

    /* Create a socket point */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        exit(1);
    }
    server = gethostbyname("localhost");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr,
            server->h_length);
    serv_addr.sin_port = htons(1390);

    /* Now connect to the server */
    if (::connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr), sizeof(serv_addr)) < 0) 
    {
        perror("ERROR connecting");
        exit(1);
    }

    EventDriver ed;
    SocketTransport trans(sockfd);
    ed.add(&trans);

    struct _: AsyncReactor {
        Registry *reg;
        _ (Registry *r): reg(r) {}
        void processEvent (int ev) {
            Log l("client ");

            // client

            l.info("getting remote Echo...\n");
            Stub<Echo> e;
            e.remote = reg->getRemote("test.Echo");

            l.info("using Echo(remote:%08x)...\n", e.remote.get());
            {
                string ret;
                if (e.echo(ret, "hi")) {
                    l.error("echo failed.\n");
                }
                printf("%s\n", ret.c_str());
            }

            {
                vector<string> args;
                args.push_back("a");
                args.push_back("b");
                args.push_back("c");
                string ret;
                if (e.concat(ret, args)) {
                    l.error("concat failed.\n");
                }
                printf("%s\n", ret.c_str());
            }

            {
                Ref<TestException> ex1;
                printf("Invoking touchmenot()\n");
                switch (e.touchmenot(ex1)) {
                default:
                    l.error("Unexpected return\n");
                    break;
                case 0:
                    printf("Silently returned.\n");
                    break;
                case 1:
                    printf("Got exception :%d\n", *ex1->i);
                    break;
                }
            }

            Ref<EchoCallback> cb = new EchoCallbackImpl();

            if (e.recursiveEcho("rrrrrrrrrrrrrrrrrrrrrrrrrrr", cb)) {
                l.error("recursiveEcho failed\n");
            }

            Person p;
            p.name = "Sooin";
            p.callback = cb;

            if (e.hello(p)) {
                l.error("hello failed.\n");
            }

            if (e.asyncEcho("hi again", cb)) {
                l.error("asyncEcho failed.\n");
            }

            printf("starting doubleit test.\n");
            {
                int32_t ret;
                if (e.doubleit(ret, 3) == 0) {
                    printf("doubleit(3) returns %d\n", ret);
                } else {
                    l.error("doubleit failed.\n");
                }
            }
        }
    } ar(trans.registry);
    ed.add(&ar);
    ed.loop();
}

int main ()
{
    test1();
    return 0;
}

