package com.alticast.rop;
import com.alticast.io.*;

public class I64Codec implements Codec
{
    public static final I64Codec instance = new I64Codec();

    public void scan (Buffer buf) {
        buf.drop(8);
    }

    public Object read (Buffer buf) {
        return New.i64(buf.readI64());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeI64(((Long)obj).longValue());
    }
}
