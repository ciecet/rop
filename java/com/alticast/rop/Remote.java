package com.alticast.rop;

public class Remote {
    int id; // negative
    int count;
    public Registry registry;

    protected void finalize () {
        registry.notifyRemoteDestroy(id, count);
    }
};
