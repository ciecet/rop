package com.alticast.rop;

import java.io.*;
import com.alticast.io.*;

public class SocketTransport extends Transport implements LogConsts {

    private boolean closed = false;
    private SocketReactor reactor;

    public SocketTransport () {
        reactor = new SocketReactor();
    }

    public Reactor getReactor () {
        return reactor;
    }

    public void send (Port p, Buffer buf) {
        try {
            reactor.send (p, buf);
        } catch (IOException e) {
            e.printStackTrace();
            throw new RemoteException("Sending failed", e);
        }
    }

    public void wait (Port p) {
        synchronized (this) {
            if (closed) {
                throw new RemoteException("Transport was closed");
            }
        }
        p.waitRead();
    }

    private class SocketReactor extends BufferedFileReactor {

        protected void readBuffer () throws IOException {
            while (inBuffer.size() >= 4) {
                int msize = inBuffer.readI32();
                if (inBuffer.size() < msize) {
                    inBuffer.preWriteI32(msize);
                    return;
                }

                Buffer sbuf = inBuffer.readBuffer(msize);
                int pid = -sbuf.readI32();
                if (D) Log.debug("socketTransport read " + sbuf.size() +
                        " bytes from port:" + pid);
                synchronized (registry) {
                    Port p = registry.getPort(pid);
                    registry.recv(p, sbuf);
                }
            }
        }

        public synchronized void send (Port p, Buffer buf) throws IOException {
            if (D) Log.debug("socketTransport sending " + buf.size() +
                    " bytes to port:" + p.id);
            outBuffer.writeI32(buf.size() + 4);
            outBuffer.writeI32(p.id);
            outBuffer.writeBuffer(buf);
            buf.clear();
            sendBuffer();
        }

        public void close () {
            synchronized (SocketTransport.this) {
                closed = true;
            }
            if (W) Log.warn("Removing reactor "+this);
            super.close();
            registry.dispose();
        }
    }
}
