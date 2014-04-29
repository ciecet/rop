package com.alticast.io;

public class NativeIo {

    private NativeIo () {}

    public static final int ERROR_UNKNOWN = -1;
    public static final int ERROR_WOULDBLOCK = -2;
    public static final int ERROR_BIND = -2;

    public static native int createServerSocket (int port);
    public static native int createSocket (String addr, int port);
    public static native int accept (int servSocket);
    public static native void close (int fd);

    public static native synchronized int tryRead (int fd, byte[] buf, int off,
            int size);
    public static native synchronized int tryWrite (int fd, byte[] buf, int off,
            int size);

    public static native long getMonotonicTime ();
}
