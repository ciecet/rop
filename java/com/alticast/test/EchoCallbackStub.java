package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;

public class EchoCallbackStub extends Stub implements EchoCallback {
    public void call (String msg) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(1<<6, remote, 0);
            com.alticast.rop.StringCodec.instance.write(msg, buf);
            remote.registry.asyncCall(rc, false);
        } finally {
            New.release(rc);
        }
    }
}
