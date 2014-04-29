package com.alticast.rop;

import java.io.*;
import java.util.*;
import com.alticast.io.*;

public class HttpTransport extends Transport implements HttpRequestHandler,
        LogConsts {

    // Represents the binding of port and xhr session.
    // xhr session may span to multiple xhr connections.
    private static class XhrSession {
        public int portId; // port id attached with this xhr session.
        public int callDepth; // increase on sync call, decrease on return
        public Buffer response; // pending xhr response
        public Buffer responseTail;

        public Buffer nextResponse () {
            Buffer r = response;
            response = r.next2; // tail is ignored if next is null.
            return r;
        }

        public void addResponse (Buffer sb) {
            if (response == null) {
                response = responseTail = sb;
            } else {
                responseTail.next2 = sb;
                responseTail = sb;
            }
        }
    }

    private boolean closed = false;
    private HttpReactor webSocket;

    // xmlHttpRequest was located out of XhrSession since rop uses blocked xhr
    // and only one xhr connection may exist at a time.
    private HttpReactor xmlHttpRequest;
    private XhrSession currentXhrSession;
    private List pendingMessages;

    // list of xhr sessions.
    // Multiple xhr sessions may exist at a time as following scenario:
    // 1. browser sends *sync* call (from port A)
    // 2. webmw sends back inner *async* call
    // 3. browser sends sync call again in there. (from port B, not A)
    private List xhrSessions = new ArrayList();

    private HttpServer httpServer;
    private String sessionId;

    public HttpTransport (String sid, HttpServer srv) {
        sessionId = sid;
        httpServer = srv;
        touchTimeout();
    }

    public void setWebSocket (HttpReactor ws) {
        if (webSocket != null) {
            throw new IllegalStateException("websocket already exist");
        }
        webSocket = ws;

        try {
            // flush pending messages
            synchronized (registry) {
                if (pendingMessages == null) return;
                Iterator i = pendingMessages.iterator();
                pendingMessages = null;
                while (i.hasNext()) {
                    ws.sendMessage((Buffer)i.next());
                }
            }
        } catch (IOException e) {
            e.printStackTrace();
            close();
        }
    }

    public boolean hasWebSocket () {
        return webSocket != null;
    }

    private XhrSession getXhrSession (int portId) {
        for (int i = 0; i < xhrSessions.size(); i++) {
            XhrSession sess = (XhrSession)xhrSessions.get(i);
            if (sess.portId == portId) {
                return sess;
            }
        }
        return null;
    }

    // send a pending response, or keep xhr for later use.
    private void sendNextResponse (HttpReactor xhr, XhrSession sess) {
        if (sess.response == null) {
            // keep xhr
            xmlHttpRequest = xhr;
            currentXhrSession = sess;
            return;
        }

        try {
            xhr.send(sess.nextResponse());
            xhr.endResponse();
        } catch (IOException e) {
            xhr.close(e);
            close();
        }

        if (sess.callDepth == 0 && sess.response == null) {
            xhrSessions.remove(sess);
        }
    }

    // this function doesn't call registry.recv() in order to handle xhr.
    private void handleMessage (Buffer msg, HttpReactor xhr) {
        //msg.dump();

        // read pid first.
        if (msg.size() < 4) {
            throw new RemoteException("Empty message");
        }
        int pid = -msg.readI32();
        XhrSession sess = null;

        synchronized (registry) {

            // xhr specific
            if (xhr != null) {

                // previous connection must be null
                if (xmlHttpRequest != null) {
                    xhr.close("Concurrent XHR detected.");
                    return;
                }

                sess = getXhrSession(pid);

                // browser may send xhr with port number only
                // to get next response.
                if (msg.size() == 0) {
                    if (sess == null) {
                        throw new RemoteException(
                                "XHR Session was not yet established.");
                    }
                    sendNextResponse(xhr, sess);
                    return;
                }
            }

            if (msg.size() < 1) {
                throw new RemoteException("Empty message on port:"+pid);
            }
            Port p = registry.getPort(pid);
            int h = msg.readI8() & 0xff;

            if (I) Log.info("reading "+(msg.size()+1)+" bytes from port:"+pid+
                    " head:"+h);

            // establish an xhr session if not found.
            if (xhr != null && sess == null) {
                sess = new XhrSession();
                sess.portId = pid;
                xhrSessions.add(sess);
            }

            switch (h >>> 6) {
            case Port.MESSAGE_RETURN:
                {
                    RemoteCall rc = p.removeRemoteCall();
                    Buffer buf2 = rc.buffer;
                    buf2.writeI8(h);
                    buf2.writeBuffer(msg);
                    rc.scan();
                    rc.isValid = true;
                    p.notifyRead();

                    if (xhr != null) {
                        sess.callDepth--;
                        // browser sent a return, which implies that
                        // initial xhr request was not yet finished.
                        if (sess.callDepth <= 0) {
                            throw new RemoteException(
                                    "XHR call depth mismatch");
                        }
                    }
                    break;
                }

            case Port.MESSAGE_SYNC_CALL:
                if (xhr != null) {
                    sess.callDepth++;
                }
                // fall through
            case Port.MESSAGE_ASYNC_CALL:
            case Port.MESSAGE_CHAINED_CALL:
                {
                    LocalCall lc = (LocalCall)New.get(LocalCall.class);
                    if (msg.size() < 4) {
                        throw new RemoteException("Too short header");
                    }
                    int oid = -msg.readI32();
                    if (D) Log.debug("oid:"+oid);
                    Buffer buf2 = lc.init(h, oid, registry);
                    buf2.writeBuffer(msg);
                    lc.scan();
                    p.addLocalCall(lc);
                    break;
                }

            default: return; // never happens
            }

            // close xhr request if complete.
            if (xhr != null) {
                // send dummy response, and close it.
                if (sess.callDepth == 0) {
                    xhrSessions.remove(sess);
                    try {
                        xhr.sendText(200, "");
                    } catch (IOException e) {
                        xhr.close(e);
                    }
                    return;
                }
                sendNextResponse(xhr, sess);
            }
        }
    }

    public void wait (Port p) {
        synchronized (this) {
            if (closed) {
                throw new RemoteException("Transport was closed");
            }
        }
        p.waitRead();
    }

    public void send (Port p, Buffer buf) {
        try {
            _send(p, buf);
        } catch (IOException e) {
            e.printStackTrace();
            throw new RemoteException("IO Error", e);
        }
    }

    private void _send (Port p, Buffer buf) throws IOException {

        buf.preWriteI32(p.id);

        synchronized (registry) {
            registry.checkDisposed();

            // if the port is attached with a xhr session
            // send through the xhr.
            XhrSession sess = getXhrSession(p.id);
            if (sess != null) {

                // trace call depth.
                // 0 means that there are no additional response via xhr.
                switch ((buf.peek(4) & 0xff) >> 6) {
                case Port.MESSAGE_SYNC_CALL:
                    sess.callDepth++;
                    break;
                case Port.MESSAGE_RETURN:
                    sess.callDepth--;
                    break;
                }

                // construct send buffer
                Buffer sb = new Buffer();
                int len = (buf.size() + 2) / 3 * 4;
                sb.writeRawString("HTTP/1.1 200 OK\r\n" +
                        "Content-Type: text/plain\r\n" +
                        "Content-Length: "+len+"\r\n" +
                        "Access-Control-Allow-Origin: *\r\n" +
                        "\r\n");
                sb.writeBase64(buf);

                if (sess == currentXhrSession && sess.response == null) {
                    // directly send the response
                    xmlHttpRequest.send(sb);
                    xmlHttpRequest.endResponse();
                    xmlHttpRequest = null;
                    currentXhrSession = null;

                    // close xhr session
                    if (sess.callDepth == 0) {
                        xhrSessions.remove(sess);
                    }
                } else {
                    sess.addResponse(sb);
                }
            } else { // via websocket
                if (webSocket != null) {
                    webSocket.sendMessage(buf);
                } else {
                    if (pendingMessages == null) {
                        pendingMessages = new ArrayList();
                    }
                    pendingMessages.add(new Buffer(buf));
                }
            }
        }

        buf.clear();
    }

    void close () {
        synchronized (this) {
            closed = true;
        }

        synchronized (registry) {
            if (webSocket != null) {
                webSocket.close("bound transport was closed");
                webSocket = null;
            }
            if (xmlHttpRequest != null) {
                xmlHttpRequest.close("bound transport was closed");
                xmlHttpRequest = null;
            }
            registry.dispose();
        }
    }

    private Buffer messageBuffer = new Buffer();

    public void handleHttpRequest (HttpReactor r) {
        // decode base64 content
        messageBuffer.clear();
        r.getContent().readBase64(messageBuffer);
        handleMessage(messageBuffer, r);
    }

    public void handleWebsocketOpen (HttpReactor r) {
    }

    public void handleWebsocketMessage (HttpReactor r) {
        handleMessage(r.getContent(), null);
    }

    public void handleWebsocketTextMessage (HttpReactor r) {
        messageBuffer.clear();
        r.getContent().readBase64(messageBuffer);
        handleMessage(messageBuffer, null);
    }

    public void handleWebsocketClose (HttpReactor r) {
        if (I) Log.info("closing transport:" + this);
        close ();
        httpServer.transportClosed(sessionId);
    }

    private static final long TIMEOUT = 60*1000;

    public long timeout;
    public void touchTimeout () {
        timeout = NativeIo.getMonotonicTime() + TIMEOUT;
    }
    public boolean isStale () {
        return (webSocket == null && (
                NativeIo.getMonotonicTime() - timeout > 0));
    }

    public String toString () {
        return "HttpTransport("+sessionId+")";
    }
}
