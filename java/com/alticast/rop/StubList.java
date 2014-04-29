package com.alticast.rop;
import java.util.*;

/**
 * Collection of arbitrary objects where some of them are rop stub instances.
 * Stub instances in this list is automatically cleared if its bound rop
 * conenction is disconnected.
 * Intended for managing listeners whose type is defined in rop idl.
 */
public class StubList implements StubListener {

    private List stubs = new ArrayList();

    public synchronized void add (Object o) {
        stubs.add(o);
        if (o instanceof Stub) {
            Stub s = (Stub)o;
            s.addStubListener(this);
        }
    }

    public synchronized void remove (Object o) {
        int fi = stubs.indexOf(o); // found index
        if (fi >= 0) {
            o = stubs.remove(fi);
            if (o instanceof Stub) {
                Stub s = (Stub)o;
                s.removeStubListener(this);
            }
            return;
        }

        if (!(o instanceof Stub)) return;
        Remote r = ((Stub)o).remote;

        // find other stub which has the same remote object.
        for (int i = stubs.size() - 1; i >= 0; i--) {
            Object o2 = stubs.get(i);
            if (!(o2 instanceof Stub)) continue;
            Stub s2 = (Stub)o2;
            if (s2.remote != r) continue;
            stubs.remove(i);
            s2.removeStubListener(this);
            return;
        }
    }

    public int size () {
        return stubs.size();
    }

    public void stubRevoked (Stub s) {
        remove(s);
    }

    public synchronized Object[] toArray () {
        return stubs.toArray();
    }
}

