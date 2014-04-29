package com.alticast.rop;

import com.alticast.io.*;

public abstract class Transport {

    public final Registry registry;

    public Transport () {
        registry = new Registry(this);
    }

    public abstract void send (Port p, Buffer buf);
    public abstract void wait (Port p);
    //public abstract void close ();
}
