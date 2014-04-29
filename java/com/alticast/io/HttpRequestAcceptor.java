package com.alticast.io;

import java.io.*;

/**
 * Web service container like servlet.
 */
public interface HttpRequestAcceptor {
    /**
     * Provide handler for given request.
     * HttpRequestHandler can handle both of http request and websocket.
     * If you need to support only one of them, check r.isWebsocket()
     * and return null for unsupported request type.
     * @arg basePath is where this acceptor is serving on.
     */
    HttpRequestHandler acceptHttpRequest (String basePath, HttpReactor r);
}
