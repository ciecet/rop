package com.alticast.io;

import java.io.*;

/**
 * Web service implementation per connection.
 * Note that all methods are called from event thread driving HttpReactor.
 */
public interface HttpRequestHandler {

    /**
     * Handle a http request.
     *
     * Following methods are relevant until you finish response:
     * r.getRequestMethod() - GET, POST, ...
     * r.getRequestUri() - /xxx/yyy/index.html?zzz
     * r.getRequestPath() - /xxx/yyy/index.html
     * r.getRequestQuery() - zzz
     * r.getRequestHandlerPath() - /yyy/index.html (if acceptor bound to /xxx)
     * r.requestProtocol - HTTP/1.1
     * r.getHeader(key_in_lower_case)
     * r.getContent() - body content
     * r.isWebsocket() - false always
     *
     * Response can be sent by one of following calls (mt-safe):
     * r.sendResponse(status, headers, body) or
     * r.sendHead(status, headers) / send(body) / endResponse() or
     * r.sendText(status, message)
     *
     * In errornous case, you can abort connection by using r.close().
     * It is mt-safe as well.
     */
    void handleHttpRequest (HttpReactor r);

    /**
     * Get notified when websocket connection was established.
     *
     * Following methods are relevant until closed:
     * r.getRequestMethod() - GET, POST, ...
     * r.getRequestUri() - /xxx/yyy/index.html?zzz
     * r.getRequestPath() - /xxx/yyy/index.html
     * r.getRequestQuery() - zzz
     * r.getRequestHandlerPath() - /yyy/index.html (if acceptor bound to /xxx)
     * r.getHeader(key_in_lower_case)
     * r.isWebsocket() - true always
     *
     * Message can be sent by using following calls until closed (mt-safe):
     * r.sendMessage(Buffer) - send binrary message
     * r.sendMessage(String) - send text message
     */
    void handleWebsocketOpen (HttpReactor r);

    /**
     * Handle a websocket binrary message.
     *
     * Following methods are relevant only within this call:
     * r.getContent() - messaage
     */
    void handleWebsocketMessage (HttpReactor r);

    /**
     * Handle a websocket text message.
     * Note that content is still in utf8 encoded bytes in content buffer.
     * String form can be acquired from content.readString().
     *
     * Following methods are relevant only within this call:
     * r.getContent() - messaage (utf8-encoded)
     */
    void handleWebsocketTextMessage (HttpReactor r);

    /**
     * Get notified when websocket connection was closed.
     */
    void handleWebsocketClose (HttpReactor r);
}
