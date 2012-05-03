#include "Remote.h"
#include "Log.h"

using namespace rop;

struct RegistrySkeleton: SkeletonBase {
    Registry *registry;

    RegistrySkeleton (Registry *r): SkeletonBase(0), registry(r) {}

    struct __req_getRemote: Request {
        Registry *registry;

        struct args_t: SequenceReader<1> {
            string arg0;
            Reader<string> frame0;
            args_t(): frame0(arg0) {
                values[0] = &arg0;
                frames[0] = &frame0;
            }
        } args;

        struct ret_t: ReturnWriter<1> {
            int ret0;
            Writer<int> frame0;
            ret_t(): frame0(ret0) {
                frames[0] = &frame0;
            }
        } ret;

        __req_getRemote(Registry *reg): registry(reg) {
            argumentsReader = &args;
            returnWriter = &ret;
        }

        void call () {
            Log l("getRemote ");
            l.debug("finding %s\n", args.arg0.c_str());
            map<string,InterfaceRef>::iterator i =
                    registry->exportables.find(args.arg0);
            if (i == registry->exportables.end()) {
                ret.ret0 = 0;
                ret.index = 0;
                l.debug("not found\n");
                return;
            }
            SkeletonBase *skel = registry->getSkeleton(i->second.get());
            ret.ret0 = skel->id;
            ret.index = 0;
            l.debug("found %08x(%d)\n", skel, skel->id);
        }
    };

    struct __req_notifyRemoteDestroy: Request {
        Registry *registry;

        struct args_t: SequenceReader<1> {
            int32_t arg0;
            Reader<int32_t> frame0;
            args_t(): frame0(arg0) {
                values[0] = &arg0;
                frames[0] = &frame0;
            }
        } args;

        __req_notifyRemoteDestroy(Registry *reg): registry(reg) {
            argumentsReader = &args;
            returnWriter = 0;
        }

        void call () {
            Log l("notifyRemoteDestroy ");
            int id = -args.arg0; // reverse local<->remote
            l.debug("deleting skeleton:%d\n", id);

            SkeletonBase *skel = registry->skeletons[id];
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

    struct _:ReturnReader<1> {
        int value0;
        Reader<int> frame0;
        _(): frame0(value0) {
            values[0] = &value0;
            frames[0] = &frame0;
        }
    } ret;
    p->addReturn(&ret);

    p->flushAndWait(); // may throw exception
    return getRemote(-ret.value0); // reverse local<->remote
}

void Registry::notifyRemoteDestroy (int id)
{
    Port *p = transport->getPort();

    RequestWriter<1> req(0x1<<6, 0, 1);
    Writer<int32_t> arg0(id);
    req.args[0] = &arg0;
    p->writer.push(&req);
    p->flush();
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
        skeletons[skel->id] = skel;
    }
    return skel;
}

Registry::~Registry ()
{
    for (map<int,Remote*>::iterator i = remotes.begin();
            i != remotes.end(); i++) {
        i->second->registry = 0;
    }
    for (map<int,SkeletonBase*>::iterator i = skeletons.begin();
            i != skeletons.end(); i++) {
        delete i->second;
    }
}
