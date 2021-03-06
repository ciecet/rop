package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;

public class EchoCallbackSkel extends Skeleton {

   private static final Codec codec0 = com.alticast.rop.StringCodec.instance;

    public EchoCallbackSkel (EchoCallback o) {
        object = o;
    }

    public void processRequest (LocalCall lc) {
        switch (lc.buffer.readI16()) {
        case 0: __call_call(lc); return;
        default: throw new RemoteException("Out of method index range.");
        }
    }

    private void __call_call (LocalCall lc) {
        Buffer __buf = lc.buffer;
        try {
            String msg = (String)(codec0.read(__buf));
            ((EchoCallback)object).call(msg);
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
