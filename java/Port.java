public class Port {

    int id;
    Registry registry;
    int useCount;

    ReturnReader returns, returnsTail;
    Request requests, requestsTail, lastRequest;

    Writer writer;
    Reader reader;
    Cont writeCont;
    Cont readCont;

    Thread processingThread;

    // waiting line for sending.
    Port next;

    public Port (Registry reg) {
        registry = reg;
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

    boolean isActive () {
        return true; // TODO: implement
    }
}
