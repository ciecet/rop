package com.alticast.rop;

import java.util.*;
import com.alticast.io.*;

public class LocalCall implements Reusable {

    public Buffer buffer = new Buffer();
    public List objects = new ArrayList();
    public int objectIndex = 0;
    public Skeleton skeleton;
    private int messageType;
    public LocalCall next;

    // return info
    public Object value;
    public int index = -2; // mark async method by default.
    public Codec codec;

    public Buffer init (int h, int oid, Registry reg) {
        messageType = (h >> 6) & 3;
        synchronized (reg) {
            skeleton = reg.getSkeleton(oid);
            if (skeleton == null) {
                throw new RemoteException("No skeleton:"+oid);
            }
        }
        buffer.appData = reg;
        return buffer;
    }

    public void reuse () {
        buffer.reuse();
        objects.clear();
        objectIndex = 0;
        skeleton = null;
        value = null;
        codec = null;
        index = -2;
    }

    public int getMessageType () {
        return messageType;
    }

    public Buffer makeReturnMessage (Port p) {
        if (index == -2) return null;
        if (buffer.size() > 0) {
            throw new IllegalStateException("message buffer is not empty");
        }
        buffer.writeI8((byte)((Port.MESSAGE_RETURN << 6) | (index & 63)));
        if (codec != null) {
            codec.write(value, buffer);
        }
        buffer.dump();
        return buffer;
    }

    public void scan () {
        buffer.appData2 = this;
        int osize = buffer.size();
        skeleton.scan(buffer);

        // recover buffer size
        buffer.drop(buffer.size() - osize);
    }

    public void addObject (Object obj) {
        objects.add(obj);
    }

    public Object removeObject () {
        if (objectIndex >= objects.size()) {
            throw new IllegalStateException("Lack of pre-scanned objects");
        }
        return objects.set(objectIndex++, null);
    }
}
