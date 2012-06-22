package com.alticast.rop;
public interface Codec {
    Object read (Buffer buf);
    void write (Object obj, Buffer buf);
}
