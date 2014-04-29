package com.alticast.io;

import java.util.*;
import java.lang.ref.*;
import com.alticast.rop.*;

public class EventDriver implements Runnable, LogConsts {

    public static final int EVENT_READ = 1<<0;
    public static final int EVENT_WRITE = 1<<1;
    public static final int EVENT_FILE = 1<<2; // used for mask only.
    public static final int EVENT_TIMEOUT = 1<<3;
    
    static native long nCreateEventDriver ();
    static native void nDeleteEventDriver (long ned);
    static native void nWatchFile (long ned, int fd, int fmask, long nr,
            Reactor r);
    // return true if became unbound
    static native boolean nUnwatchFile (long ned, long nr);
    static native void nSetTimeout (long ned, long usec, long nr, Reactor r);
    // return true if became unbound
    static native boolean nUnsetTimeout (long ned, long nr);
    static native void nUnbind (long ned, long nr);
    static native void nWait (long ned, long usec);
    static native void nRun (long ned);
    static native boolean nIsEventThread (long ned);

    static native long nCreateReactor (boolean s);
    static native void nDeleteReactor (long nr);
    static native boolean nTryUnrefReactor (long nr);
    static native int nGetMask (long nr);
    static native int nGetFd (long nr);

    private static EventDriver defaultInstance = null;
    public synchronized static EventDriver getDefaultInstance () {
        if (defaultInstance == null) {
            defaultInstance = new EventDriver();
            new Thread(defaultInstance).start();
        }
        return defaultInstance;
    }

    long impl;

    public EventDriver () {
        impl = nCreateEventDriver ();
    }

    protected void finalize () {
        if (impl != 0) {
            nDeleteEventDriver(impl);
        }
    }

    public void watchFile (int fd, int fmask, Reactor r) {
        synchronized (r) {
            nWatchFile(impl, fd, fmask, r.impl, r);
            r.eventDriver = this;
        }
    }

    public void unwatchFile (Reactor r) {
        synchronized (r) {
            if (nUnwatchFile(impl, r.impl)) {
                r.eventDriver = null;
            }
        }
    }

    public void setTimeout (long usec, Reactor r) {
        synchronized (r) {
            nSetTimeout(impl, usec, r.impl, r);
            r.eventDriver = this;
        }
    }

    public void unsetTimeout (Reactor r) {
        synchronized (r) {
            if (nUnsetTimeout(impl, r.impl)) {
                r.eventDriver = null;
            }
        }
    }

    public void unbind (Reactor r) {
        synchronized (r) {
            nUnbind(impl, r.impl);
            r.eventDriver = null;
        }
    }

    public void waitEvent (long usec) {
        nWait(impl, usec);
    }

    public void run () {
        nRun(impl);
    }

    public boolean isEventThread () {
        return nIsEventThread (impl);
    }
}
