import java.io.*;
import java.util.*;
import java.lang.ref.*;

public class Registry {

    private static int nextThreadId = 1;

    private static ThreadLocal threadIds = new ThreadLocal () {
        protected synchronized Object initialValue () {
            Log.warning("nextThreadId... :"+nextThreadId+" thread:"+Thread.currentThread());
            return new int[] { nextThreadId++, 0 }; // local / rpc
        }
    };
    public static int[] getThreadIds () {
        return (int[])threadIds.get();
    }

    private int nextSkeletonId = 0;
    private Map exportables = new HashMap();
    private Map remotes = new HashMap();
    private Map skeletons = new HashMap();
    private Map skeletonByExportable = new IdentityHashMap();
    public Map ports = new HashMap();
    private boolean isClosed = false;

    public final Transport transport;

    // Created by transport.
    public Registry (Transport trans) {
        Skeleton skel = new RegistrySkeleton();
        skel.id = nextSkeletonId++;
        skeletons.put(new Integer(skel.id), skel);
        transport = trans;
    }

    // unsafe
    public void registerExportable (String name, Exportable e) {
        exportables.put(name.intern(), e);
    }

    // unsafe
    public Port getPort () {
        int[] tids = (int[])getThreadIds();
        return getPort((tids[1] != 0) ? tids[1] : tids[0]);
    }

    // unsafe
    public Port getPort (int pid) {
        Port p = (Port)ports.get(New.i32(pid));
        if (p == null) {
            synchronized (New.class) {
                p = New.port(this);
            }
            p.id = pid;
            ports.put(New.i32(pid), p);
        }
        p.ref();
        return p;
    }
    
    void asyncCall (Writer args, ReturnReader ret) {
        Port p;
        synchronized (this) {
            checkClosed();
            p = getPort();
            Log.fatal("async p:"+p.id);
            p.writer = args;
            if (ret != null) {
                p.addReturn(ret);
            }
        }
        transport.send(p);
        synchronized (this) {
            p.deref();
        }
    }

    // safe
    void syncCall (Writer args, ReturnReader ret) {
        Port p;
        Thread oldProcessingThread;
        synchronized (this) {
            checkClosed();
            p = getPort();
            p.writer = args;
            p.addReturn(ret);
            oldProcessingThread = p.processingThread;
            p.processingThread = Thread.currentThread();
        }

        transport.send(p);
        
        while (!ret.isValid) {
            transport.receive(p);
            while (p.processRequest());
        }

        synchronized (this) {
            p.processingThread = oldProcessingThread;
            p.deref();
        }

        if (ret.index == -1) {
            throw new RemoteException("sync call failed.");
        }
    }

    // safe
    public void dispose () {
        synchronized (this) {
            if (isClosed) return;
            isClosed = true;
            remotes.clear();
            skeletons.clear();
            skeletonByExportable.clear();
        }
    }

    // unsafe
    public Remote getRemote (int id) {
        Remote r = null;
        Integer id2 = New.i32(id);
        WeakReference wr = (WeakReference)remotes.get(id2);
        if (wr != null) {
            r = (Remote)wr.get();
        }
        if (r == null) {
            r = new Remote();
            r.id = id;
            r.registry = this;
            wr = new WeakReference(r);
            remotes.put(id2, wr);
        }
        return r;
    }

    // safe
    public Remote getRemote (String name) {
        RequestWriter req;
        ReturnReader ret;
        synchronized (New.class) {
            req = New.requestWriter(0, 0, 0);
            req.addArg(name, New.stringWriter());
            ret = New.returnReader();
            ret.addReader(New.i32Reader());
        }
        syncCall(req, ret);

        synchronized (this) {
            Remote r = getRemote(-((Integer)ret.value).intValue());
            r.count++;
            return r;
        }
    }

    // safe
    public void notifyRemoteDestroy (int id, int count) {
        RequestWriter req;
        synchronized (New.class) {
            req = New.requestWriter(1<<6, 0, 1);
            req.addArg(New.i32(id), New.i32Writer());
            req.addArg(New.i32(count), New.i32Writer());
        }
        asyncCall(req, null);
    }

    // unsafe
    public Skeleton getSkeleton (int id) {
        return (Skeleton)skeletons.get(New.i32(id));
    }

    // unsafe
    public Skeleton getSkeleton (Exportable e) {
        Skeleton skel = (Skeleton)skeletonByExportable.get(e);
        if (skel == null) {
            skel = e.createSkeleton();
            skel.id = nextSkeletonId++;
            skeletons.put(new Integer(skel.id), skel);
            skeletonByExportable.put(e, skel);
        }
        return skel;
    }

    private class RegistrySkeleton extends Skeleton {
        public Request createRequest (int h, int mid) {
            synchronized (New.class) {
                Request req = New.request(h, this, mid);
                switch (mid) {
                case 0:
                    req.addReader(New.stringReader());
                    req.ret.addWriter(New.i32Writer());
                    return req;
                case 1:
                    req.addReader(New.i32Reader());
                    req.addReader(New.i32Reader());
                    return req;
                default:
                    req.release();
                    return null;
                }
            }
        }

        public void call (Request req) {
            switch (req.methodIndex) {
            case 0:
                __call_getRemote(req.arguments, req.ret);
                return;
            case 1:
                __call_notifyRemoteDestroy(req.arguments, req.ret);
                return;
            default:
                return;
            }
        }

        private void __call_getRemote (List args, ReturnWriter ret) {
            synchronized (Registry.this) {
                checkClosed();
                Exportable exp = (Exportable)exportables.get(args.get(0));
                if (exp == null) {
                    ret.index = -1;
                    return;
                }
                Skeleton skel = (Skeleton)getSkeleton(exp);
                skel.count++;
                ret.index = 0;
                ret.value = New.i32(skel.id);
            }
        }

        private void __call_notifyRemoteDestroy (List args, ReturnWriter ret) {
            int id = -((Integer)args.get(0)).intValue();
            int count = ((Integer)args.get(1)).intValue();

            synchronized (Registry.this) {
                checkClosed();
                Skeleton skel = getSkeleton(id);
                skel.count -= count;
                if (skel.count == 0) {
                    skeletons.remove(id);
                    skeletonByExportable.remove(skel.object);
                }
            }
        }
    }

    public void checkClosed () {
        if (isClosed) throw new IllegalStateException("closed");
    }
}
