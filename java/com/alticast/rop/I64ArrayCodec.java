package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class I64ArrayCodec implements Codec {

    public static final I64ArrayCodec instance = new I64ArrayCodec();

    public void scan (Buffer buf) {
        int num = buf.readI32();
        buf.drop(num * 8);
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        long[] o = new long[num];
        for (int i = 0; i < num; i++) {
            o[i] = buf.readI64();
        }
        return o;
    }

    public void write (Object obj, Buffer buf) {
        long[] o = (long[])obj;
        int num = o.length;
        buf.writeI32(num);
        for (int i = 0; i < num; i++) {
            buf.writeI64(o[i]);
        }
    }
}
