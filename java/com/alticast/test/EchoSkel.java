package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.TestException;
import com.alticast.test.EchoCallback;
import com.alticast.test.Person;
public class EchoSkel extends Skeleton {
    public EchoSkel (Echo o) {
        object = o;
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
        case 0: __call_echo(lc); return;
        case 1: __call_concat(lc); return;
        case 2: __call_touchmenot(lc); return;
        case 3: __call_recursiveEcho(lc); return;
        case 4: __call_hello(lc); return;
        case 5: __call_asyncEcho(lc); return;
        default: throw new RemoteException("Out of method index range.");
        }
    }

    private void __call_echo (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(com.alticast.rop.StringCodec.instance.read(__buf));
            lc.value = ((Echo)object).echo(msg);
            lc.codec = com.alticast.rop.StringCodec.instance;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_concat (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            List msgs = (List)(new com.alticast.rop.ListCodec(com.alticast.rop.StringCodec.instance).read(__buf));
            lc.value = ((Echo)object).concat(msgs);
            lc.codec = com.alticast.rop.StringCodec.instance;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_touchmenot (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            ((Echo)object).touchmenot();
            lc.codec = com.alticast.rop.VoidCodec.instance;
            lc.index = 0;
        } catch (com.alticast.test.TestException e) {
            lc.value = e;
            lc.codec = com.alticast.test.TestExceptionCodec.instance;
            lc.index = 1;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_recursiveEcho (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(com.alticast.rop.StringCodec.instance.read(__buf));
            com.alticast.test.EchoCallback cb = (com.alticast.test.EchoCallback)(new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class).read(__buf));
            ((Echo)object).recursiveEcho(msg, cb);
            lc.codec = com.alticast.rop.VoidCodec.instance;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_hello (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            com.alticast.test.Person p = (com.alticast.test.Person)(com.alticast.test.PersonCodec.instance.read(__buf));
            ((Echo)object).hello(p);
            lc.codec = com.alticast.rop.VoidCodec.instance;
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
    private void __call_asyncEcho (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(com.alticast.rop.StringCodec.instance.read(__buf));
            com.alticast.test.EchoCallback cb = (com.alticast.test.EchoCallback)(new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class).read(__buf));
            ((Echo)object).asyncEcho(msg, cb);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
