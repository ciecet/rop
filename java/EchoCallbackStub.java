public class EchoCallbackStub extends Stub implements EchoCallback {
    
    private static final Codec[] codecs = new Codec[] {
        StringCodec.instance
    };

    public void call (String msg) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);

        try {
            Buffer buf = rc.init(1<<6, remote, 0);
            codecs[0].write(msg, buf);
            remote.registry.asyncCall(rc, false);
        } finally {
            New.release(rc);
        }
    }
}
