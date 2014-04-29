package com.alticast.rop;
import com.alticast.io.*;

public class I32Codec implements Codec
{
    public static final I32Codec instance = new I32Codec();

    public void scan (Buffer buf) {
        buf.drop(4);
    }

    public Object read (Buffer buf) {
        return New.i32(buf.readI32());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeI32(((Integer)obj).intValue());
    }
}
