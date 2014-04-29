package com.alticast.rop;

import com.alticast.io.*;

public class Port implements Reusable, Runnable, LogConsts {

    public static final int MESSAGE_SYNC_CALL = 0;
    public static final int MESSAGE_ASYNC_CALL = 1;
    public static final int MESSAGE_CHAINED_CALL = 2;
    public static final int MESSAGE_RETURN = 3;

    int id;
    int otherId; // used for processing async requests
    Registry registry;

    // these fields are monitored within Registry instance.
    public RemoteCall remoteCalls;
    private RemoteCall remoteCallsTail;
    public LocalCall localCalls; // this needs port lock as well.
    private LocalCall localCallsTail;
    public Object deferredReturn;

    public Thread processingThread; // non-null if being processed.

    // guard for read wait.
    private boolean readEvented = false;

    public void reuse () {
        registry = null;
        remoteCalls = remoteCallsTail = null;
        localCalls = localCallsTail = null;
        deferredReturn = null;
        processingThread = null;
        readEvented = false;
    }

    // register remote call object which is waiting for return value.
    public void addRemoteCall (RemoteCall rc) {
        if (remoteCallsTail == null) {
            remoteCalls = remoteCallsTail = rc;
            return;
        }
        remoteCallsTail.next = rc;
        remoteCallsTail = rc;
    }

    // register remote call object which is waiting for return value.
    public void addRemoteCallFront (RemoteCall rc) {
        if (remoteCallsTail == null) {
            remoteCalls = remoteCallsTail = rc;
            return;
        }
        rc.next = remoteCalls;
        remoteCalls = rc;
    }

    public RemoteCall removeRemoteCall () {
        RemoteCall rc = remoteCalls;
        if (rc != null) {
            remoteCalls = rc.next;
            rc.next = null;
            if (rc == remoteCallsTail) {
                remoteCallsTail = null;
            }
        }
        return rc;
    }

    public synchronized void addLocalCall (LocalCall lc) {
        if (localCallsTail == null) {
            localCalls = localCallsTail = lc;
        } else {
            localCallsTail.next = lc;
            localCallsTail = lc;
        }

        if (processingThread != null) {
            notifyRead();
        } else {
            processingThread = new Thread(this);
            processingThread.start();
        }
    }

    public synchronized LocalCall removeLocalCall () {
        LocalCall lc = localCalls;
        if (lc != null) {
            localCalls = lc.next;
            lc.next = null;
            if (lc == localCallsTail) {
                localCallsTail = null;
            }
        }
        return lc;
    }

    void processRequests () {
        LocalCall lc;
        while (true) {
            synchronized (registry) {
                lc = removeLocalCall();
                if (lc == null) return;
            }

            try {
                processRequest(lc);
            } catch (Throwable t) {
                if (W) Log.warn("Got exception from "+lc);
                if (t instanceof Error) {
                    throw (Error)t;
                } else {
                    throw (RuntimeException)t;
                }
            }
        }
    }

    private void processRequest (LocalCall lc) {
        Context ctx = Context.get();
        int opid = ctx.portId;
        try {
            if (lc.getMessageType() == MESSAGE_SYNC_CALL) {
                ctx.portId = id;
            } else {
                if (otherId == 0) {
                    // lazly initialize otherId here.
                    otherId = Context.getNextPortId();
                }
                ctx.portId = otherId;
            }
            ctx.registry = registry;

            if (D) {
                Log.debug("Calling "+lc.skeleton+
                        " from:"+registry.getLocal("app"));
            }
            lc.skeleton.processRequest(lc);
        } catch (Throwable t) {
            t.printStackTrace();
        } finally {
            ctx.portId = opid;
            ctx.registry = null;
        }

        switch (lc.getMessageType()) {
        case MESSAGE_SYNC_CALL:
        case MESSAGE_ASYNC_CALL:
            Buffer buf = null;
            try {
                buf = lc.makeReturnMessage(this);
                if (buf == null) break; // async method
            } catch (Throwable t) {
                // RemoteException may occur on type mismatch.
                t.printStackTrace();

                buf = lc.buffer;
                buf.clear();
                buf.writeI8((byte)((Port.MESSAGE_RETURN << 6) | (-1 & 63)));
            }
            registry.transport.send(this, buf);
            deferredReturn = null;
            break;
        case MESSAGE_CHAINED_CALL:
            deferredReturn = lc.value;
            break;
        }

        New.release(lc);
    }

    public void run () {
        while (true) {
            try {
                processRequests();
                synchronized (this) {
                    if (localCalls == null) {
                        wait(2000);
                    }
                }
            } catch (Throwable t) {
                t.printStackTrace();
            }

            synchronized (registry) {
                synchronized (this) {
                    if (localCalls != null) {
                        continue;
                    }
                }
                processingThread = null;
                registry.tryReleasePort(this);
                return;
            }
        }
    }

    public synchronized void waitRead () {
        while (!readEvented) {
            try {
                wait();
            } catch (InterruptedException e) {}
        }
        readEvented = false;
    }

    public synchronized void notifyRead () {
        notify();
        readEvented = true;
    }
}
