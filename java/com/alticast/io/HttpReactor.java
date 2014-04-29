package com.alticast.io;

import java.io.*;
import java.security.*;
import java.util.*;
import com.alticast.rop.*;

public class HttpReactor extends BufferedFileReactor {

    private static final int BEGIN_READ = 0;
    private static final int READ_HEADS = 1;
    private static final int TRY_UPGRADE = 2;
    private static final int READ_BODY = 3;
    private static final int READ_WEBSOCKET = 4;
    private static final int READ_WEBSOCKET_PAYLOAD = 5;
    private static final int READ_WEBSOCKET_76 = 6;
    private static final int READ_WEBSOCKET_PAYLOAD_76 = 7;

    protected static final int OPCODE_CONTINUATION = 0x00;
    protected static final int OPCODE_TEXT = 0x01;
    protected static final int OPCODE_BINARY = 0x02;
    protected static final int OPCODE_CLOSE = 0x08;
    protected static final int OPCODE_PING = 0x09;
    protected static final int OPCODE_PONG = 0x0a;

    public static String getStatusName (int sc) {
        switch (sc) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 402: return "Payment Required";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        default: return "Unknown Status";
        }
    }

    public static String guessContentType (String path) {
        int dot = path.lastIndexOf(".");
        if (dot == -1) return "text/plain";
        String ext = path.substring(dot);

        if (ext.equals(".html")) {
            return "text/html";
        } else if (ext.equals(".js")) {
            return "text/javascript";
        } else if (ext.equals(".css")) {
            return "text/css";
        } else if (ext.equals(".xml")) {
            return "text/xml";
        } else if (ext.equals(".jpg") || ext.equals(".jpeg")) {
            return "image/jpeg";
        } else if (ext.equals(".gif")) {
            return "image/gif";
        } else if (ext.equals(".png")) {
            return "image/png";
        } else if (ext.equals(".ico")) {
            return "image/vnd.microsoft.icon";
        } else if (ext.equals(".svg")) {
            return "image/svg+xml";
        } else {
            return "text/plain";
        }
    }

    private int state = BEGIN_READ;

    protected String requestLine;
    protected String requestMethod; // GET, POST, ...
    protected String requestUri; // /xxx/index.html?a=b
    protected String requestPath; // /xxx/index.html
    protected String requestQuery; // a=b
    protected String requestProtocol; // HTTP/1.1
    protected Map headers = new HashMap();
    protected int contentLength;
    protected Buffer contentBuffer; // sub-buffer from inBuffer

    public String getRequestMethod () { return requestMethod; }
    public String getRequestUri () { return requestUri; }
    public String getRequestPath () { return requestPath; }
    public String getRequestQuery () { return requestQuery; }
    public String getRequestHandlerPath () { return requestHandlerPath; }
    public String getRequestProtocol () { return requestProtocol; }
    public Buffer getContent () { return contentBuffer; }
    public String getHeader (String key) { return (String)headers.get(key); }
    public boolean isWebsocket () { return websocketVersion != 0; }

    // 0:not websocket, +:version, -:hixie version
    protected int websocketVersion;
    protected int opcode;

    // temporal data
    private byte[] key3 = new byte[8];
    private byte[] frameMask = new byte[4];

    protected final HttpServerReactor serverReactor;
    protected HttpRequestHandler requestHandler;
    protected String requestHandlerPath; // /xxx/index.html

    public HttpReactor () {
        this(null, false);
    }

    public HttpReactor (HttpServerReactor srv, boolean s) {
        super(s);
        serverReactor = srv;
    }

    /** Read http request. May override. */
    protected void readRequest () {
        if (Log.D) dumpRequest();

        if (serverReactor != null) {
            serverReactor.updateRequestHandler(this);
        }

        if (requestHandler == null) {
            close("No handler for the request:"+getRequestUri());
            return;
        }

        if (isWebsocket()) {
            requestHandler.handleWebsocketOpen(this);
        } else {
            requestHandler.handleHttpRequest(this);
        }
    }

    void setRequestHandler (HttpRequestHandler hrh, String bp) {
        requestHandler = hrh;
        if (hrh == null || bp == null) return;
        requestHandlerPath = requestPath.substring(bp.length());
    }

    /** Read websocket frame. Need override. */
    protected void readFrame () {
        if (requestHandler == null) {
            close("No handler for the websocket connection");
            return;
        }

        if (opcode == OPCODE_TEXT) {
            requestHandler.handleWebsocketTextMessage(this);
        } else {
            requestHandler.handleWebsocketMessage(this);
        }
    }

    public void postClose () {
        if (requestHandler == null) return;
        if (isWebsocket()) requestHandler.handleWebsocketClose(this);
        requestHandler = null;
    }

    private boolean readingRequest = false;
    private boolean responseEnded = false;

    private void _readRequest () throws IOException {
        readingRequest = true;
        readRequest();
        readingRequest = false;
        synchronized (this) {
            if (responseEnded) {
                responseEnded = false; // re-initialize
            } else {
                setReadable(false); // stop reading until ended
            }
        }
    }

    public void endResponse () {
        if (responseEnded) return;
        responseEnded = true;

        EventDriver ed = getEventDriver();
        if (ed == null) return; // if already unwatching.
        if (ed.isEventThread() && readingRequest) return;
        ed.setTimeout(0, this);
    }

    public void processEvent (int events) {
        if ((events & EventDriver.EVENT_TIMEOUT) != 0 && responseEnded) {
            responseEnded = false;
            setReadable(true);
            events &= ~EventDriver.EVENT_TIMEOUT;
            events |= EventDriver.EVENT_READ;
        }
        super.processEvent(events);
    }

    // send websocket binrary message.
    // hixie-76 sends base64-encoded string since it lacks binrary support.
    public synchronized void sendMessage (Buffer buf) throws IOException {
        if (websocketVersion < 0) {
            // send base64-encoded string as hixie-76 frame
            outBuffer.writeI8(0x00);
            outBuffer.writeBase64(buf);
            outBuffer.writeI8(0xFF);
        } else {
            // work-around for WEBMW-325
            if (false) {
                int plen = (buf.size() + 2) / 3 * 4;
                outBuffer.writeI8(0x80 | OPCODE_TEXT);
                if (plen < 126) {
                    outBuffer.writeI8(plen);
                } else if (plen < 65536) {
                    outBuffer.writeI8(126);
                    outBuffer.writeI16(plen);
                } else {
                    outBuffer.writeI8(127);
                    outBuffer.writeI64(plen);
                }
                outBuffer.writeBase64(buf);
                sendBuffer();
                return;
            }

            int plen = buf.size();
            outBuffer.writeI8(0x80 | OPCODE_BINARY);
            if (plen < 126) {
                outBuffer.writeI8(plen);
            } else if (plen < 65536) {
                outBuffer.writeI8(126);
                outBuffer.writeI16(plen);
            } else {
                outBuffer.writeI8(127);
                outBuffer.writeI64(plen);
            }
            outBuffer.writeBuffer(buf, plen);
        }
        sendBuffer();
    }

    // send websocket message as string.
    public synchronized void sendMessage (String msg) throws IOException {

        // make utf8 encodied bytes
        byte[] utf8str;
        try {
            utf8str = msg.getBytes("UTF8");
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            return; // not expected.
        }

        if (websocketVersion < 0) {
            outBuffer.writeI8(0x00);
            outBuffer.writeBytes(utf8str);
            outBuffer.writeI8(0xFF);
        } else {
            int plen = utf8str.length;
            outBuffer.writeI8(0x80 | OPCODE_TEXT);
            if (plen < 126) {
                outBuffer.writeI8(plen);
            } else if (plen < 65536) {
                outBuffer.writeI8(126);
                outBuffer.writeI16(plen);
            } else {
                outBuffer.writeI8(127);
                outBuffer.writeI64(plen);
            }
            outBuffer.writeBytes(utf8str);
        }
        sendBuffer();
    }

    public void sendText (int sc, String msg) throws IOException {
        try {
            byte[] buf = msg.getBytes("UTF8");
            sendResponse(sc, new String[] {
                "Content-Type: text/plain; charset=utf-8",
                "Content-Length: "+buf.length
            }, buf);
        } catch (UnsupportedEncodingException e) {
            sendResponse(sc, new String[] {
                "Content-Type: text/plain",
                "Content-Length: "+msg.length()
            }, msg);
        }
    }

    public void sendHead (int status, String[] headers) throws IOException {
        StringBuffer sb = new StringBuffer();
        sb.append("HTTP/1.1 ");
        sb.append(status);
        sb.append(" ");
        sb.append(getStatusName(status));
        sb.append("\r\n");
        for (int i = 0; i < headers.length; i++) {
            String h = headers[i];
            if (h == null) continue;
            sb.append(h);
            sb.append("\r\n");
        }
        //sb.append("connection: keep-alive\r\n");
        sb.append("Access-Control-Allow-Origin: *\r\n");
        sb.append("\r\n");
        send(sb);
    }

    public void sendResponse (int status, String[] headers, Object body)
            throws IOException {
        sendHead(status, headers);
        if (body != null) send(body);
        endResponse();
    }

    public void sendFile (String path) throws IOException {
        sendFile(path, null);
    }

    public void sendFile (String path, String[] heads) throws IOException {
        FileInputStream fis = null;
        Buffer fbuf = new Buffer();
        int sz;
        try {
            if (path.indexOf("../") >= 0) {
                throw new IOException("Path included '..'");
            }
            fis = new FileInputStream(path);
            sz = fis.available();
        } catch (IOException e) {
            e.printStackTrace();
            sendText(404, "Cannot read file:"+path);
            return;
        }

        synchronized (this) {
            try {
                List headList = new ArrayList();
                headList.add("Content-Type: "+ guessContentType(path));
                headList.add("Content-Length: "+sz);
                if (heads != null) {
                    for (int i = 0; i < heads.length; i++) {
                        headList.add(heads[i]);
                    }
                }
                sendHead(200, (String[])headList.toArray(
                        new String[headList.size()]));
                outBuffer.readFrom(fis, sz);
                sendBuffer();

                endResponse();
            } finally {
                try {
                    fis.close();
                } catch (IOException e2) { }
            }
        }
    }

    public void dumpRequest () {
        Log.debug(requestLine);
        for (Iterator iter = headers.keySet().iterator(); iter.hasNext();) {
            Object k = iter.next();
            Object v = headers.get(k);
            Log.debug("HEAD "+k+": "+v);
        }
    }

    private boolean parseHeader () {
        // pre-check if inBuffer contains complete http heads.
        byte[] buf = inBuffer.getRawBuffer();
        int begin = inBuffer.getReadPosition();
        int end = inBuffer.getWritePosition();
        int i, j = 0; // j is count of successive '\n'
        for (i = begin; i < end; i++) {
            if (buf[i] == 0x0d) continue;
            if (buf[i] == 0x0a) {
                j++;
                if (j == 2) break;
            } else {
                j = 0;
            }
        }
        if (j != 2) {
            return false;
        }
        end = i + 1;
        inBuffer.drop(end - begin);

        int line = -1;
        int colon = -1;
        for (i = begin; i < end; i++) {
            if (line == -1) {
                if (buf[i] != (byte)'\r' && buf[i] != (byte)'\n') line = i;
                continue;
            }
            if (colon == -1 && buf[i] == (byte)':') {
                colon = i;
                continue;
            }
            if (buf[i] != (byte)'\r' && buf[i] != (byte)'\n') continue;
            
            // read lines
            try {
                if (requestLine == null) {
                    requestLine = new String(buf, line, i - line, "ASCII");
                    int first = requestLine.indexOf(' ');
                    int second = requestLine.indexOf(' ', first + 1);
                    requestMethod = requestLine.substring(0, first);
                    requestUri = requestLine.substring(first + 1, second);
                    {
                        int query = requestUri.indexOf('?');
                        if (query == -1) {
                            requestPath = requestUri;
                            requestQuery = "";
                        } else {
                            requestPath = requestUri.substring(0, query);
                            requestQuery = requestUri.substring(query + 1);
                        }
                        requestHandlerPath = requestPath;
                    }
                    requestProtocol = requestLine.substring(second + 1);
                    continue;
                }

                if (colon == -1) {
                    if (W) {
                        Log.warn("header field without colon:" + new String(
                                buf, line, i - line, "ASCII"));
                    }
                    continue;
                }

                String k = new String(buf, line, colon - line,
                        "ASCII").toLowerCase();
                do { colon++; } while (buf[colon] == (byte)' ');
                String v = new String(buf, colon, i - colon);
                headers.put(k,v);

                if (k.equals("content-length")) {
                    contentLength = Integer.parseInt(v);
                }
            } catch (Exception e) {
                e.printStackTrace();
                // should not happen.
            } finally {
                line = -1;
                colon = -1;
            }
        }
        return true;
    }

    // return true if handshake processing is over.
    private boolean handshake () throws IOException {
        if (!"websocket".equalsIgnoreCase(getHeader("upgrade"))) {
            return true;
        }

        // determine websocket version
        String v = getHeader("sec-websocket-version");
        if (v != null) {
            websocketVersion = Integer.parseInt(v);
            if(websocketVersion < 4) {
                websocketVersion = -76; // hixie-76 request format.
            }
        } else {
            websocketVersion = -76;
        }
        if (D) Log.debug("web socket version = " + websocketVersion);

        if (websocketVersion < 0) { // hixie-76

            // try to read key3
            if (inBuffer.size() < 8) return false;
            inBuffer.readBytes(key3, 0, 8);

            byte[] content, keyBVal1, keyBVal2;
            byte[] keyCombination = new byte[16];
            StringBuffer responseHead = new StringBuffer();
            String keyVal1 = getHeader("sec-websocket-key1");
            String keyVal2 = getHeader("sec-websocket-key2");

            if(keyVal1 == null || keyVal2 == null || key3.length != 8) {
                if (W) Log.warn("handshake (hixie 76) failed.  key1:" + keyVal1
                        + " key2:" + keyVal2 + " key3:" + key3);
                return false;
            }

            keyBVal1 = getDigits76(keyVal1);
            keyBVal2 = getDigits76(keyVal2);

            System.arraycopy(keyBVal1, 0, keyCombination, 0, 4);
            System.arraycopy(keyBVal2, 0, keyCombination, 4, 4);
            System.arraycopy(key3, 0, keyCombination, 8, 8);

            try {
                MessageDigest md5 = MessageDigest.getInstance("MD5");
                md5.update(keyCombination);
                content = md5.digest();
            } catch (Exception e) {
                e.printStackTrace();
                return false;
            }

            String origin = getHeader("origin");
            String host = getHeader("host");
            String protocol = getHeader("sec-websocket-protocol");

            responseHead.append("HTTP/1.1 101 Switching Protocols\r\n" +
                    "Upgrade: websocket\r\n" +
                    "Connection: Upgrade\r\n");
            responseHead.append("Sec-WebSocket-Origin: " + origin + "\r\n");
            responseHead.append("Sec-WebSocket-Location: " +
                    "ws://" + host + requestUri + "\r\n");
            if(protocol != null) { // optional
                responseHead.append("Sec-WebSocket-Protocol: " + protocol + "\r\n");
            }
            responseHead.append("\r\n");

            byte[] rhBytes = responseHead.toString().getBytes();
            byte[] response = new byte[rhBytes.length + content.length];
            System.arraycopy(rhBytes, 0, response, 0, rhBytes.length);
            System.arraycopy(content, 0, response, rhBytes.length, content.length);
            send(response);
        } else {
            // RFC6455
            try {
                String key = getHeader("sec-websocket-key");

                MessageDigest md = MessageDigest.getInstance("SHA-1");
                byte[] digest = md.digest(
                        (key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").
                        getBytes("iso-8859-1"));
                if (D) Log.debug("sha1 digest returns legnth:"+digest.length);
                byte[] encoded = new byte[28];
                Base64.encode(digest, 0, encoded, 0, 20);
                send("HTTP/1.1 101 Switching Protocols\r\n" +
                        "Upgrade: websocket\r\n" +
                        "Connection: Upgrade\r\n" +
                        "Sec-WebSocket-Accept: " +
                        new String(encoded) + "\r\n\r\n");
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
        return true;
    }

    private byte[] getDigits76 (String key) {
        char each;
        int keyInt, spaceCount = 0;
        byte[] b = new byte[4];
        StringBuffer intStr = new StringBuffer();

        for(int i = 0; i < key.length(); i++) {
            each = key.charAt(i);
            if(Character.isDigit(each)) {
                intStr.append(each);
            } else if(Character.isWhitespace(each)) {
                spaceCount++;
            }
        }

        keyInt = (int)(Long.parseLong(intStr.toString(), 10) / spaceCount);
        for(int i = 0; i < 4; i++) {
            int offset = (b.length - 1 - i) * 8;
            b[i] = (byte)((keyInt >>> offset) & 0xff);
        }
        return b;
    }

    private boolean readWebSocketHeader () {
        if (inBuffer.size() < 2) return false;
        int h0 = inBuffer.peek(0);
        int h1 = inBuffer.peek(1);
        //if (D) Log.debug("h0:"+(h0&0xff)+", h1:"+(h1&0xff));
        //inBuffer.dump();

        opcode = h0 & 0x0f;
        int nread = 2;

        boolean mask = (h1 & 0x80) != 0;
        if (mask) {
            nread += 4;
        } else {
            close("received unmasked frame.");
            return false;
        }
        long plen = h1 & 0x7f;
        if (plen == 126) {
            nread += 2;
        } else if (plen == 127) {
            nread += 8;
        }
        if (inBuffer.size() < nread) return false;

        inBuffer.drop(2);
        if (plen == 126) {
            plen = (long)(inBuffer.readI16() & 0xffff);
        } else if (plen == 127) {
            plen = inBuffer.readI64();
        }
        inBuffer.readBytes(frameMask, 0, 4);
        contentLength = (int)plen; // don't care trunc...
        return true;
    }

    public void readBuffer () throws IOException {
        while (_readBuffer());
    }

    private void applyMask () {
        byte[] buf = contentBuffer.getRawBuffer();
        int rpos = contentBuffer.getReadPosition();
        int wpos = contentBuffer.getWritePosition();
        for (int i = rpos; i < wpos; i++) {
            buf[i] ^= frameMask[(i - rpos) % 4];
        }
    }

    private boolean _readBuffer () throws IOException {
        contentBuffer = null; // lazy clean-up

        switch (state) {
        case BEGIN_READ:
            contentLength = 0;
            requestLine = null;
            headers.clear();
            state = READ_HEADS;
            // fall through

        case READ_HEADS:
            if (!parseHeader()) return false;
            state = TRY_UPGRADE;
            // fall through

        case TRY_UPGRADE:
            if (!handshake()) return false;

            // if this is websocket
            if (websocketVersion != 0) {
                state = (websocketVersion > 0) ? READ_WEBSOCKET :
                        READ_WEBSOCKET_76;
                readRequest();
                return true;
            }

            // if body is empty.
            if (contentLength == 0) {
                state = BEGIN_READ;
                _readRequest();
                return true;
            }

            state = READ_BODY;
            // fall through

        case READ_BODY:
            if (inBuffer.size() < contentLength) return false;
            contentBuffer = inBuffer.readBuffer(contentLength);
            state = BEGIN_READ;
            _readRequest();
            return true;

        case READ_WEBSOCKET:
            if (!readWebSocketHeader()) return false;
            state = READ_WEBSOCKET_PAYLOAD;
            // fall through

        case READ_WEBSOCKET_PAYLOAD:
            if (inBuffer.size() < contentLength) return false;
            contentBuffer = inBuffer.readBuffer(contentLength);
            applyMask();
            state = READ_WEBSOCKET;
            switch (opcode) {
            case OPCODE_TEXT:
            case OPCODE_BINARY:
                readFrame();
                return true;

            case OPCODE_CLOSE:
                close("websocket closed by peer.");
                return false;

            case OPCODE_PING:
                if (W) Log.warn("ping not supported.");
                return true;

            case OPCODE_PONG:
                if (W) Log.warn("pong not supported.");
                return true;

            default:
                close("unknown opcode:"+opcode);
                return false;
            }

        case READ_WEBSOCKET_76:
            if (inBuffer.size() < 1) return false;
            switch ((byte)inBuffer.peek(0)) {
            case (byte)0xff: // close request
                if (inBuffer.size() < 2) return false;
                if ((byte)inBuffer.peek(1) == (byte)0x00) {
                    close("web socket (hixie-76) closed by peer.");
                    return false;
                }
                // fall through

            default:
                close("Invalid frame");
                return false;

            case (byte)0x00: // normal frame
                state = READ_WEBSOCKET_PAYLOAD_76;
                inBuffer.drop(1);
                contentLength = 0;
                break;
            }
            // fall through

        case READ_WEBSOCKET_PAYLOAD_76:
            while (true) {
                if (inBuffer.size() <= contentLength) return false;
                if ((byte)inBuffer.peek(contentLength) == (byte)0xff) break;
                contentLength++;
            }

            opcode = OPCODE_TEXT;
            contentBuffer = inBuffer.readBuffer(contentLength);
            inBuffer.drop(1);
            state = READ_WEBSOCKET_76;
            readFrame();
            return true;

        default:
            throw new IllegalStateException("Fallen into wrong state");
        }
    }
}
