package com.alticast.rop;

import com.alticast.io.*;

public abstract class Skeleton {
    int count;
    public int id;
    public Object object;

    public abstract void processRequest (LocalCall lc);
    public abstract void scan (Buffer buf);
}
