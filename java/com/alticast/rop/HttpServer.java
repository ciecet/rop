package com.alticast.rop;

import java.io.*;
import java.util.*;
import java.math.*;
import java.security.*;

import com.alticast.io.*;
import com.alticast.main.*;

/**
 * Http Server for ROP support.
 * Note that this server creates safe HttpReactor which reacts on events
 * even in nested EventDriver context.
 */
public class HttpServer extends HttpServerReactor implements
        HttpRequestAcceptor {

    private static final SecureRandom random = new SecureRandom();
    private static final HttpServer instance = new HttpServer();
    public static HttpServer getInstance() { return instance; }
    public static boolean init () {
        try {
            instance.start(Config.AL_CF_ROP_HTTP_SERVER_PORT);
            return true;
        } catch (IOException e) {
            e.printStackTrace();
            return false;
        }
    }

    private static String createSessionId () {
        // 32 digit. (160 / log(2,32))
        return new BigInteger(160, random).toString(32);
    }

    private HttpServer () {
        bind("/r", this);
        bind(null, new AbstractHttpRequestHandler () {
            public void handleHttpRequest (HttpReactor r) {
                try {
                    String path = r.getRequestPath();
                    r.sendFile(Config.AL_CF_ROP_HTTP_SERVER_DOC_DIR + path,
                            (String[])pageHeaders.get(path));
                } catch (IOException e) {
                    r.close(e);
                }
            }
        });
    }

    protected HttpReactor createHttpReactor () {
        return new HttpReactor(this, true);
    }

    private Map transports = new HashMap();
    private Map pageHeaders = new HashMap();

    public void setPageHeaders (String page, String[] headers) {
        pageHeaders.put(page, headers);
    }

    void transportClosed (String sid) {
        transports.remove(sid);
    }

    public HttpRequestHandler acceptHttpRequest (String basePath,
            HttpReactor r) {
        // drop base path
        String path = r.getRequestPath().substring(basePath.length());

        String sid;
        HttpTransport trans = null;

        // if this connection is a websocket,
        if (r.isWebsocket()) {

            // check stale transports on this callback.
            // (can be moved to other place if necessary)
            for (Iterator i = transports.values().iterator(); i.hasNext(); ) {
                HttpTransport t = (HttpTransport)i.next();
                if (!t.isStale()) continue;
                i.remove();
                t.close();
            }

            if (path.length() > 0 && path.charAt(0) == '/') {
                // use already existing transport
                sid = path.substring(1);
                trans = (HttpTransport)transports.get(sid);
                if (trans == null || trans.hasWebSocket()) return null;
            } else {
                // create websocket-initiated transport
                sid = createSessionId();
                trans = new HttpTransport(sid, this);
                if (I) {
                    Log.info("Created websocket-initiated transport " + trans);
                }
                transports.put(sid, trans);
            }

            // send session id
            Buffer buf = new Buffer();
            buf.writeBytes(sid.getBytes());
            try {
                r.sendMessage(buf);
            } catch (IOException e) {
                r.close(e);
                trans.close();
                return null;
            }

            trans.setWebSocket(r);
            return trans;
        }

        // Create xhr-initiated transport ahead.
        if (path.length() == 0) {
            sid = createSessionId();
            trans = new HttpTransport(sid, this);
            if (I) {
                Log.info("Created xhr-initiated transport " + trans +
                        " sid:" + sid);
            }
            transports.put(sid, trans);
            return new SimpleResponse(200, sid);
        }

        // otherwise, treat as rop over xhr.
        sid = path.substring(1);
        trans = (HttpTransport)transports.get(sid);
        if (trans == null) {
            if (W) Log.warn("No corresponding transport for sid:"+sid);
            return new SimpleResponse(404, "No transport here!");
        }
        trans.touchTimeout();
        return trans;
    }

    private static class SimpleResponse extends AbstractHttpRequestHandler {
        private int status;
        private String message;
        public SimpleResponse (int sc, String msg) {
            status = sc;
            message = msg;
        }
        public void handleHttpRequest (HttpReactor r) {
            try {
                r.sendText(status, message);
            } catch (IOException e) {
                r.close(e);
            }
        }
    }
}
