import java.util.*;

public class EchoStub extends Stub implements Echo {

    private static final Codec[] codecs = new Codec[] {
        StringCodec.instance,
        new ListCodec(StringCodec.instance),
        VoidCodec.instance, 
        TestExceptionCodec.instance,
        new InterfaceCodec(EchoCallbackStub.class),
        PersonCodec.instance
    };

    public String echo (String msg) {
        RemoteCall call = (RemoteCall)New.get(RemoteCall.class);

        try {
            Buffer buf = call.init(0<<6, remote, 0);

            codecs[0].write(msg, buf);
            remote.registry.syncCall(call);

            switch (buf.readI8() & 63) {
            case 0: return (String)codecs[0].read(buf);
            default: throw new RemoteException("Remote call failed.");
            }
        } finally {
            New.release(call);
        }
    }

    public String concat (List words) {
        RemoteCall call = (RemoteCall)New.get(RemoteCall.class);

        try {
            Buffer buf = call.init(0<<6, remote, 1);

            codecs[1].write(words, buf);
            remote.registry.syncCall(call);

            switch (buf.readI8() & 63) {
            case 0: return (String)codecs[0].read(buf);
            default: throw new RemoteException("Remote call failed.");
            }
        } finally {
            New.release(call);
        }
    }

    public void touchmenot () throws TestException {
        RemoteCall call = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = call.init(0<<6, remote, 2);

            remote.registry.syncCall(call);

            switch (buf.readI8() & 63) {
            case 0: return;
            case 1: throw (TestException)codecs[3].read(buf);
            default: throw new RemoteException("Remote call failed.");
            }
        } finally {
            New.release(call);
        }
    }

    public void recursiveEcho (String msg, EchoCallback cb) {
        RemoteCall call = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = call.init(0<<6, remote, 3);

            codecs[0].write(msg, buf);
            codecs[4].write(cb, buf);
            remote.registry.syncCall(call);

            switch (buf.readI8() & 63) {
            case 0: return;
            default: throw new RemoteException("Remote call failed.");
            }
        } finally {
            New.release(call);
        }
    }

    public void hello (Person p) {
        RemoteCall call = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = call.init(0<<6, remote, 4);

            codecs[5].write(p, buf);
            remote.registry.syncCall(call);

            switch (buf.readI8() & 63) {
            case 0: return;
            default: throw new RemoteException("Remote call failed.");
            }
        } finally {
            New.release(call);
        }
    }

    public void asyncEcho (String msg, EchoCallback cb) {
        RemoteCall call = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = call.init(1<<6, remote, 5);

            codecs[0].write(msg, buf);
            codecs[4].write(cb, buf);
            remote.registry.asyncCall(call, false);
        } finally {
            New.release(call);
        }
    }
}
