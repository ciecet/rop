package com.alticast.rop;
import com.alticast.io.*;

public class I8Codec implements Codec
{
    public static final I8Codec instance = new I8Codec();

    public void scan (Buffer buf) {
        buf.drop(1);
    }

    public Object read (Buffer buf) {
        return New.i8(buf.readI8());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeI8(((Byte)obj).byteValue());
    }
}
