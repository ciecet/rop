package com.alticast.rop;
import com.alticast.io.*;

public class F32Codec implements Codec {
    public static final F32Codec instance = new F32Codec();

    public void scan (Buffer buf) {
        buf.drop(4);
    }

    public Object read(Buffer buf) {
        return New.f32(buf.readF32());
    }

    public void write(Object obj, Buffer buf) {
        buf.writeF32(((Float) obj).floatValue());
    }

}
