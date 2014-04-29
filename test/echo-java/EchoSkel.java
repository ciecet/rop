package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.TestException;
import com.alticast.test.EchoCallback;
import com.alticast.test.Person;
public class EchoSkel extends Skeleton {

   private static final Codec codec0 = com.alticast.rop.StringCodec.instance;
   private static final Codec codec1 = new com.alticast.rop.ListCodec(com.alticast.rop.StringCodec.instance);
   private static final Codec codec2 = com.alticast.rop.VoidCodec.instance;
   private static final Codec codec3 = com.alticast.test.TestExceptionCodec.instance;
   private static final Codec codec4 = new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class);
   private static final Codec codec5 = com.alticast.test.PersonCodec.instance;
   private static final Codec codec6 = com.alticast.rop.I32Codec.instance;

    public EchoSkel (Echo o) {
        object = o;
    }

    public void scan (Buffer buf) {
        switch (lc.buffer.readI16()) {
        default: throw new RemoteException("Out of method index range.");
        case 0:
            codec0.scan(buf);
            return;
        case 1:
            codec1.scan(buf);
            return;
        case 2:
            return;
        case 3:
            codec0.scan(buf);
            codec4.scan(buf);
            return;
        case 4:
            codec5.scan(buf);
            return;
        case 5:
            codec0.scan(buf);
            codec4.scan(buf);
            return;
        case 6:
            codec6.scan(buf);
            return;
        }
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
        default: throw new RemoteException("Out of method index range.");
        case 0: __call_echo(lc); return;
        case 1: __call_concat(lc); return;
        case 2: __call_touchmenot(lc); return;
        case 3: __call_recursiveEcho(lc); return;
        case 4: __call_hello(lc); return;
        case 5: __call_asyncEcho(lc); return;
        case 6: __call_doubleit(lc); return;
        }
    }

    private void __call_echo (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(codec0.read(__buf));
            lc.value = ((Echo)object).echo(msg);
            lc.codec = codec0;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_concat (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            List msgs = (List)(codec1.read(__buf));
            lc.value = ((Echo)object).concat(msgs);
            lc.codec = codec0;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_touchmenot (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            ((Echo)object).touchmenot();
            lc.codec = codec2;
            lc.index = 0;
        } catch (com.alticast.test.TestException e) {
            lc.value = e;
            lc.codec = codec3;
            lc.index = 1;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_recursiveEcho (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(codec0.read(__buf));
            com.alticast.test.EchoCallback cb = (com.alticast.test.EchoCallback)(codec4.read(__buf));
            ((Echo)object).recursiveEcho(msg, cb);
            lc.codec = codec2;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_hello (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            com.alticast.test.Person p = (com.alticast.test.Person)(codec5.read(__buf));
            ((Echo)object).hello(p);
            lc.codec = codec2;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_asyncEcho (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(codec0.read(__buf));
            com.alticast.test.EchoCallback cb = (com.alticast.test.EchoCallback)(codec4.read(__buf));
            ((Echo)object).asyncEcho(msg, cb);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_doubleit (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            int i = __buf.readI32();
            lc.value = New.i32(((Echo)object).doubleit(i));
            lc.codec = codec6;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
