package com.alticast.rop;
import com.alticast.io.*;

import java.util.*;

public class MapCodec implements Codec {

    private final Codec keyCodec, valueCodec;

    public MapCodec (Codec kc, Codec vc) {
        keyCodec = kc;
        valueCodec = vc;
    }

    public void scan (Buffer buf) {
        int num = buf.readI32();
        while (num-- > 0) {
            keyCodec.scan(buf);
            valueCodec.scan(buf);
        }
    }

    public Object read (Buffer buf) {
        int num = buf.readI32();
        Map m = new HashMap(num);
        while (num-- > 0) {
            Object k = keyCodec.read(buf);
            Object v = valueCodec.read(buf);
            m.put(k, v);
        }
        return m;
    }

    public void write (Object obj, Buffer buf) {
        Map m = (Map)obj;
        buf.writeI32(m.size());
        for (Iterator iter = m.keySet().iterator(); iter.hasNext();) {
            Object k = iter.next();
            keyCodec.write(k, buf);
            valueCodec.write(m.get(k), buf);
        }
    }
}
