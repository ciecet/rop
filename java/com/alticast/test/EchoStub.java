package com.alticast.test;
import com.alticast.rop.*;
import java.util.*;
import com.alticast.test.TestException;
import com.alticast.test.EchoCallback;
import com.alticast.test.Person;
public class EchoStub extends Stub implements Echo {
    public String echo (String msg) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 0);
            com.alticast.rop.StringCodec.instance.write(msg, buf);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return (String)(com.alticast.rop.StringCodec.instance.read(buf));
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
    public String concat (List msgs) {
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 1);
            new com.alticast.rop.ListCodec(com.alticast.rop.StringCodec.instance).write(msgs, buf);
            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return (String)(com.alticast.rop.StringCodec.instance.read(buf));
            default: throw new RemoteException("Remote Call Failed.");
            }
        } finally {
            New.release(rc);
        }
    }
    public void touchmenot () throws com.alticast.test.TestException{
        RemoteCall rc = (RemoteCall)New.get(RemoteCall.class);
        try {
            Buffer buf = rc.init(0<<6, remote, 2);

            remote.registry.syncCall(rc);
            switch (buf.readI8() & 63) {
            case 0: return;
            case 1: throw (com.alticast.test.TestException)(com.alticast.test.TestExceptionCodec.instance.read(buf));
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
            com.alticast.rop.StringCodec.instance.write(msg, buf);
            new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class).write(cb, buf);
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
            com.alticast.test.PersonCodec.instance.write(p, buf);
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
            com.alticast.rop.StringCodec.instance.write(msg, buf);
            new com.alticast.rop.InterfaceCodec(com.alticast.test.EchoCallbackStub.class).write(cb, buf);
            remote.registry.asyncCall(rc, false);
        } finally {
            New.release(rc);
        }
    }
}
