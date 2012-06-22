package com.alticast.rop;

public abstract class Skeleton {
    int id;
    int count;
    public Object object;

    public abstract void processRequest (LocalCall lc);
}
