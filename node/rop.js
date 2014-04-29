acba.define('js:rop',
['log:rop', 'js:rop_base', 'js:rop_buffer'],
function (log, rop, RopBuffer) {

    //log.level = log.TRACE

    var HTTP_TRANSPORT_TIMEOUT = 60 * 1000

    var net = require('net')
    var http = require('http')
    var url = require('url')
    var path = require('path')
    var fs = require('fs')
    var ws = require('ws')
    var crypto = require('crypto')

    var encode64 = function (msg) {
        return new Buffer(msg).toString('base64')
    }

    var decode64 = function (msg) {
        return new Buffer(msg, 'base64').toString('binary')
    }

    var HttpServer = function (port) {
        var self = this
        this.transports = {}
        var createSessionId = function () {
            return crypto.randomBytes(24).toString('base64').
                    replace(/[/+]/g, '')
        }

        log.debug('HttpServer is listening on port '+port)
        var hs = http.createServer().listen(port)
        hs.on('request', function(request, response) {
            var uri = url.parse(request.url).pathname
            log.debug('http req:'+uri)

            if (uri.match(/^\/r($|\/)/)) {
                handleHttpRequest(uri.substring(2), request, response)
                return
            }
            sendFile(uri, request, response)
        })

        var wss = new ws.Server({server: hs});
        wss.on('connection', function(ws) {
            var uri = url.parse(ws.upgradeReq.url).pathname
            if (!uri.match(/^\/r/)) {
                ws.close()
                return
            }
            uri = uri.substring(2)

            // check stale transports on this callback.
            // (can be moved to other place if necessary)
            var now = Date.now()
            for (var sid in self.transports) {
                var trans = self.transports[sid]
                if (!trans.__io.websocket && now - trans.__timeout > 0) {
                    log.info("Closing stale transport:"+sid)
                    trans.close()
                    delete self.transports[sid]
                }
            }

            var sid, trans
            if (uri.length > 0 && uri[0] === '/') {
                // use already existing transport
                sid = uri.substring(1)
                trans = self.transports[sid]
                if (!trans || trans.__io.websocket) {
                    ws.close()
                    return
                }
            } else {
                // create websocket-initiated transport
                sid = createSessionId()
                trans = createHttpTransport(sid)
                self.transports[sid] = trans
            }

            console.log(new Buffer(sid, 'binary'))
            ws.send(new Buffer(sid, 'binary'))
            trans.__io.websocket = ws
            ws.onmessage = function (e) {
                trans.__io.onmessage(new RopBuffer(e.data))
            }
            ws.onclose = function (e) {
                trans.__io.onclose()
                delete self.transports[sid]
            }
            trans.__io.flushPendingMessages() // not part of IO interface
        })

        var handleHttpRequest = function (path, request, response) {

            // Create xhr-initiated transport
            if (path === '') {
                var sid = createSessionId()
                var trans = createHttpTransport(sid)
                trans.__timeout = Date.now() + HTTP_TRANSPORT_TIMEOUT
                self.transports[sid] = trans
                response.writeHead(200, {
                    'Content-Type': 'text/plain',
                    'Access-Control-Allow-Origin': '*'
                })
                response.write(sid)
                response.end()
                log.info('Created xhr-initiated transport:'+sid)
                return
            }

            // otherwise, treat as rop over xhr.
            var sid = path.substring(1)
            var trans = self.transports[sid]
            if (!trans) {
                response.writeHead(404, {'Access-Control-Allow-Origin': '*'})
                response.end()
                return
            }
            trans.__timeout = Date.now() + HTTP_TRANSPORT_TIMEOUT

            var bufs = []

            request.on('data', function (data) {
                bufs.push(data)
            })
            request.on('end', function () {
                var data = Buffer.concat(bufs)
                var buf = new RopBuffer(new Buffer(data.toString(), 'base64'))
                trans.__io.onmessage(buf, function (buf) {
                    if (buf === undefined) {
                        response.writeHead(200,
                                {'Access-Control-Allow-Origin': '*'})
                        response.end()
                        return
                    }
                    response.writeHead(200,
                            {'Access-Control-Allow-Origin': '*'})
                    response.end(buf.readSubbuffer().toString('base64'))
                })
            })
        }

        var getContentType = function (ext) {
            switch (ext.toLowerCase()) {
            case '.html': return "text/html"
            case '.js': return "text/javascript"
            case '.css': return "text/css"
            case '.xml': return "text/xml"
            case '.jpg': case '.jpeg': return "image/jpeg"
            case '.gif': return "image/gif"
            case '.png': return "image/png"
            case '.ico': return "image/vnd.microsoft.icon"
            case '.svg': return "image/svg+xml"
            default: return "text/plain"
            }
        }

        var basedir = path.join(process.cwd(), 'js') + '/'
        var sendFile = function (uri, request, response) {
            var filepath = path.join(basedir, uri)
            if (filepath.slice(0, basedir.length) !== basedir) {
                filepath = '/non_existing_uri'
            }
            fs.stat(filepath, function(err, stats) {
                if (stats && stats.isDirectory()) {
                    filepath = path.join(filepath, 'index.html')
                    stats = fs.statSync(filepath)
                }

                if(!stats || !stats.isFile()) {
                    response.writeHead(404, {
                            'Content-Type': 'text/plain',
                            'Access-Control-Allow-Origin': '*'})
                    response.write("404 Not Found\n");
                    response.end();
                    return;
                }

                fs.readFile(filepath, "binary", function(err, file) {
                    if (err) {
                        response.writeHead(500, {
                                'Content-Type': 'text/plain',
                                'Access-Control-Allow-Origin': '*'})
                        response.write(err + "\n");
                        response.end();
                        return;
                    }

                    response.writeHead(200, {
                        'Content-Type': getContentType(path.extname(uri)),
                        'Access-Control-Allow-Origin': '*'
                    });
                    response.write(file, "binary");
                    response.end();
                });
            });
        }

        var createHttpTransport = function (sid) {

            var pendingMessages = []

            var io = {
                onmessage: function () {},
                onclose: function () {
                    log.warn('Connection closed, but not handled.')
                },
                send: function (buf) {
                    var msg = buf.readSubbuffer().toString('base64')
                    if (pendingMessages) {
                        pendingMessages.push(msg)
                        return
                    }
                    this.websocket.send(msg)
                },
                close: function () {
                    if (this.websocket) this.websocket.close()
                },
                flushPendingMessages: function () { // not part of IO interface
                    for (var i = 0; i < pendingMessages.length; i++) {
                        this.websocket.send(pendingMessages[i])
                    }
                    pendingMessages = undefined
                },
            }
            return new rop.AbstractTransport({io:io})
        }
    }

    var SocketServer = function (port) {
        var ss = net.createServer()
        ss.listen(port, function () {
            log.debug('SocketServer is listening on port '+port)
        })

        ss.on('connection', function (conn) {
            log.info('accept socket connection:'+conn)
            var trans = new SocketTransport({connection:conn})
        })
    }

    var SocketTransport = rop.defineClass(rop.AbstractTransport, {
        init: function (opts) {
            var self = this

            if (typeof(opts) === 'string') {
                opts = {
                    host: opts
                }
            }

            var conn = opts.connection
            if (!conn) {
                var hostname, por
                if (opts.host) {
                    var sp = opts.host.split(':')
                    hostname = sp[0]
                    port = sp[1]
                } else {
                    hostname = opts.hostname
                    port = opts.port
                }
                conn = net.connect(port, hostname)

                // hold deferred messages until connected
                this.pendingMessages = []
                conn.on('connect', function () {
                    var msgs = self.pendingMessages
                    if (!msgs) return
                    self.pendingMessages = undefined
                    for (var i = 0; i < msgs.length; i++) {
                        io.send(msgs[i])
                    }
                    if (self.onopen) self.onopen()
                })
            }

            var inbuf = new RopBuffer()
            var msgsz = undefined
            conn.on('data', function (msg) {
                inbuf.writeBuffer(msg)
                while (true) {
                    if (msgsz === undefined) {
                        if (inbuf.size() < 4) return
                        msgsz = inbuf.readI32()
                    }

                    if (inbuf.size() < msgsz) return
                    var subbuf = inbuf.readSubbuffer(msgsz)
                    msgsz = undefined
                    io.onmessage(new RopBuffer(subbuf))
                }
            })

            conn.on('error', function (e) {
                log.warn('Socket got error:'+e)
                if (self.onerror) self.onerror()
            })
            conn.on('close', function (err) {
                log.info('Socket closed. err?:'+err)
                io.onclose()
                if (self.onclose) self.onclose()
            })

            var lenbuf = new Buffer(4)
            var io = {
                onmessage: function () {},
                onclose: function () {
                    log.warn('Connection closed, but not handled.')
                },
                send: function (msg) {
                    if (self.pendingMessages) {
                        self.pendingMessages.push(new RopBuffer(msg))
                        return
                    }

                    var msgbuf = msg.readSubbuffer()
                    lenbuf.writeInt32BE(msgbuf.length, 0)
                    conn.write(lenbuf)
                    conn.write(msgbuf)
                },
                close: function () {
                    conn.close()
                }
            }
            SocketTransport.super.init.call(this, {
                io: io,
                mode: opts.mode
            })
        }
    })

    var StubList = rop.defineClass({
        init: function () {
            this.list = []
        },
        add: function (s) {
            this.list.push(s)
            if (s.__isStub) {
                s.addStubListener(this)
            }
        },
        remove: function (s) {
            for (var i = 0; i < this.list.length; i++) {
                if (this.list[i] === s) {
                    this.list.splice(i, 1)
                    if (s.__isStub) {
                        s.removeStubListener(this)
                    }
                    return
                }
            }

            if (!s.__isStub) return
            var r = s.remote

            // find other stub which has the same remote object.
            for (var i = 0; i < this.list.length; i++) {
                var s = this.list[i]
                if (s.remote === r) {
                    this.list.splice(i, 1)
                    s.removeStubListener(this)
                    return
                }
            }
        },
        stubRevoked: function (s) {
            this.remove(s)
        }
    })

    var StubSetter = rop.defineClass({
        init: function (obj, setter) {
            this.object = obj
            this.setter = setter
            this.stub = undefined
        },
        set: function (s) {
            if (this.stub) {
                this.stub.removeStubListener(this)
                this.stub = undefined
            }
            this.setter.call(this.object, s, false)
            if (!rop.isStub(s)) return
            this.stub = s
            s.addStubListener(this)
        },
        stubRevoked: function (s) {
            this.stub = undefined
            this.setter.call(this.object, undefined, true)
        }
    })

    rop.HttpServer = HttpServer
    rop.SocketServer = SocketServer
    rop.SocketTransport = SocketTransport
    rop.StubList = StubList
    rop.StubSetter = StubSetter
    return rop
})
