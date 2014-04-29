package com.alticast.rop;

import java.util.*;
import java.io.*;
import com.alticast.io.*;

public class StreamTransport extends Transport implements Runnable, LogConsts {
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
        if (D) Log.debug("Port:"+p.id+" >>>");
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
                int pid = -is.readInt();
                Port p = null;
                if (D) Log.debug(">>> Port:"+pid);
                int h = is.readByte() & 0xff;

                if (((h>>6) & 3) == Port.MESSAGE_RETURN) {
                    RemoteCall rc = null;
                    synchronized (registry) {
                        p = registry.getPort(pid);
                        rc = p.removeRemoteCall();
                    }
                    Buffer buf = rc.buffer;
                    buf.writeI8(h);
                    buf.readFrom(is, len - 5);
                    rc.scan();
                    rc.isValid = true;
                    p.notifyRead();
                    // TODO: async call with return value may cause port leak.
                } else {
                    LocalCall lc = (LocalCall)New.get(LocalCall.class);
                    int oid = -is.readInt();
                    Buffer buf = lc.init(h, oid, registry);
                    buf.readFrom(is, len - 9);
                    lc.scan();
                    synchronized (registry) {
                        p = registry.getPort(pid);
                        p.addLocalCall(lc);
                    }
                }
            }
        } catch (Throwable t) {
            t.printStackTrace();
            registry.dispose();
        }
    }

    public void close () {
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
