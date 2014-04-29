package com.alticast.rop;

import java.io.*;
import java.util.*;
import java.lang.ref.*;
import com.alticast.io.*;

public class Registry implements LogConsts {

    private static final Map exportables = new HashMap();

    // unsafe
    public static void export (String name, Exportable e) {
        exportables.put(name, e);
    }

    private int nextSkeletonId = 0;
    private final Map remotes = new HashMap();
    private final Map skeletons = new HashMap();
    private final Map skeletonByExportable = new IdentityHashMap();
    public final Map ports = new HashMap();
    private final Map localData = new IdentityHashMap();
    private final Set disposeListeners = new HashSet();
    private boolean disposed = false;
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
    public Port getPort () {
        int pid;
        Context ctx = Context.get();
        if (ctx.registry == this && ctx.portId != 0) {
            pid = ctx.portId;
        } else {
            pid = ctx.originalPortId;
        }
        return getPort(pid);
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
            if (p.deferredReturn != null) return;
            synchronized (p) {
                if (p.localCalls != null) return;
            }
        }
        ports.remove(New.i32(p.id));
        New.release(p);
    }

    // safe
    public void asyncCall (RemoteCall rc, boolean getRet) {

        Port p;
        synchronized (this) {
            checkDisposed();
            p = getPort();
            if (getRet) {
                p.addRemoteCall(rc); // add to return-wait queue.
            }
        }

        try {
            transport.send(p, rc.buffer);
        } finally {
            synchronized (this) {
                tryReleasePort(p);
            }
        }
    }

    // safe
    public void syncCall (RemoteCall rc) {

        Port p;
        Thread oldProcessingThread;

        synchronized (this) {
            checkDisposed();
            p = getPort();
            p.addRemoteCallFront(rc);
            oldProcessingThread = p.processingThread;
            p.processingThread = Thread.currentThread();
        }

        try {
            transport.send(p, rc.buffer);

            while (!rc.isValid) {
                transport.wait(p);
                p.processRequests();
            }
        } finally {
            synchronized (this) {
                p.processingThread = oldProcessingThread;
                tryReleasePort(p);
            }
        }
    }

    // safe
    public synchronized void dispose () {
        if (disposed) return;
        remotes.clear();
        skeletons.clear();
        skeletonByExportable.clear();
        disposed = true;

        Object[] lnrs = disposeListeners.toArray();
        for (int i = 0; i < lnrs.length; i++) {
            RegistryListener d = (RegistryListener)lnrs[i];
            d.registryDisposed(this);
        }
        disposeListeners.clear();

        // wake-up waiting threads on this connection
        for (Iterator i = ports.values().iterator(); i.hasNext();) {
            Port p = (Port)i.next();
            p.notifyRead();
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

        public void scan (Buffer buf) {
            switch (buf.readI16()) {
            default: return;
            case 0:
                StringCodec.instance.scan(buf);
                break;
            case 1:
                buf.drop(8); // i32 * 2
                break;
            }
        }

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
                checkDisposed();

                String arg0 = (String)StringCodec.instance.read(buf);
                Exportable exp = (Exportable)exportables.get(arg0);
                if (exp == null) {
                    if (W) Log.warn("getRemote(\""+arg0+"\") failed.");
                    lc.index = -1;
                    return;
                }
                if (I) Log.info("getRemote(\""+arg0+"\") returns "+exp);
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

            if (I) Log.info("no longer used by remote:"+id+" reg:"+this);

            synchronized (Registry.this) {
                checkDisposed();
                Skeleton skel = getSkeleton(id);
                if (D) Log.debug("skel:"+skel);
                skel.count -= count;
                if (skel.count == 0) {
                    skeletons.remove(New.i32(id));
                    skeletonByExportable.remove(skel.object);
                }
            }
        }
    }

    public void checkDisposed () {
        if (disposed) throw new RemoteException("closed");
    }

    // This is supposed to be single point for receiving message from
    // transport. However, HttpTransport has its own impl due to
    // the exceptional behavior.
    public void recv (Port p, Buffer buf) {

        int h = buf.readI8() & 0xff;
        if (I) Log.info("reading from port:"+p.id+" head:"+h);

        if ((h >> 6) == Port.MESSAGE_RETURN) {
            RemoteCall rc = p.removeRemoteCall();
            Buffer buf2 = rc.buffer;
            buf2.writeI8(h);
            buf2.writeBuffer(buf);
            buf.drop(buf.size());
            rc.scan();
            rc.isValid = true;
            p.notifyRead();
        } else {
            LocalCall lc = (LocalCall)New.get(LocalCall.class);
            if (buf.size() < 6) {
                throw new RemoteException("Too short header. size:"+buf.size());
            }
            int oid = -buf.readI32();
            Buffer buf2 = lc.init(h, oid, this);
            buf2.writeBuffer(buf);
            buf.drop(buf.size());
            lc.scan();
            p.addLocalCall(lc);
        }
    }

    public synchronized Object getLocal (Object var) {
        return localData.get(var);
    }

    public synchronized void setLocal (Object var, Object value) {
        localData.put(var, value);
    }

    public synchronized void addRegistryListener (RegistryListener dl) {
        if (disposed) {
            dl.registryDisposed(this);
            return;
        }
        disposeListeners.add(dl);
    }

    public synchronized void removeRegistryListener (RegistryListener dl) {
        disposeListeners.remove(dl);
    }
}
