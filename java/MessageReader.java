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
            System.out.println("reading head... head:"+head+" next step:"+step);
            return read(null, ctx);
        case 1: // return
            System.out.println("reading return...");
            ReturnReader rr = port.popReturn();
            if (rr == null) {
                throw new IllegalStateException("No waiting return");
            }
            step++;
            return rr.start(head, this);
        case 2: // post return
            System.out.println("post return...");
            synchronized (port) {
                port.notify();
            }
            synchronized (port.registry) {
                port.registry.releasePort(port);
            }
            synchronized (New.class) {
                Cont c = cont;
                release();
                return c;
            }
        case 3: // read call request
            if (ctx.buffer.size() < 6) return ctx.blocked(this);
            {
                int oid = -ctx.buffer.readI32();
                int mid = ctx.buffer.readI16();
                request = port.registry.getSkeleton(oid).createRequest(head,
                        mid);
                System.out.println("request for oid:"+oid+" mid:"+mid+" obj:"+request);
            }
            step++;
            return request.start(this);
        case 4: // read returned
            port.addRequest((Request)request);
            synchronized (port) {
                if (port.processingThread != null) {
                    port.notify();
                } else {
                    port.registry.transport.notifyUnhandledRequest(port);
                }
            }
            synchronized (port.registry) {
                port.registry.releasePort(port);
            }
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
