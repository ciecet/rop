import java.util.*;

public class EchoSkel extends Skeleton {

    public EchoSkel (Echo e) {
        object = e;
    }

    public Request createRequest (int h, int mid) {
        synchronized (New.class) {
            Request req = New.request(h, this, mid);
            switch (mid) {
            case 0:
                req.addReader(New.stringReader());
                req.ret.addWriter(New.stringWriter());
                return req;
            case 1:
                req.addReader(New.listReader(New.stringReader()));
                req.ret.addWriter(New.stringWriter());
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
            __call_echo(req.arguments, req.ret);
            return;
        case 1:
            __call_concat(req.arguments, req.ret);
            return;
        default:
            return;
        }
    }

    public void __call_echo (List args, ReturnWriter ret) {
        try {
            ret.value = ((Echo)object).echo((String)args.get(0));
            ret.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            ret.index = -1;
        }
    }

    public void __call_concat (List args, ReturnWriter ret) {
        try {
            ret.value = ((Echo)object).concat((List)args.get(0));
            ret.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            ret.index = -1;
        }
    }
}
