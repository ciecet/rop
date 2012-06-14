import java.util.*;

class EchoStub extends Stub implements Echo {

    public EchoStub (Remote r) {
        super(r);
    }

    public String echo (String msg) {
        RequestWriter req;
        ReturnReader ret;
        synchronized (New.class) {
            req = New.requestWriter(0, remote.id, 0);
            req.addArg(msg, New.stringWriter());
            ret = New.returnReader();
            ret.addReader(New.stringReader());
        }
        remote.registry.syncCall(req, ret);
        switch (ret.index) {
        case 0:
            return (String)ret.value;
        default:
            throw new RemoteException("return index:"+ret.index);
        }
    }

    public String concat (List words) {
        RequestWriter req;
        ReturnReader ret;
        synchronized (New.class) {
            req = New.requestWriter(0, remote.id, 1);
            req.addArg(words, New.listWriter(New.stringWriter()));
            ret = New.returnReader();
            ret.addReader(New.stringReader());
        }
        remote.registry.syncCall(req, ret);
        switch (ret.index) {
        case 0:
            return (String)ret.value;
        default:
            throw new RemoteException("return index:"+ret.index);
        }
    }

    public void touchmenot () throws TestException {
        RequestWriter req;
        ReturnReader ret;
        synchronized (New.class) {
            req = New.requestWriter(0, remote.id, 2);
            ret = New.returnReader();
            ret.addReader(New.voidReader());
            ret.addReader(New.reader("TestException"));
        }
        remote.registry.syncCall(req, ret);
        if (ret.index == 1) {
            throw (TestException)ret.value;
        }
    }

    public void recursiveEcho (String msg, EchoCallback cb) {
        RequestWriter req;
        ReturnReader ret;
        synchronized (New.class) {
            req = New.requestWriter(0, remote.id, 3);
            req.addArg(msg, New.stringWriter());
            req.addArg(cb, New.interfaceWriter("EchoCallback"));
            ret = New.returnReader();
            ret.addReader(New.voidReader());
        }
        remote.registry.syncCall(req, ret);
    }

    public void hello (Person p) {
        RequestWriter req;
        ReturnReader ret;
        synchronized (New.class) {
            req = New.requestWriter(0, remote.id, 4);
            req.addArg(p, New.writer("Person"));
            ret = New.returnReader();
            ret.addReader(New.voidReader());
        }
        remote.registry.syncCall(req, ret);
    }

    public void asyncEcho (String msg, EchoCallback cb) {
        RequestWriter req;
        synchronized (New.class) {
            req = New.requestWriter(0, remote.id, 5);
            req.addArg(msg, New.stringWriter());
            req.addArg(cb, New.interfaceWriter("EchoCallback"));
        }
        remote.registry.asyncCall(req, null);
    }
}
