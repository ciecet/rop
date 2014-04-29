package com.alticast.rop;
import com.alticast.io.*;

public class InterfaceCodec implements Codec
{
    private final Class stubClass;

    public InterfaceCodec (Class c) {
        stubClass = c;
    }

    public void scan (Buffer buf) {
        int id = -buf.readI32();
        if (id < 0) return;
        Registry reg = (Registry)buf.appData;
        LocalCall lc = (LocalCall)buf.appData2;
        synchronized (reg) {
            Skeleton skel = reg.getSkeleton(id);
            lc.addObject(skel.object);
        }
    }

    public Object read (Buffer buf) {
        int id = -buf.readI32();
        Registry reg = (Registry)buf.appData;
        synchronized (reg) {
            if (id >= 0) {
                LocalCall lc = (LocalCall)buf.appData2;
                return lc.removeObject();
            } else {
                Remote r = reg.getRemote(id);
                try {
                    Stub s = (Stub)stubClass.newInstance();
                    s.remote = r;
                    r.count++;
                    return s;
                } catch (Exception e) {
                    e.printStackTrace();
                    throw new RemoteException("Cannot instanciate a stub");
                }
            }
        }
    }

    public void write (Object obj, Buffer buf) {
        Registry reg = (Registry)buf.appData;
        synchronized (reg) {
            if (obj instanceof Stub) {
                Stub s = (Stub)obj;
                if (s.remote.registry == reg) {
                    buf.writeI32(s.remote.id);
                    return;
                }
            }

            if (obj instanceof Exportable) {
                Skeleton skel = reg.getSkeleton((Exportable)obj);
                skel.count++;
                buf.writeI32(skel.id);
            } else {
                throw new RemoteException("invalid object:"+obj);
            }
        }
    }
}
