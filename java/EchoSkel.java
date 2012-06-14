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
                break;
            case 1:
                req.addReader(New.listReader(New.stringReader()));
                req.ret.addWriter(New.stringWriter());
                break;
            case 2:
                req.ret.addWriter(New.voidWriter());
                req.ret.addWriter(New.writer("TestException"));
                Log.debug(req);
                break;
            case 3:
                req.addReader(New.stringReader());
                req.addReader(New.interfaceReader("EchoCallback"));
                req.ret.addWriter(New.voidWriter());
                break;
            case 4:
                req.addReader(New.reader("Person"));
                req.ret.addWriter(New.voidWriter());
                break;
            case 5:
                req.addReader(New.stringReader());
                req.addReader(New.interfaceReader("EchoCallback"));
                break;
            default:
                req.release();
                req = null;
            }
            return req;
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
        case 2:
            __call_touchmenot(req.arguments, req.ret);
            return;
        case 3:
            __call_recursiveEcho(req.arguments, req.ret);
            return;
        case 4:
            __call_hello(req.arguments, req.ret);
            return;
        case 5:
            __call_asyncEcho(req.arguments, req.ret);
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

    public void __call_touchmenot (List args, ReturnWriter ret) {
        try {
            ((Echo)object).touchmenot();
            ret.index = 0;
        } catch (TestException e) {
            ret.value = e;
            ret.index = 1;
        } catch (Throwable t) {
            t.printStackTrace();
            ret.index = -1;
        }
    }

    public void __call_recursiveEcho (List args, ReturnWriter ret) {
        try {
            ((Echo)object).recursiveEcho((String)args.get(0), (EchoCallback)args.get(1));
            ret.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            ret.index = -1;
        }
    }

    public void __call_hello (List args, ReturnWriter ret) {
        try {
            ((Echo)object).hello((Person)args.get(0));
            ret.index = 0;
        } catch (Throwable t) {
            t.printStackTrace();
            ret.index = -1;
        }
    }

    public void __call_asyncEcho (List args, ReturnWriter ret) {
        try {
            ((Echo)object).asyncEcho((String)args.get(0), (EchoCallback)args.get(1));
        } catch (Throwable t) {
            t.printStackTrace();
        }
    }
}
