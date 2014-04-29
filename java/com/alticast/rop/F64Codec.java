package com.alticast.rop;
import com.alticast.io.*;

public class F64Codec implements Codec {
    public static final F64Codec instance = new F64Codec();

    public void scan (Buffer buf) {
        buf.drop(8);
    }

    public Object read (Buffer buf) {
        return New.f64(buf.readF64());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeF64(((Double)obj).doubleValue());
    }

}
