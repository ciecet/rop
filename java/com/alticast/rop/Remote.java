package com.alticast.rop;

public class Remote {
    int count;
    public int id; // negative
    public Registry registry;

    protected void finalize () {
        registry.notifyRemoteDestroy(id, count);
    }
};
