package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class I8ArrayCodec implements Codec {

    public static final I8ArrayCodec instance = new I8ArrayCodec();

    public void scan (Buffer buf) {
        int num = buf.readI32();
        buf.drop(num);
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        byte[] o = new byte[num];
        buf.readBytes(o, 0, num);
        return o;
    }

    public void write (Object obj, Buffer buf) {
        byte[] o = (byte[])obj;
        int num = o.length;
        buf.writeI32(num);
        buf.writeBytes(o, 0, num);
    }
}
