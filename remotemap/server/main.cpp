#include <stdio.h>
#include <vector>
#include <string>
#include "rop.h"
#include "Log.h"
#include "EventDriver.h"
#include "SocketTransport.h"
#include "HttpTransport.h"
#include "rop/RemoteMap.h"

using namespace std;
using namespace acba;
using namespace rop;

class RemoteMapImpl: public Exportable<RemoteMap> {
    typedef map<string,vector<string> > RemoteMap;
    RemoteMap remoteMap;

    Log log;

public:
    RemoteMapImpl (): log("remotemap ") {}

    int32_t findLocations (vector<string>  &ret, const string &rname,
            const string &proto);
    int32_t registerLocations (const vector<string> &rnames,
            const vector<string> &locs);
};

int32_t RemoteMapImpl::findLocations (vector<string> &ret,
        const string &rname, const string &proto)
{
    log.info("Finding remote object:%s proto:%s\n", rname.c_str(),
            proto.c_str());

    RemoteMap::iterator i = remoteMap.find(rname);
    if (i == remoteMap.end()) {
        log.info("Not found\n");
        return 0;
    }
    vector<string> &locs = i->second;

    if (proto == "*") {
        for (vector<string>::iterator j = locs.begin(); j != locs.end(); j++) {
            log.info("Found location:%s\n", j->c_str());
        }
        ret = locs;
        return 0;
    }

    for (vector<string>::iterator j = locs.begin(); j != locs.end(); j++) {
        if (j->compare(0, proto.size(), proto) != 0) continue;
        log.info("Found location:%s\n", j->c_str());
        ret.push_back(*j);
    }
    return 0;
}

int32_t RemoteMapImpl::registerLocations (const vector<string> &rnames,
        const vector<string> &locs)
{
    for (vector<string>::const_iterator i = rnames.begin(); i != rnames.end();
            i++) {
        log.info("Registering remote object:%s\n", i->c_str());
        vector<string> &_locs = remoteMap[*i];
        if (_locs.size() > 0) {
            log.warn("Overwriting...\n");
        }
        _locs = locs;
        for (vector<string>::const_iterator j = locs.begin(); j != locs.end();
                j++) {
            log.info("Location: %s\n", j->c_str());
        }
    }
    return 0;
}

int main () {
    Registry::add("rop.RemoteMap", new RemoteMapImpl);

    EventDriver ed;
    SocketServer srv;
    srv.start(&ed, 1302);
    HttpServer hsrv;
    hsrv.start(&ed, 1303);

    printf("Running remotemap server...\n");
    ed.run();
    return 0;
}
