package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;

public class TestExceptionCodec implements Codec {

    public static final TestExceptionCodec instance = new TestExceptionCodec();

    public Object read (Buffer buf) {
        TestException o = new TestException();
        o.i = (Integer)(new com.alticast.rop.NullableCodec(com.alticast.rop.I32Codec.instance).read(buf));
        return o;
    }

    public void write (Object obj, Buffer buf) {
        TestException o = (TestException)obj;
        new com.alticast.rop.NullableCodec(com.alticast.rop.I32Codec.instance).write(o.i, buf);
    }
}
