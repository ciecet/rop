package com.alticast.rop;

public class RemoteException extends RuntimeException {
    public RemoteException (String msg) {
        super(msg);
    }

    public RemoteException (String msg, Throwable cause) {
        super(msg, cause);
    }
}
