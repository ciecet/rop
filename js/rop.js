"use strict";
acba.define('js:rop',
['log:rop', 'js:rop_base', 'js:rop_buffer'],
function (log, rop, RopBuffer) {

    //log.level = log.TRACE

    var HttpTransport = rop.defineClass(rop.AbstractTransport, {
        webSocket: undefined,
        xhrUrl: undefined,
        onopen: function () {
            log.info("ROP transport is open.")
        },
        onclose: function () {
            log.info("ROP transport was closed.")
        },
        onerror: function () {
            log.info("Failed to open ROP transport")
        },
        init: function (opts, onopen, onclose, onerror) {
            var sessionId, onready, ws
            this.pendingMessages = []

            if (onopen || onclose || onerror) {
                log.warn("Passing callbacks via constructor is deprecated.")
                if (onopen) this.onopen = onopen
                if (onclose) this.onclose = onclose
                if (onerror) this.onerror = onerror
            }

            if (typeof(opts) === 'string') {
                opts = {
                    host: opts,
                    synchronous: false
                }
            }

            // create websocket
            if (opts.synchronous) {
                log.info("Opening xhr-initiated transport.")
                var req = new XMLHttpRequest()
                req.open("POST", "http://"+opts.host+"/r", false)
                req.setRequestHeader("Content-Type", "text/plain")
                req.send()
                if (req.status !== 200) {
                    throw "ROP Xhr failed. status:"+req.status
                }
                sessionId = req.responseText
                log.info('Got sid:'+sessionId)
                this.xhrUrl = "http://"+opts.host+"/r/"+sessionId
                ws = new WebSocket("ws://"+opts.host+"/r/"+sessionId)
            } else {
                log.info("Opening websocket-initiated transport.")
                ws = new WebSocket("ws://"+opts.host+"/r")
            }
            ws.binaryType = "arraybuffer"

            // open websocket
            var self = this
            ws.onopen = function () {
                log.debug("WebSocket open")
                // defer transport.onopen until session id arrives
            }
            ws.onclose = function () {
                log.warn("WebSocket closed before transport init")
                if (self.onerror) self.onerror()
            }
            ws.onerror = function (e) {
                console.error(e)
                log.warn("WebSocket failed to open")
                if (self.onerror) self.onerror()
            }

            // handle first message
            ws.onmessage = function (e) {
                if (e.data.constructor === ArrayBuffer) {
                    var sid = String.fromCharCode.apply(String,
                            new Uint8Array(e.data));
                } else {
                    var sid = atob(e.data)
                }
                window.e = e
                log.info('Websocket is established for transport:'+sid)
                if (sessionId) {
                    if (sessionId !== sid) {
                        log.warn("Got inconsistent transport id. expected:"+
                                sessionId + " got:"+ sid)
                        ws.close() // calls self.onerror() eventually
                        return
                    }
                } else {
                    sessionId = sid
                    self.xhrUrl = "http://"+opts.host+"/r/"+sessionId
                }
                self.webSocket = ws

                var pmsgs = self.pendingMessages
                delete self.pendingMessages
                if (pmsgs.length > 0) {
                    log.info('Sending pending '+pmsgs.length+' messages')
                    for (var i = 0; i < pmsgs.length; i++) {
                        io.send(pmsgs[i])
                    }
                }

                // replace callbacks
                ws.onclose = function () {
                    log.info("WebSocket closed")
                    io.onclose() // notify to rop runtime
                    if (self.onclose) self.onclose()
                }
                ws.onerror = function (e) {
                    log.warn('Error from websocket')
                    console.error(e)
                    if (self.onerror) self.onerror()
                }
                ws.onmessage = function (e) {
                    var msg = e.data
                    if (msg.constructor === String) {
                        msg = atob(msg)
                    }
                    io.onmessage(new RopBuffer(msg))
                }

                if (self.onopen) self.onopen()
            }

            var io = {
                onmessage: function () {},
                onclose: function () {},
                send: function (buf) {
                    if (!self.webSocket) {
                        self.pendingMessages.push(new RopBuffer(buf))
                        return
                    }

                    // work-around for WEBMW-325
                    // self.webSocket.send(btoa(String.fromCharCode.apply(
                    //        String, buf.readSubarray())))
                    self.webSocket.send(buf.readSubarray(buf.size()))
                },
                close: function () {
                    if (self.webSocket) self.webSocket.close()
                },
                exchange: function (buf) {
                    var req = new XMLHttpRequest()
                    if (self.xhrUrl === undefined) throw "connect websocket first."
                    req.open("POST", self.xhrUrl, false)
                    req.setRequestHeader("Content-Type", "text/plain")
                    var sa = buf.readSubarray()
                    req.send(btoa(String.fromCharCode.apply(String, sa)))
                    if (req.status !== 200) {
                        throw "ROP Xhr failed. status:"+req.status
                    }
                    var msg = req.responseText
                    if (msg === "") {
                          log.trace("got empty response from xhr (it's ok)")
                          return undefined
                    }
                    log.trace('response:'+msg)
                    return new RopBuffer(atob(msg))
                }
            }
            HttpTransport.super.init.call(this, {
                io: io,
                mode: opts.mode
            })
        }
    })

    rop.HttpTransport = HttpTransport
    rop.Transport = HttpTransport
    return rop
})
