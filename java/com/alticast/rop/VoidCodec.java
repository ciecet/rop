package com.alticast.rop;
import com.alticast.io.*;

public class VoidCodec implements Codec {

    public static final VoidCodec instance = new VoidCodec();

    public void scan (Buffer buf) {
    }

    public Object read (Buffer buf) {
        return null;
    }
    public void write (Object obj, Buffer buf) {
    }
}
