package com.alticast.io;

import java.io.*;

/**
 * Dummy implementation of HttpRequestAcceptor and HttpRequestHandler.
 */
public abstract class AbstractHttpRequestHandler implements
        HttpRequestAcceptor, HttpRequestHandler {

    public HttpRequestHandler acceptHttpRequest (String basePath,
            HttpReactor r) {
        return this;
    }

    public void handleHttpRequest (HttpReactor r) {
        try {
            r.sendText(501, "Implement me");
        } catch (IOException e) {
            r.close(e);
        }
    }

    public void handleWebsocketOpen (HttpReactor r) {
        r.close("Unsupported websocket open");
    }

    public void handleWebsocketMessage (HttpReactor r) {
        r.close("Unsupported websocket binrary message");
    }

    public void handleWebsocketTextMessage (HttpReactor r) {
        r.close("Unsupported websocket text message");
    }

    public void handleWebsocketClose (HttpReactor r) {}
}
