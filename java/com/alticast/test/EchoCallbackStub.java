package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;

public class EchoCallbackStub extends Stub implements EchoCallback {

   private static final Codec codec0 = com.alticast.rop.StringCodec.instance;

    public void call (String msg) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(1<<6, remote, 0);
            codec0.write(msg, buf);
            remote.registry.asyncCall(rc, false);
        } finally {
            New.release(rc);
        }
    }
}
