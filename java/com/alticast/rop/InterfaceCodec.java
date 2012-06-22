package com.alticast.rop;

public class InterfaceCodec implements Codec
{
    private final Class stubClass;

    public InterfaceCodec (Class c) {
        stubClass = c;
    }

    public Object read (Buffer buf) {
        int id = -buf.readI32();
        Registry reg = (Registry)buf.appData;
        synchronized (reg) {
            if (id >= 0) {
                Skeleton skel = reg.getSkeleton(id);
                return skel.object;
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
            if (obj instanceof Exportable) {
                Skeleton skel = reg.getSkeleton((Exportable)obj);
                skel.count++;
                buf.writeI32(skel.id);
            } else if (obj instanceof Stub) {
                buf.writeI32(((Stub)obj).remote.id);
            } else {
                throw new IllegalStateException("invalid object");
            }
        }
    }
}
