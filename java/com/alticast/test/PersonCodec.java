package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.EchoCallback;
public class PersonCodec implements Codec {

    public static final PersonCodec instance = new PersonCodec();

    public Object read (Buffer buf) {
        Person o = new Person();
        o.name = (String)(com.alticast.rop.StringCodec.instance.read(buf));
        o.callback = (com.alticast.test.EchoCallback)(new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class).read(buf));
        return o;
    }

    public void write (Object obj, Buffer buf) {
        Person o = (Person)obj;
        com.alticast.rop.StringCodec.instance.write(o.name, buf);
        new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class).write(o.callback, buf);
    }
}
