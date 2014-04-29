package com.alticast.io;

import java.io.*;
import java.util.*;
import com.alticast.rop.*;

public class HttpServerReactor extends FileReactor implements LogConsts {

    private final Map requestAcceptors = new HashMap();
    public void bind (String basePath, HttpRequestAcceptor h) {
        requestAcceptors.put(basePath, h);
    }

    public void read () {
        int fd = NativeIo.accept(getFd());
        if (I) Log.info("New http connection. fd:"+fd);

        getEventDriver().watchFile(fd, EventDriver.EVENT_READ,
                createHttpReactor());
    }

    protected HttpReactor createHttpReactor () {
        return new HttpReactor(this, false);
    }

    // subclass my override this method to handle incoming request.
    // default behavior is to call dispatchRequest().

    /**
     * Returns handler for given request.
     * May return null if no proper handler found.
     */
    void updateRequestHandler (HttpReactor r) {
        String path = r.getRequestPath();
        HttpRequestAcceptor acceptor = null;
        String basePath = null;
        int slash = 0;

        if (path.length() > 0 && path.charAt(0) == '/') {
            while (acceptor == null && slash < path.length()) {
                slash = path.indexOf('/', slash + 1);
                if (slash == -1) slash = path.length();
                basePath = path.substring(0, slash);
                acceptor = (HttpRequestAcceptor)requestAcceptors.get(basePath);
            }

            // try to use default acceptor
            if (acceptor == null) {
                basePath = null;
                acceptor = (HttpRequestAcceptor)requestAcceptors.get(basePath);
            }
        }

        if (acceptor == null) {
            if (W) Log.debug("No request acceptor for "+path);
            r.setRequestHandler(null, null);
        } else {
            if (D) Log.debug("Found acceptor: "+acceptor);
            r.setRequestHandler(acceptor.acceptHttpRequest(basePath, r),
                    basePath);
        }
        Log.warn(requestAcceptors);
    }

    public void start (int port) throws IOException {
        start(port, EventDriver.getDefaultInstance());
    }

    public void start (int port, EventDriver ed) throws IOException {
        int fd = NativeIo.createServerSocket(port);
        if (fd == -1) throw new IOException("cannot create server socket");
        ed.watchFile(fd, EventDriver.EVENT_READ, this);
    }

    public void stop () {
        if (I) Log.info("Stopping http server by request");
        close();
    }
}
