package com.alticast.rop;

import java.util.*;

public class LocalCall implements Reusable {

    public Buffer buffer = new Buffer();
    private int messageType;
    public Skeleton skeleton;
    public LocalCall next;

    // return info
    public Object value;
    public int index = -1;
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
        skeleton = null;
        value = null;
        codec = null;
        index = -1;
    }

    public int getMessageType () {
        return messageType;
    }

    public Buffer makeReturnMessage (Port p) {
        if (buffer.size() > 0) {
            throw new IllegalStateException("message buffer is not empty");
        }
        buffer.writeI8((byte)((Port.MESSAGE_RETURN << 6) | (index & 63)));
        if (codec != null) {
            codec.write(value, buffer);
        }
        return buffer;
    }
}
