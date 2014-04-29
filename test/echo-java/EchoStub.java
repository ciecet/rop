package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.TestException;
import com.alticast.test.EchoCallback;
import com.alticast.test.Person;
public class EchoStub extends Stub implements Echo {

   private static final Codec codec0 = com.alticast.rop.StringCodec.instance;
   private static final Codec codec1 = new com.alticast.rop.ListCodec(com.alticast.rop.StringCodec.instance);
   private static final Codec codec2 = com.alticast.rop.VoidCodec.instance;
   private static final Codec codec3 = com.alticast.test.TestExceptionCodec.instance;
   private static final Codec codec4 = new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class);
   private static final Codec codec5 = com.alticast.test.PersonCodec.instance;
   private static final Codec codec6 = com.alticast.rop.I32Codec.instance;
   private static final Codec[] ret0 = { codec0 };
   private static final Codec[] ret1 = { null, codec3 };
   private static final Codec[] ret2 = { codec6 };

    public String echo (String msg) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 0);
            codec0.write(msg, buf);
            rc.setReturnCodecs(ret0);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            default: throw new RemoteException("Remote Call Failed.");
            case 0: return (String)(codec0.read(buf));
            }
        } finally {
            New.release(rc);
        }
    }
    public String concat (List msgs) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 1);
            codec1.write(msgs, buf);
            rc.setReturnCodecs(ret0);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return (String)(codec0.read(buf));
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
    public void touchmenot () throws com.alticast.test.TestException {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 2);

            rc.setReturnCodecs(ret1);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return;
            case 1: throw (com.alticast.test.TestException)(codec3.read(buf));
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
    public void recursiveEcho (String msg, com.alticast.test.EchoCallback cb) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 3);
            codec0.write(msg, buf);
            codec4.write(cb, buf);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return;
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
    public void hello (com.alticast.test.Person p) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 4);
            codec5.write(p, buf);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return;
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
    public void asyncEcho (String msg, com.alticast.test.EchoCallback cb) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(1<<6, remote, 5);
            codec0.write(msg, buf);
            codec4.write(cb, buf);
            remote.registry.asyncCall(rc, false);
        } finally {
            New.release(rc);
        }
    }
    public int doubleit (int i) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 6);
            buf.writeI32(i);
            rc.setReturnCodecs(ret2);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return buf.readI32();
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
}
