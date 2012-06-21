import java.util.*;

public class EchoCallbackSkel extends Skeleton {

    private static final Codec[] codecs = new Codec[] {
        StringCodec.instance
    };

    public EchoCallbackSkel (EchoCallback o) {
        object = o;
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
        case 0: __call_call(lc); return;
        default: return;
        }
    }

    public void __call_call (LocalCall lc) {
        Buffer buf = lc.buffer;
        try {
            String arg0 = (String)codecs[0].read(buf);
            ((EchoCallback)object).call(arg0);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
