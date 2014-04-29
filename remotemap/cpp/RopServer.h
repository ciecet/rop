#ifndef REMOTEMAP_H
#define REMOTEMAP_H

#include <stdio.h>
#include <vector>
#include <string>
#include "rop.h"
#include "EventDriver.h"
#include "SocketTransport.h"
#include "HttpTransport.h"
#include "rop/RemoteMap.h"

namespace rop {

// TODO: clean-up on dispose
class RopServer {
    std::map<std::string,InterfaceRef> exportables;
    SocketServer socketServer;
    HttpServer httpServer;

    acba::Log log;

public:
    RopServer (bool mt = true):
            socketServer(mt), httpServer(mt),
            log("ropsrv ") {}
    void add (std::string objname, InterfaceRef exp) {
        exportables[objname] = exp;
    }
    int start (int socketPort, int httpPort, acba::EventDriver *ed);
    int start (int socketPort, acba::EventDriver *ed) {
        return start(socketPort, -1, ed);
    }
};

}

#endif

