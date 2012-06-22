package com.alticast.rop;

public class Port implements Reusable, Runnable {

    public static final int MESSAGE_SYNC_CALL = 0;
    public static final int MESSAGE_ASYNC_CALL = 1;
    public static final int MESSAGE_CHAINED_CALL = 2;
    public static final int MESSAGE_RETURN = 3;

    int id;
    Registry registry;

    public RemoteCall remoteCalls;
    public LocalCall localCalls;
    public Object deferredReturn;

    private RemoteCall remoteCallsTail;
    private LocalCall localCallsTail;

    public Thread processingThread; // non-null if being processed.

    public void reuse () {
        registry = null;
        remoteCalls = remoteCallsTail = null;
        localCalls = localCallsTail = null;
        deferredReturn = null;
        processingThread = null;
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

    public void addLocalCall (LocalCall lc) {
        if (localCallsTail == null) {
            localCalls = localCallsTail = lc;
            return;
        }
        localCallsTail.next = lc;
        localCallsTail = lc;
    }

    public LocalCall removeLocalCall () {
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

    boolean processRequest () {
        LocalCall lc = null;
        synchronized (registry) {
            lc = removeLocalCall();
            if (lc == null) return false;
        }
        
        int[] tids = (int[])registry.getThreadIds();
        int otid = tids[1];
        tids[1] = (lc.getMessageType() == MESSAGE_SYNC_CALL) ? id : tids[0];
        lc.skeleton.processRequest(lc);
        tids[1] = otid;

        switch (lc.getMessageType()) {
        case MESSAGE_SYNC_CALL:
            registry.transport.send(this, lc.makeReturnMessage(this));
            deferredReturn = null;
            break;
        case MESSAGE_CHAINED_CALL:
            deferredReturn = lc.value;
            break;
        }

        New.release(lc);
        return true;
    }

    public void run () {
        while (true) {
            while (processRequest());

            synchronized (this) {
                try {
                    wait(2000);
                } catch (InterruptedException e) {}
            }

            synchronized (registry) {
                if (localCalls == null) {
                    processingThread = null;
                    registry.tryReleasePort(this);
                    return;
                }
            }
        }
    }
}
