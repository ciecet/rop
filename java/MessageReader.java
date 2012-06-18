public class MessageReader extends Reader {
    int head;
    Request request;
    Port port;

    public Cont read (Object ret, IOContext ctx) {
        switch (step) {
        case 0: // read header
            if (ctx.buffer.size() < 1) return ctx.blocked(this);
            head = ctx.buffer.readI8() & 0xff;
            if ((head >> 6) == 3) {
                step = 1;
            } else {
                step = 3;
            }
            return read(null, ctx);
        case 1: // read return
            ReturnReader rr = port.popReturn();
            if (rr == null) {
                throw new IllegalStateException("No waiting return");
            }
            step++;
            return rr.start(head, this);
        case 2: // got return
            Log.debug("got a return. idx:"+((ReturnReader)ret).index);
            synchronized (port) {
                port.notify();
            }
            synchronized (New.class) {
                Cont c = cont;
                release();
                return c;
            }
        case 3: // read request
            if (ctx.buffer.size() < 6) return ctx.blocked(this);
            {
                int oid = -ctx.buffer.readI32();
                int mid = ctx.buffer.readI16();
                request = port.registry.getSkeleton(oid).createRequest(head,
                        mid);
            }
            step++;
            return request.start(this);
        case 4: // got request
            Log.debug("got a request. obj:" + request.skeleton.object +
                    " mid:" + request.methodIndex+" req:"+request);
            port.addRequest((Request)request);
            port.registry.transport.notifyUnhandledRequest(port);
            synchronized (New.class) {
                Cont c = cont;
                release();
                return c;
            }
        default:
            throw new IllegalStateException("Unsupported message type:"+head);
        }
    }

    public void release () {
        request = null;
        port = null;
        cont = null;
        New.releaseReusable(this);
    }
}
