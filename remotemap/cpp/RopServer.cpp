#include <stdio.h>
#include <vector>
#include <sstream>
#include <string>

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

#include "rop.h"
#include "EventDriver.h"
#include "SocketTransport.h"
#include "HttpTransport.h"
#include "RopServer.h"
#include "rop/RemoteMap.h"

using namespace std;
using namespace rop;
using namespace acba;

#ifndef ROP_REMOTEMAP_HOST
#define ROP_REMOTEMAP_HOST "localhost"
#endif
#ifndef ROP_REMOTEMAP_PORT
#define ROP_REMOTEMAP_PORT "1302"
#endif

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

int RopServer::start (int socketPort, int httpPort, EventDriver *ed)
{
    vector<string> locs;
    if (socketPort > 0) {
        socketServer.setExportables(exportables);
        if (socketServer.start(socketPort, ed)) return -1;
        ostringstream loc;
        loc << "rop://localhost:";
        loc << socketPort;
        locs.push_back(loc.str().c_str());
        log.info("Running server as %s\n", loc.str().c_str());

    }

    if (httpPort > 0) {
        httpServer.setExportables(exportables);
        if (httpServer.start(httpPort, ed)) return -1;
        ostringstream loc;
        loc << "rop-http://localhost:";
        loc << httpPort;
        locs.push_back(loc.str().c_str());
        log.info("Running server as %s\n", loc.str().c_str());
    }

    // notify remote map

    int fd = connect();
    if (fd < 0) {
        log.warn("Failed to connect to remotemap server\n");
        return -1;
    }

    Ref<SocketTransport> trans = new SocketTransport();
    ed->watchFile(fd, EVENT_READ, trans);

    Ref<RemoteMap> rmap = new Stub<RemoteMap>;
    rmap->remote = trans->registry->getRemote("rop.RemoteMap");
    if (rmap->remote) {
        vector<string> rnames;
        for (Registry::ExportableMap::iterator i = exportables.begin();
                i != exportables.end(); i++) {
            rnames.push_back(i->first);
        }
        rmap->registerLocations (rnames, locs);
    } else {
        log.warn("Failed to get remotemap object\n");
    }

    trans->close();
    return 0;
}
