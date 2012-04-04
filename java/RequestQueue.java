public class RequestQueue extends Thread {

    private Channel channel;
    private CallRequest requests;
    private boolean disposed = false;

    public RequestQueue (Channel chn) {
        channel = chn;
        start();
    }

    public synchronized void enqueue (CallRequest req) {
        if (requests == null) {
            notify();
        }
        req.previous = requests;
        requests = req;
    }

    public synchronized void dispose () {
        disposed = true;
        notify();
    }

    public void run () {
        while (true) {
            CallRequest req = null;
            synchronized (this) {
                if (disposed) {
                    break;
                }
                if (requests == null) {
                    try {
                        wait();
                    } catch (InterruptedException e) {
                    }
                }
                req = requests;
                requests = req.previous;
            }

            System.out.println("Invoking method");
            try {
                req.skeleton.handleRequest(req);
            } catch (Throwable t) {
                t.printStackTrace();
            }

            System.out.println("Sending ret "+req.returnValue);
            channel.lock();
            channel.writeI8(Channel.RET | (req.returnType & Channel.POSITION_MASK));
            channel.writeI32(req.returnId);
            if (req.returnType != -1) {
                channel.getCodec(req.skeleton.getReturnTypes(req.methodIndex)[req.returnType]).write(channel, req.returnValue);
            }
            channel.unlock();
        }
    }
}
