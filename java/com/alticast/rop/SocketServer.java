package com.alticast.rop;

import java.util.*;

import com.alticast.rop.*;
import com.alticast.io.*;
import com.alticast.main.*;

public class SocketServer implements LogConsts {

    public void start (int port) {
        start(port, EventDriver.getDefaultInstance());
    }

    public void start (int port, final EventDriver ed) {

        final FileReactor server = new FileReactor() {
            public void read () {
                int fd = NativeIo.accept(getFd());
                if (I) Log.info("New socket connection. fd:"+fd+" srvfd:"+getFd());
                SocketTransport st = new SocketTransport();
                ed.watchFile(fd, ed.EVENT_READ, st.getReactor());
            }
        };

        int fd = NativeIo.createServerSocket(port);
        if (fd == -1) throw new RemoteException("socket server failed");
        ed.watchFile(fd, ed.EVENT_READ, server);
    }

    public static boolean init () {
        SocketServer srv = new SocketServer();
        srv.start(Config.AL_CF_ROP_SOCKET_SERVER_PORT);
        return true;
    }
}
