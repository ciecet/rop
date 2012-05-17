#include "Remote.h"
#include "Log.h"

using namespace std;
using namespace rop;

struct RegistrySkeleton: SkeletonBase {
    Registry *registry;

    RegistrySkeleton (Registry *r): SkeletonBase(0), registry(r) {}

    struct __req_getRemote: Request {
        Registry *registry;
        ArgumentsReader<string> args;
        ReturnWriter<int32_t> ret;
        __req_getRemote(Registry *reg): registry(reg) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            Log l("getRemote ");
            l.debug("finding %s\n", args.get<string>(0).c_str());
            map<string,InterfaceRef>::iterator i =
                    registry->exportables.find(args.get<string>(0));
            if (i == registry->exportables.end()) {
                ret.get<int32_t>(0) = 0;
                ret.index = 0;
                l.debug("not found\n");
                return;
            }
            SkeletonBase *skel = registry->getSkeleton(i->second.get());
            ret.get<int32_t>(0) = skel->id;
            ret.index = 0;
            l.debug("found %08x(%d)\n", skel, skel->id);
        }
    };

    struct __req_notifyRemoteDestroy: Request {
        Registry *registry;
        ArgumentsReader<int32_t, int32_t> args;
        __req_notifyRemoteDestroy(Registry *reg): registry(reg) {
            argumentsReader = &args;
            returnWriter = 0;
        }

        void call () {
            Log l("notifyRemoteDestroy ");
            int32_t id = -args.get<int32_t>(0); // reverse local<->remote
            int32_t stamp = args.get<int32_t>(1);

            SkeletonBase *skel = registry->skeletons[id];
            if (skel->stamp > stamp) {
                l.debug("Skipped skeleton deletion.\n");
                return;
            }

            l.debug("deleting skeleton:%d\n", id);
            registry->skeletons.erase(id);
            registry->skeletonByExportable.erase(skel->object.get());
            delete skel;
        }
    };

    Request *createRequest (int methodIndex) {
        switch (methodIndex) {
        case 0: return new __req_getRemote(registry);
        case 1: return new __req_notifyRemoteDestroy(registry);
        default: return 0;
        }
    }
};

SkeletonBase *Registry::createSkeleton ()
{
    return new RegistrySkeleton(this);
}

Remote *Registry::getRemote (string objname)
{
    Port* p = transport->getPort();

    RequestWriter<1> req(0, 0, 0);
    Writer<string> arg0(objname);
    req.args[0] = &arg0;
    p->writer.push(&req);

    ReturnReader<int32_t> ret;
    p->sendAndWait(&ret); // may throw exception
    return getRemote(-ret.get<int32_t>(0)); // reverse local<->remote
}

void Registry::notifyRemoteDestroy (int32_t id, int32_t stamp)
{
    remotes.erase(id);

    Port *p = transport->getPort(0);

    Writer<int32_t> arg0(id);
    Writer<int32_t> arg1(stamp);
    RequestWriter<2> req(0x1<<6, 0, 1);
    req.args[0] = &arg0;
    req.args[1] = &arg1;
    p->writer.push(&req);
    p->send(0);
}

Remote *Registry::getRemote (int id)
{
    Remote *&r = remotes[id];
    if (!r) {
        r = new Remote();
        r->id = id;
        r->registry = this;
    }
    return r;
}

SkeletonBase *Registry::getSkeleton (int id)
{
    map<int,SkeletonBase*>::iterator i = skeletons.find(id);
    if (i == skeletons.end()) {
        return 0;
    }
    return i->second;
}

SkeletonBase *Registry::getSkeleton (Interface *obj)
{
    SkeletonBase *&skel = skeletonByExportable[obj];
    if (!skel) {
        skel = obj->createSkeleton();
        skel->id = nextSkeletonId++;
        skel->stamp = 0;
        skeletons[skel->id] = skel;
    }
    return skel;
}

Registry::~Registry ()
{
    Log l("~reg ");
    // Don't send notifyRemoteDestroy because peer's skeletons will be deleted
    // anyway after connection-loss detection.
    for (map<int,Remote*>::iterator i = remotes.begin();
            i != remotes.end(); i++) {
        i->second->registry = 0;
    }
    for (map<int,SkeletonBase*>::iterator i = skeletons.begin();
            i != skeletons.end(); i++) {
        l.debug("removing skel:%d\n", i->second->id);
        delete i->second;
    }
}
