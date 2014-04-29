package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class F32ArrayCodec implements Codec {

    public static final F32ArrayCodec instance = new F32ArrayCodec();

    public void scan (Buffer buf) {
        int num = buf.readI32();
        buf.drop(num * 4);
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        float[] o = new float[num];
        for (int i = 0; i < num; i++) {
            o[i] = buf.readF32();
        }
        return o;
    }

    public void write (Object obj, Buffer buf) {
        float[] o = (float[])obj;
        int num = o.length;
        buf.writeI32(num);
        for (int i = 0; i < num; i++) {
            buf.writeF32(o[i]);
        }
    }
}
