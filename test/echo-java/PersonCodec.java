package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.EchoCallback;
public class PersonCodec implements Codec {

   private static final Codec codec0 = com.alticast.rop.StringCodec.instance;
   private static final Codec codec1 = new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class);

    public static final PersonCodec instance = new PersonCodec();

    public Object read (Buffer buf) {
        Person o = new Person();
        o.name = (String)(codec0.read(buf));
        o.callback = (com.alticast.test.EchoCallback)(codec1.read(buf));
        return o;
    }

    public void write (Object obj, Buffer buf) {
        Person o = (Person)obj;
        codec0.write(o.name, buf);
        codec1.write(o.callback, buf);
    }
}
