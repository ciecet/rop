import java.io.*;
import java.util.*;

public class Channel extends Skeleton implements Exportable, Runnable {

    public static final int CALL = 0 << 6;
    public static final int CALL_WO_RETURN = 1 << 6;
    public static final int RET = 2 << 6;
    public static final int CONTROL = 3 << 6;
    public static final int THREAD_MOD = 1 << 5;
    public static final int POSITION_MASK = 31;

    private DataInputStream in;
    private DataOutputStream out;

    private volatile boolean active = true;
    private Object lock = new Object();
    private volatile Thread writingThread = null;
    private volatile int lockDepth = 0;
    private Map remoteReturns = new HashMap();
    private Map codecs = new IdentityHashMap();
    private Map roots = new HashMap();
    private Map skeletonByExportable = new IdentityHashMap();

    // TODO: shrinking
    private ArrayList skeletons = new ArrayList();
    // TODO: shrinking
    private Map remoteObjects = new HashMap();

    public Channel (InputStream i, OutputStream o) {
        in = new DataInputStream(new BufferedInputStream(i));
        out = new DataOutputStream(new BufferedOutputStream(o));

        // register base codecs
        putCodec("String", new StringCodec());
        putCodec("Integer", new IntegerCodec());
        putCodec("RemoteIn", new RemoteInCodec());
        putCodec("List<String>", new ListCodec(getCodec("String")));
        putCodec("RemoteOut", new RemoteOutCodec());

        // Register index-0 writeStruct self
        getSkeleton(this); // .index must be 0

        // Start pumping thread
        new Thread(this).start();
    }

    public Codec getCodec (String name) {
        return (Codec)codecs.get(name);
    }

    public void putCodec (String name, Codec c) {
        codecs.put(name, c);
    }

    public void registerObject (String name, Object obj) {
        roots.put(name, obj);
    }

    public Skeleton getSkeleton (Exportable obj) {
        Skeleton skel = (Skeleton)skeletonByExportable.get(obj);
        if (skel == null) {
            skel = obj.getSkeleton();
            skel.index = skeletons.size();
            skeletons.add(skel);
            skeletonByExportable.put(obj, skel);
        }
        return skel;
    }

    public RemoteObject getRemoteObject (String name) {
        lock();
        writeI8(CALL);
        RemoteReturn ret = writeRemoteReturn(getReturnTypes(0));
        writeI16(0); // object id for this
        writeI8(0); // method index for this
        getCodec("String").write(this, name);
        unlock();

        ret.waitForReturn();
        System.out.println("getRemoteObject("+name+") returns "+ret.returnType+" "+ret.returnValue);
        if (ret.returnType == 0) {
            return (RemoteObject)ret.returnValue;
        }
        abort(null);
        return null;
    }

    public RemoteObject getRemoteObject (int idx) {
        Integer i = new Integer(idx);
        RemoteObject ro = (RemoteObject)remoteObjects.get(i);
        if (ro != null) {
            return ro;
        }
        ro = new RemoteObject(this, idx);
        remoteObjects.put(i, ro);
        return ro;
    }

    private RequestQueue defaultRequestQueue = new RequestQueue(this);

    private void handleMessage () throws IOException {
        int h = readI8();

        switch (h >> 6) {
        case CALL>>6:
            System.out.println("Got a call");
            CallRequest r = new CallRequest();
            RequestQueue rq = null;
            r.chainIndex = h & POSITION_MASK;
            if ((h & THREAD_MOD) == 0) {
                rq = defaultRequestQueue;
            } else {
                throw new UnsupportedOperationException("yet");
            }
            r.returnId = readI32();
            r.objectId = readI16();
            r.methodIndex = readI8();
            System.out.println("got request retid:"+r.returnId+" objid:"+r.objectId+" mid:"+r.methodIndex);
            r.skeleton = (Skeleton)skeletons.get(r.objectId);
            System.out.println("sksleton:"+r.skeleton);
            String[] argTypes = r.skeleton.getArgumentTypes(r.methodIndex);
            r.args = new Object[argTypes.length];
            for (int i = 0; i < argTypes.length; i++) {
                r.args[i] = getCodec(argTypes[i]).read(this);
                System.out.println("args["+i+"]= ("+argTypes[i]+")"+r.args[i]);
            }
            rq.enqueue(r);
            break;

        case RET>>6:
            System.out.println("Got a ret");
            Integer k = new Integer(readI32());
            RemoteReturn ret = (RemoteReturn)remoteReturns.remove(k);
            if (ret == null) {
                abort(null);
                return;
            }

            synchronized (ret) {
                ret.returnType = h & POSITION_MASK;
                if (ret.returnType == POSITION_MASK) {
                    ret.returnType = -1;
                } else {
                    ret.returnValue = getCodec(ret.returnTypes[ret.returnType]).
                            read(this);
                }
                ret.notify();
            }
            break;

        case CALL_WO_RETURN>>6:
        case CONTROL>>6:
        default:
            throw new IllegalStateException(
                    "Unsupported message type:"+h);
        }
    }

    public void run () {
        try {
            while (active) {
                handleMessage();
            }
        } catch (Throwable t) {
            abort(t);
        } finally {
            // clearn up
        }
    }

    public void close () {
        if (!active) {
            return;
        }
        active = false;
        try {
            in.close();
        } catch (IOException e) {
        }
        try {
            out.close();
        } catch (IOException e) {
        }
    }

    public void abort (Throwable cause) {
        close();
        if (cause != null) {
            cause.printStackTrace();
        }
        throw new ChannelException("aborted.");
    }

    public void checkActive () {
        if (!active) {
            throw new ChannelException("inactive");
        }
    }

    public void lock () {
        synchronized (lock) {
            Thread ct = Thread.currentThread();
            if (ct == writingThread) {
                lockDepth++;
                return;
            }
            while (writingThread != null) {
                try {
                    lock.wait();
                } catch (InterruptedException e) {
                }
            }
            checkActive();
            writingThread = ct;
            lockDepth = 1;
        }
    }

    public void unlock () {
        synchronized (lock) {
            checkActive();
            lockDepth--;
            if (lockDepth > 0) {
                return;
            }
            if (lockDepth < 0) {
                throw new IllegalStateException("unbalanced unlock");
            }
            try {
                out.flush();
            } catch (IOException e) {
                abort(e);
            }
            lock.notify();
            writingThread = null;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Self-Skeleton

    private static final String[][][] types = new String[][][] {
        { {"String"}, {"RemoteOut"}}
    };
    public Skeleton getSkeleton () {
        return this;
    }
    public String[] getArgumentTypes (int index) {
        return types[index][0];
        
    }
    public String[] getReturnTypes (int index) {
        return types[index][1];
    }
    public void handleRequest (CallRequest req) {
        switch (req.methodIndex) {
        case 0: 
            try {
                req.returnValue = roots.get(req.args[0]);
                req.returnType = 0;
            } catch (Throwable t) {
                req.returnValue = t;
                req.returnType = -1;
            }
            break;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    // Read / Write

    public Skeleton readRemoteIn () {
        try {
            return (Skeleton)skeletons.get(readI32());
        } catch (Throwable t) {
            abort(t);
            return null;
        }
    }

    public int readI8 () {
        try {
            return in.readUnsignedByte();
        } catch (Throwable t) {
            abort(t);
            return 0;
        }
    }

    public void writeI8 (int i) {
        try {
            out.write(i);
        } catch (Throwable t) {
            abort(t);
        }
    }

    public int readI16 () {
        try {
            return in.readUnsignedShort();
        } catch (Throwable t) {
            abort(t);
            return 0;
        }
    }

    public void writeI16 (int i) {
        try {
            out.writeShort(i);
        } catch (Throwable t) {
            abort(t);
        }
    }

    public int readI32 () {
        try {
            return in.readInt();
        } catch (Throwable t) {
            abort(t);
            return 0;
        }
    }

    public void writeI32 (int i) {
        try {
            out.writeInt(i);
        } catch (Throwable t) {
            abort(t);
        }
    }

    public long readI64 () {
        try {
            return in.readLong();
        } catch (Throwable t) {
            abort(t);
            return 0l;
        }
    }

    public void writeI64 (long i) {
        try {
            out.writeLong(i);
        } catch (Throwable t) {
            abort(t);
        }
    }

    public void readBytes (byte[] a, int off, int len) {
        try {
            in.readFully(a, off, len);
        } catch (Throwable t) {
            abort(t);
        }
    }

    public void writeBytes (byte[] a, int off, int len) {
        try {
            out.write(a, off, len);
        } catch (Throwable t) {
            abort(t);
        }
    }

    public void writeRemoteOut (Exportable obj) {
        writeI32(getSkeleton(obj).index);
    }

    private static int nextReturnId = 0;
    public RemoteReturn writeRemoteReturn (String[] types) {
        RemoteReturn ret = new RemoteReturn();
        ret.id = nextReturnId++;
        ret.returnTypes = types;
        remoteReturns.put(new Integer(ret.id), ret);
        writeI32(ret.id);
        return ret;
    }
}
