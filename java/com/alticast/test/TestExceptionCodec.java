package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;

public class TestExceptionCodec implements Codec {

   private static final Codec codec0 = new com.alticast.rop.NullableCodec(com.alticast.rop.I32Codec.instance);

    public static final TestExceptionCodec instance = new TestExceptionCodec();

    public Object read (Buffer buf) {
        TestException o = new TestException();
        o.i = (Integer)(codec0.read(buf));
        return o;
    }

    public void write (Object obj, Buffer buf) {
        TestException o = (TestException)obj;
        codec0.write(o.i, buf);
    }
}
