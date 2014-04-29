package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class F64ArrayCodec implements Codec {

    public static final F64ArrayCodec instance = new F64ArrayCodec();

    public void scan (Buffer buf) {
        int num = buf.readI32();
        buf.drop(num * 8);
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        double[] o = new double[num];
        for (int i = 0; i < num; i++) {
            o[i] = buf.readF64();
        }
        return o;
    }

    public void write (Object obj, Buffer buf) {
        double[] o = (double[])obj;
        int num = o.length;
        buf.writeI32(num);
        for (int i = 0; i < num; i++) {
            buf.writeF64(o[i]);
        }
    }
}
