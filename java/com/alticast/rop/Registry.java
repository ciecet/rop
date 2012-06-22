package com.alticast.rop;

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
    private Remote peer;

    public final Transport transport;

    // Created by transport.
    public Registry (Transport trans) {
        Skeleton skel = new RegistrySkel();
        skel.id = nextSkeletonId++;
        skeletons.put(New.i32(skel.id), skel);
        transport = trans;
        peer = new Remote();
        peer.id = 0;
        peer.registry = this;
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
            p = (Port)New.get(Port.class);
            p.id = pid;
            p.registry = this;
            ports.put(New.i32(pid), p);
        }
        return p;
    }

    // unsafe
    public void tryReleasePort (Port p) {
        if (p.remoteCalls != null) return;
        if (p.id < 0) {
            if (p.processingThread != null) return;
            if (p.localCalls != null) return;
            if (p.deferredReturn != null) return;
        }
        ports.remove(New.i32(p.id));
        New.release(p);
    }

    // safe
    public void asyncCall (RemoteCall rc, boolean getRet) {
        Port p;
        synchronized (this) {
            checkClosed();
            p = getPort();
            if (getRet) {
                p.addRemoteCall(rc); // add to return-wait queue.
            }
        }

        transport.send(p, rc.buffer);

        synchronized (this) {
            tryReleasePort(p);
        }
    }

    // safe
    public void syncCall (RemoteCall rc) {
        Port p;
        Thread oldProcessingThread;
        synchronized (this) {
            checkClosed();
            p = getPort();
            p.addRemoteCall(rc);
            oldProcessingThread = p.processingThread;
            p.processingThread = Thread.currentThread();
        }

        transport.send(p, rc.buffer);
        
        while (!rc.isValid) {
            transport.wait(p);
            while (p.processRequest());
        }

        synchronized (this) {
            p.processingThread = oldProcessingThread;
            tryReleasePort(p);
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
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, peer, 0);
            StringCodec.instance.write(name, buf);
            syncCall(rc);
            if ((buf.readI8() & 63) > 0) {
                return null;
            }
            synchronized (this) {
                Remote r = getRemote(-buf.readI32());
                r.count++;
                return r;
            }
        } finally {
            New.release(rc);
        }
    }

    // safe
    public void notifyRemoteDestroy (int id, int count) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(1<<6, peer, 1);
            buf.writeI32(id);
            buf.writeI32(count);
            asyncCall(rc, false);
        } finally {
            New.release(rc);
        }
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

    private class RegistrySkel extends Skeleton {

        public void processRequest (LocalCall lc) {
            switch (lc.buffer.readI16()) {
            case 0: __call_getRemote(lc); return;
            case 1: __call_notifyRemoteDestroy(lc); return;
            default: return;
            }
        }

        private void __call_getRemote (LocalCall lc) {
            Buffer buf = lc.buffer;
            synchronized (Registry.this) {
                checkClosed();
                Exportable exp = (Exportable)exportables.get(
                        StringCodec.instance.read(buf));
                if (exp == null) {
                    lc.index = -1;
                    return;
                }
                Skeleton skel = (Skeleton)getSkeleton(exp);
                skel.count++;
                lc.value = New.i32(skel.id);
                lc.codec = I32Codec.instance;
                lc.index = 0;
            }
        }

        private void __call_notifyRemoteDestroy (LocalCall lc) {
            Buffer buf = lc.buffer;
            int id = -buf.readI32();
            int count = buf.readI32();

            Log.debug("no longer used by remote:"+id+" reg:"+this);

            synchronized (Registry.this) {
                checkClosed();
                Skeleton skel = getSkeleton(id);
                Log.debug("skel:"+skel);
                skel.count -= count;
                if (skel.count == 0) {
                    skeletons.remove(New.i32(id));
                    skeletonByExportable.remove(skel.object);
                }
            }
        }
    }

    public void checkClosed () {
        if (isClosed) throw new IllegalStateException("closed");
    }
}
