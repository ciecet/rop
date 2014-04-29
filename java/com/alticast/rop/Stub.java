package com.alticast.rop;
import java.util.*;

public class Stub implements RegistryListener, Exportable {
    public Remote remote;
    public List listeners;

    public Stub () {}

    /**
     * Create skeleton object for this stub.
     * Stub object can be exported to other rop connection.
     * Using reflection to load skel class in lazy manner.
     */
    public Skeleton createSkeleton () {
        try {
            String clsName = getClass().getName();
            clsName = clsName.substring(0, clsName.length() - 4) + "Skel";
            Class skelCls = Class.forName(clsName);
            Skeleton skel = (Skeleton)skelCls.newInstance();
            skel.object = this;
            return skel;
        } catch (Throwable t) {
            t.printStackTrace();
            throw new RemoteException("Cannot export stub object");
        }
    }

    public void addStubListener (StubListener l) {
        if (listeners == null) {
            listeners = new ArrayList();
            remote.registry.addRegistryListener(this);
        }
        listeners.add(l);
    }

    public void removeStubListener (StubListener l) {
        listeners.remove(l);
        if (listeners.size() == 0) {
            remote.registry.removeRegistryListener(this);
            listeners = null;
        }
    }

    public void registryDisposed (Registry reg) {
        Object[] ls = listeners.toArray();
        for (int i = 0; i < ls.length; i++) {
            ((StubListener)ls[i]).stubRevoked(this);
        }
    }
}
