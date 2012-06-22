package com.alticast.rop;

public class RemoteCall implements Reusable {
    public Buffer buffer = new Buffer();
    public boolean isValid = false;
    public RemoteCall next;

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
    }
}
