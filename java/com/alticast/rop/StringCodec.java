package com.alticast.rop;
import com.alticast.io.*;

import java.io.UnsupportedEncodingException;

public class StringCodec implements Codec
{
    public static final StringCodec instance = new StringCodec();

    public void scan (Buffer buf) {
        int len = buf.readI32();
        buf.drop(len);
    }

    public Object read (Buffer buf) {
        return buf.readString(buf.readI32());
    }

    public void write (Object obj, Buffer buf) {
        buf.writeString((String)obj);
    }
}
