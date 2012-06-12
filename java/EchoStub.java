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
}

