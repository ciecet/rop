public class Port implements Reusable {

    int id;
    Registry registry;
    int useCount;

    ReturnReader returns, returnsTail;
    Request requests, requestsTail, lastRequest;

    Writer writer;
    Reader reader;
    Cont writeCont;
    Cont readCont;

    Thread processingThread; // non-null if processes are to be processed.

    // waiting line for sending.
    Port next;

    public Port () {
        //registry = reg;
    }

    public void addReturn (ReturnReader ret) {
        if (returnsTail == null) {
            returns = returnsTail = ret;
            return;
        }
        returnsTail.next = ret;
        returnsTail = ret;
    }

    public ReturnReader popReturn () {
        ReturnReader ret = returns;
        if (ret != null) {
            returns = ret.next;
            ret.next = null;
            if (ret == returnsTail) {
                returnsTail = null;
            }
        }
        return ret;
    }

    public void addRequest (Request req) {
        if (requestsTail == null) {
            requests = requestsTail = req;
            return;
        }
        requestsTail.next = req;
        requestsTail = req;
    }

    public Request popRequest () {
        Request req = requests;
        if (req != null) {
            requests = req.next;
            req.next = null;
            if (req == requestsTail) {
                requestsTail = null;
            }
        }
        return req;
    }

    boolean processRequest () {
        Request req = null;
        Request lreq = null;
        synchronized (registry) {
            req = popRequest();
            if (req == null) return false;
        }
        lreq = lastRequest;
        lastRequest = null;
        
        int[] tids = (int[])registry.getThreadIds();
        int otid = tids[1];
        tids[1] = ((req.messageHead & (0x1<<6)) > 0) ? tids[0]: id;
        req.call();
        tids[1] = otid;

        if (lastRequest != null) {
            synchronized (New.class) {
                lastRequest.release();
            }
        }
        lastRequest = req;
        if (req.ret.writers.size() > 0) {
            writer = req.ret;
            registry.transport.send(this);
        }
        return true;
    }

    // should be called under registry lock.
    public void ref () {
        useCount++;
    }

    // should be called under registry lock.
    public void deref () {
        useCount--;
        tryRelease();
    }

    public void tryRelease () {
        if (useCount > 0 || processingThread != null ||
                lastRequest != null) {
            return;
        }
        synchronized (registry) {
            synchronized (New.class) {
                registry.ports.remove(New.i32(id));
                New.releaseReusable(this);
            }
        }
    }

    public void release () {
        // not used directly.
    }
}
