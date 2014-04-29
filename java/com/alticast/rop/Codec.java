package com.alticast.rop;
import com.alticast.io.*;
public interface Codec {
    // pre-read given buffer and gather local objects
    void scan (Buffer buf);
    Object read (Buffer buf);
    void write (Object obj, Buffer buf);
}
