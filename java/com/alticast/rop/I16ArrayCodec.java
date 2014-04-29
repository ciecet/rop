package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class I16ArrayCodec implements Codec {

    public static final I16ArrayCodec instance = new I16ArrayCodec();

    public void scan (Buffer buf) {
        int num = buf.readI32();
        buf.drop(num * 2);
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        short[] o = new short[num];
        for (int i = 0; i < num; i++) {
            o[i] = buf.readI16();
        }
        return o;
    }

    public void write (Object obj, Buffer buf) {
        short[] o = (short[])obj;
        int num = o.length;
        buf.writeI32(num);
        for (int i = 0; i < num; i++) {
            buf.writeI16(o[i]);
        }
    }
}
