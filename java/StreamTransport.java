import java.util.*;
import java.io.*;

public class StreamTransport extends Transport implements Runnable {
    private InputStream inputStream;
    private OutputStream outputStream;
    private Object writeLock = new Object();
    private Thread readThread;

    public StreamTransport (InputStream is, OutputStream os) {
        super();
        inputStream = is;
        outputStream = os;
        readThread = new Thread(this);
        readThread.start();
    }

    public void send (Port p, Buffer buf) {
        Log.debug("Port:"+p.id+" >>>");
        try {
            buf.preWriteI32(p.id);
            buf.preWriteI32(buf.size());
            synchronized (writeLock) {
                buf.writeTo(outputStream);
            }
        } catch (Throwable t) {
            t.printStackTrace();
            registry.dispose();
        }
    }

    public void wait (Port p) {
        synchronized (p) {
            try {
                p.wait();
            } catch (InterruptedException e) {}
        }
    }

    // reading thread
    public void run () {
        try {
            int r, n;
            DataInputStream is = new DataInputStream(inputStream);
            while (true) {
                int len = is.readInt();
                Port p = registry.getPort(-is.readInt());
                Log.debug(">>> Port:"+p.id);
                int h = is.readByte() & 0xff;

                if (((h>>6) & 3) == Port.MESSAGE_RETURN) {
                    RemoteCall rc = null;
                    synchronized (registry) {
                        rc = p.removeRemoteCall();
                    }
                    Buffer buf = rc.buffer;
                    buf.writeI8(h);
                    buf.readFrom(is, len - 5);
                    rc.isValid = true;
                    synchronized (p) {
                        p.notify();
                    }
                    // TODO: async call with return value may cause port leak.
                } else {
                    LocalCall lc = (LocalCall)New.get(LocalCall.class);
                    int oid = -is.readInt();
                    Buffer buf = lc.init(h, oid, registry);
                    buf.readFrom(is, len - 9);
                    synchronized (registry) {
                        p.addLocalCall(lc);
                        if (p.processingThread != null) {
                            synchronized (p) {
                                p.notify();
                            }
                        } else {
                            p.processingThread = new Thread(p);
                            p.processingThread.start();
                        }
                    }
                }
            }
        } catch (Throwable t) {
            t.printStackTrace();
            registry.dispose();
        }
    }

    public void dispose () {
        try {
            outputStream.close();
            inputStream.close();
            registry.dispose();
            readThread.interrupt();
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
