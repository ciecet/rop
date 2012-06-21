import java.util.*;

public class EchoSkel extends Skeleton {

    private static final Codec[] codecs = new Codec[] {
        StringCodec.instance,
        new ListCodec(StringCodec.instance),
        VoidCodec.instance, 
        TestExceptionCodec.instance,
        new InterfaceCodec(EchoCallbackStub.class),
        PersonCodec.instance
    };

    public EchoSkel (Echo e) {
        object = e;
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
        case 0: __call_echo(lc); return;
        case 1: __call_concat(lc); return;
        case 2: __call_touchmenot(lc); return;
        case 3: __call_recursiveEcho(lc); return;
        case 4: __call_hello(lc); return;
        case 5: __call_asyncEcho(lc); return;
        default: return;
        }
    }

    public void __call_echo (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            String arg0 = (String)codecs[0].read(buf);
            lc.value = ((Echo)object).echo(arg0);
            lc.codec = codecs[0];
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            lc.index = -1;
        }
    }

    public void __call_concat (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            List arg0 = (List)codecs[1].read(buf);
            lc.value = ((Echo)object).concat(arg0);
            lc.codec = codecs[0];
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            lc.index = -1;
        }
    }

    public void __call_touchmenot (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            ((Echo)object).touchmenot();
            lc.index = 0;
        } catch (TestException e) {
            lc.value = e;
            lc.codec = codecs[3];
            lc.index = 1;
        } catch (Throwable t) {
            t.printStackTrace();
            lc.index = -1;
        }
    }

    public void __call_recursiveEcho (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            String arg0 = (String)codecs[0].read(buf);
            EchoCallback arg1 = (EchoCallback)codecs[4].read(buf);
            ((Echo)object).recursiveEcho(arg0, arg1);
            lc.codec = codecs[2];
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            lc.index = -1;
        }
    }

    public void __call_hello (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            Person arg0 = (Person)codecs[5].read(buf);
            ((Echo)object).hello(arg0);
            lc.codec = codecs[2];
            lc.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            lc.index = -1;
        }
    }

    public void __call_asyncEcho (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            String arg0 = (String)codecs[0].read(buf);
            EchoCallback arg1 = (EchoCallback)codecs[4].read(buf);

            ((Echo)object).asyncEcho(arg0, arg1);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
