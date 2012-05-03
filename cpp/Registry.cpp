#include "Remote.h"

using namespace rop;

struct RegistrySkeleton: SkeletonBase {
    Registry *registry;

    RegistrySkeleton (Registry *r): SkeletonBase(0), registry(0) {}

    struct __req_getRemote: Request {
        Registry *registry;
        __req_getRemote(Registry *reg): registry(reg) {}

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

        void call (Port *p) {
            map<string,InterfaceRef>::iterator i =
                    registry->exportables.find(args.arg0);
            if (i == registry->exportables.end()) {
                ret.ret0 = 0;
                ret.index = 0;
                return;
            }
            SkeletonBase *skel = registry->getSkeleton(i->second.get());
            ret.ret0 = skel->id;
            ret.index = 0;
        }
    };

    Request *createRequest (int methodIndex) {
        switch (methodIndex) {
        case 0: return new __req_getRemote(registry);
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

    struct _:ReturnReader<1> {
        int value0;
        Reader<int> frame0;
        _(): frame0(value0) {
            values[0] = &value0;
            frames[0] = &frame0;
        }
    } ret;
    p->addReturn(&ret);

    SequenceWriter<1> sw;
    Writer<string> arg0(objname);
    sw.frames[0] = &arg0;
    p->writer.push(&sw);

    p->flushAndWait(); // may throw exception
    return getRemote(ret.value0);
}

void Registry::notifyRemoteDestroy (int id)
{
    Port *p = transport->getPort();

    SequenceWriter<1> sw;
    Writer<int32_t> arg0(id);
    sw.frames[0] = &arg0;
    p->writer.push(&sw);
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
