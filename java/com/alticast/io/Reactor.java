package com.alticast.io;

import java.util.*;
import java.lang.ref.*;
import com.alticast.rop.*;

public abstract class Reactor implements LogConsts {

    private static final long REACTOR_COLLECT_PERIOD = 5 * 1000000;
    private static ReferenceQueue reactorQueue = new ReferenceQueue();
    private static Map reactorImpls = new IdentityHashMap();
    private static Reactor reactorCollector = new Reactor (true) {
        public void processEvent (int ev) {
            while (true) {
                Reference ref = reactorQueue.poll();
                if (ref == null) break;
                long nr;
                synchronized (reactorImpls) {
                    nr = ((Long)reactorImpls.remove(ref)).longValue();
                }
                EventDriver.nDeleteReactor(nr);
                if (I) Log.info("Collected reactor:"+Long.toHexString(
                        nr & 0xffffffffl));
            }
            EventDriver.getDefaultInstance().setTimeout(
                    REACTOR_COLLECT_PERIOD, this);
        }
    };

    static {
        EventDriver.getDefaultInstance().setTimeout(
                REACTOR_COLLECT_PERIOD, reactorCollector);
    }

    /**
     * Dispose corresponding native instance after given reactor is gc-ed.
     */
    private static void deleteOnGc (Reactor r) {
        Reference ref = new PhantomReference(r, reactorQueue);
        synchronized (reactorImpls) {
            reactorImpls.put(ref, new Long(r.impl));
        }
    }

    EventDriver eventDriver;
    long impl;

    public Reactor (boolean s) {
        impl = EventDriver.nCreateReactor(s);
        deleteOnGc(this);
    }

    public int getMask () { return EventDriver.nGetMask(impl); }
    public int getFd () { return EventDriver.nGetFd(impl); }
    public synchronized EventDriver getEventDriver () { return eventDriver; };

    public abstract void processEvent (int events);

    // JNI callback
    private void guardedProcessEvent (int ev) {

        synchronized (this) {
            if (EventDriver.nTryUnrefReactor(impl)) {
                eventDriver = null;
            }
        }

        try {
            if (D) {
                String str = "process event: ";
                if ((ev & EventDriver.EVENT_READ) != 0) {
                    str += "read ";
                }
                if ((ev & EventDriver.EVENT_WRITE) != 0) {
                    str += "write ";
                }
                if ((ev & EventDriver.EVENT_TIMEOUT) != 0) {
                    str += "timeout ";
                }
                Log.debug(str);
            }
            processEvent(ev);
        } catch (Throwable t) {
            if (E) {
                Log.error("Uncaught exception occurred.");
                t.printStackTrace();
            }
        }
    }
}
