package com.alticast.rop;
import com.alticast.io.*;

public class BoolCodec implements Codec {
    public static final BoolCodec instance = new BoolCodec();

    public void scan (Buffer buf) {
        buf.drop(1);
    }
    
    public Object read (Buffer buf) {
        return New.bool(buf.readBool());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeBool(((Boolean)obj).booleanValue());
    }
}
