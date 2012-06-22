package com.alticast.rop;

import java.util.*;
import java.io.*;

public class New {

    private static final int ICACHE_MAX = 32;
    private static Map pool = new IdentityHashMap();
    private static Byte[] i8s = new Byte[ICACHE_MAX*2];
    private static Short[] i16s = new Short[ICACHE_MAX*2];
    private static Integer[] i32s = new Integer[ICACHE_MAX*2];

    static {
        for (int i = 0; i < ICACHE_MAX*2; i++) {
            i8s[i] = new Byte((byte)(i - ICACHE_MAX));
            i16s[i] = new Short((short)(i - ICACHE_MAX));
            i32s[i] = new Integer(i - ICACHE_MAX);
        }
    }

    public static synchronized Reusable get (Class cls) {
        List l = (List)pool.get(cls);
        if (l != null) {
            int s = l.size();
            if (s > 0) {
                return (Reusable)l.remove(s - 1);
            }
        }

        try {
            return (Reusable)cls.newInstance();
        } catch (Exception e) {
            e.printStackTrace();
            throw new IllegalArgumentException("no default construct");
        }
    }

    public static synchronized void release (Reusable obj) {
        if (obj == null) return;
        Class cls = obj.getClass();
        List l = (List)pool.get(cls);
        if (l == null) {
            l = new ArrayList(4);
            pool.put(cls, l);
        }
        if (l.size() < 4) {
            l.add(obj);
            obj.reuse();
        }
    }

    public static Byte i8 (byte i) {
        if (i >= -ICACHE_MAX && i < ICACHE_MAX) {
            return i8s[i + ICACHE_MAX];
        }
        return new Byte(i);
    }

    public static Short i16 (short i) {
        if (i >= -ICACHE_MAX && i < ICACHE_MAX) {
            return i16s[i + ICACHE_MAX];
        }
        return new Short(i);
    }

    public static Integer i32 (int i) {
        if (i >= -ICACHE_MAX && i < ICACHE_MAX) {
            return i32s[i + ICACHE_MAX];
        }
        return new Integer(i);
    }

    public static Long i64 (long i) {
        return new Long(i);
    }
}
