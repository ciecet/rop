package com.alticast.chat;

import java.util.*;
import java.io.*;
import com.alticast.io.*;

public class ChatServer extends AbstractHttpRequestHandler {

    private Map clients = new HashMap();
    private int nextId = 0;

    public void handleHttpRequest (HttpReactor r) {
        StringBuffer sb = new StringBuffer();
        sb.append("<html><body>");
        sb.append("<input id='input' type='text' size='80'></input>");
        sb.append("<script>");
        sb.append("var input = document.getElementById('input');");
        sb.append("var ws = new WebSocket('ws://'+location.host+location.pathname);");
        sb.append("ws.onopen = function() { console.log('open') };");
        sb.append("ws.onclose = function() { console.log('closed') };");
        sb.append("ws.onerror = function() { console.log('error') };");
        sb.append("ws.onmessage = function(m) { var d = document.createElement('div'); d.innerHTML = m.data; document.body.insertBefore(d, input); input.scrollIntoView() };");
        sb.append("input.onkeypress = function(e) { if (e.keyCode !== 13) return; ws.send(this.value); this.value = '' };");
        sb.append("input.focus();");
        sb.append("</script></body></html>");

        try {
            r.sendResponse(200, new String[] {
                    "Content-type: text/html",
                    "Content-length: "+sb.length()
            }, sb);
        } catch (IOException e) {
            r.close(e);
        }
    }

    public void handleWebsocketOpen (HttpReactor r) {
        String name = r.getRequestHandlerPath();
        if (name.startsWith("/")) name = name.substring(1);
        if (name.length() == 0) {
            name = "anonymous#"+(nextId++);
        }
        clients.put(r, name);
        broadcast(name+" entered");
    }

    public void handleWebsocketTextMessage (HttpReactor r) {
        String msg = r.getContent().readString();
        broadcast("["+clients.get(r)+"]: "+msg);
    }

    private void broadcast (String msg) {
        for (Iterator iter = clients.keySet().iterator(); iter.hasNext(); ) {
            HttpReactor c = (HttpReactor)iter.next();
            try {
                c.sendMessage(msg);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }

    public void handleWebsocketClose (HttpReactor r) {
        String name = (String)clients.remove(r);
        broadcast(name+" exited");
    }

    public static void main (String[] args) {
        EventDriver ed = new EventDriver();
        HttpServerReactor hsrv = new HttpServerReactor();
        hsrv.bind("/chat", new ChatServer());
        try {
            hsrv.start(8888, ed);
        } catch (IOException e) {
            e.printStackTrace();
        }
        ed.run();
    }
}
