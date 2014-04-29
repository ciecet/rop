package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class I32ArrayCodec implements Codec {

    public static final I32ArrayCodec instance = new I32ArrayCodec();

    public void scan (Buffer buf) {
        int num = buf.readI32();
        buf.drop(num * 4);
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        int[] o = new int[num];
        for (int i = 0; i < num; i++) {
            o[i] = buf.readI32();
        }
        return o;
    }

    public void write (Object obj, Buffer buf) {
        int[] o = (int[])obj;
        int num = o.length;
        buf.writeI32(num);
        for (int i = 0; i < num; i++) {
            buf.writeI32(o[i]);
        }
    }
}
