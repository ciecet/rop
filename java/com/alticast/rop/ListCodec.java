package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class ListCodec implements Codec {

    private final Codec itemCodec;

    public ListCodec (Codec ic) {
        itemCodec = ic;
    }

    public void scan (Buffer buf) {
        int num = buf.readI32();
        while (num-- > 0) {
            itemCodec.scan(buf);
        }
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        List o = new ArrayList(num);
        while (num-- > 0) {
            o.add(itemCodec.read(buf));
        }
        return o;
    }

    public void write (Object obj, Buffer buf) {
        List o = (List)obj;
        int num = o.size();
        buf.writeI32(num);
        for (int i = 0; i < num; i++) {
            itemCodec.write(o.get(i), buf);
        }
    }
}
