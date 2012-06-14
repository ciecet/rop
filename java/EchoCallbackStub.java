public class EchoCallbackStub extends Stub implements EchoCallback {
    public void call (String msg) {
        RequestWriter req;
        synchronized (New.class) {
            req = New.requestWriter(1<<6, remote.id, 0);
            req.addArg(msg, New.stringWriter());
        }
        remote.registry.asyncCall(req, null);
    }
}
