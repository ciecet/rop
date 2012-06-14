import java.util.*;

public class EchoCallbackSkel extends Skeleton {

    public EchoCallbackSkel (EchoCallback o) {
        object = o;
    }

    public Request createRequest (int h, int mid) {
        synchronized (New.class) {
            Request req = New.request(h, this, mid);
            switch (mid) {
            case 0:
                req.addReader(New.stringReader());
                return req;
            default:
                req.release();
                return null;
            }
        }
    }

    public void call (Request req) {
        switch (req.methodIndex) {
        case 0:
            __call_call(req.arguments, null);
            return;
        default:
            return;
        }
    }

    public void __call_call (List args, ReturnWriter ret) {
        try {
            ((EchoCallback)object).call((String)args.get(0));
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
