package com.alticast.rop;

import com.alticast.io.*;

public class RemoteCall implements Reusable {
    public Buffer buffer = new Buffer();
    public boolean isValid = false;
    public RemoteCall next;
    private Codec[] returnCodecs = null;

    public Buffer init (int h, Remote r, int m) {
        buffer.appData = r.registry;
        buffer.writeI8(h);
        buffer.writeI32(r.id);
        buffer.writeI16(m);
        return buffer;
    }

    public void reuse () {
        buffer.reuse();
        isValid = false;
        returnCodecs = null;
    }

    public void setReturnCodecs (Codec[] codecs) {
        returnCodecs = codecs;
    }

    public void scan () {
        buffer.appData2 = this;
        int osize = buffer.size();
        int idx = buffer.readI8() & 63;
        if (returnCodecs != null && idx < returnCodecs.length &&
                returnCodecs[idx] != null) {
            returnCodecs[idx].scan(buffer);
        }
        buffer.drop(buffer.size() - osize);
    }
}
