var base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/="
var base64Inv = []
for (var i = 0; i<256; i++) {
    base64Inv[i] = base64Chars.indexOf(String.fromCharCode(i))
}

var encode64 = window.btoa || function(input) {
    var output = "";
    var chr1, chr2, chr3 = "";
    var enc1, enc2, enc3, enc4 = "";
    var i = 0;

    do {
        chr1 = input.charCodeAt(i++);
        chr2 = input.charCodeAt(i++);
        chr3 = input.charCodeAt(i++);

        enc1 = chr1 >> 2;
        enc2 = ((chr1 & 3) << 4) | (chr2 >> 4);
        enc3 = ((chr2 & 15) << 2) | (chr3 >> 6);
        enc4 = chr3 & 63;

        if (isNaN(chr2)) {
            enc3 = enc4 = 64;
        } else if (isNaN(chr3)) {
            enc4 = 64;
        }

        output = output +
            base64Chars.charAt(enc1) +
            base64Chars.charAt(enc2) +
            base64Chars.charAt(enc3) +
            base64Chars.charAt(enc4);
        chr1 = chr2 = chr3 = "";
        enc1 = enc2 = enc3 = enc4 = "";
    } while (i < input.length);

    return output;
}

var decode64 = window.atob || function(input) {
    var output = "";
    var chr1, chr2, chr3 = "";
    var enc1, enc2, enc3, enc4 = "";
    var i = 0;

    // remove all characters that are not A-Z, a-z, 0-9, +, /, or =
    var base64test = /[^A-Za-z0-9\+\/\=]/g;
    if (base64test.exec(input)) {
        alert("There were invalid base64 characters in the input text.\n" +
                "Valid base64 characters are A-Z, a-z, 0-9, '+', '/',and '='\n" +
                "Expect errors in decoding.");
    }
    input = input.replace(/[^A-Za-z0-9\+\/\=]/g, "");

    do {
        enc1 = base64Inv[input.charCodeAt(i++)];
        enc2 = base64Inv[input.charCodeAt(i++)];
        enc3 = base64Inv[input.charCodeAt(i++)];
        enc4 = base64Inv[input.charCodeAt(i++)];

        chr1 = (enc1 << 2) | (enc2 >> 4);
        chr2 = ((enc2 & 15) << 4) | (enc3 >> 2);
        chr3 = ((enc3 & 3) << 6) | enc4;

        output = output + String.fromCharCode(chr1);

        if (enc3 != 64) {
            output = output + String.fromCharCode(chr2);
        }
        if (enc4 != 64) {
            output = output + String.fromCharCode(chr3);
        }

        chr1 = chr2 = chr3 = "";
        enc1 = enc2 = enc3 = enc4 = "";

    } while (i < input.length);

    return output;
}

var defineClass = function(proto) {
    var f = function() {
        if (proto.init) {
            this.init.apply(this, arguments)
        }
    }
    f.prototype = proto
    proto.constructor = f
    return f
}

var inspect = function(o) {
    msg = ""+o+"\n"
    for (var k in o) {
        if (o.hasOwnProperty(k)) {
            msg+= k+" = "+o[k]+"\n"
        }
    }
    alert(msg)
}

////////////////////////////////////////////////////////////////////////////////
// acba

var Buffer = defineClass({
    BUFFER_SIZE: 4*1024,
    init: function() {
        this.buffer = new Array(this.BUFFER_SIZE)
        this.offset = 0
        this.size = 0
    },
    read: function() {
        var ret = this.buffer[this.offset];
        this.offset = (this.offset+1) % this.BUFFER_SIZE
        this.size--
        return ret
    },
    write: function(i) {
        this.buffer[(this.offset+this.size) % this.BUFFER_SIZE] = i
        this.size++
    },
    margin: function() { return this.BUFFER_SIZE - this.size },
    drop: function(s) {
        this.offset = (this.offset+s) % this.BUFFER_SIZE
        this.size -= s
    },
    hasWrappedData: function() {
        return (this.offset + this.size) > this.BUFFER_SIZE
    },
    hasWrappedMargin: function() {
        return (this.offset > 0) &&
                (this.offset + this.size) < this.BUFFER_SIZE
    },
    reset: function() {
        this.size = 0
    }
})

var IntegerCodec = [
    function (type, buf, ret) {
        var size = type[1]
        var tryRead = function() {
            if (buf.size < size) return [tryRead]
            var i = 0;
            for (var j = 0; j < size; j++) {
                i = (i << 8) + buf.read()
            }
            return ret(i);
        }
        return tryRead();
    },
    function (type, i, buf, ret) {
        var size = type[1]
        var tryWrite = function() {
            if (buf.margin() < size) {
                return [tryWrite]
            }
            for (var j = size - 1; j >= 0 ; j--) {
                buf.write((i>>(8*j)) & 0xff)
            }
            return ret();
        }
        return tryWrite();
    }
]

var StringCodec = [
    function (_, buf, ret) {
        var i = 0
        var arr
        var readNext = function() {
            if (i >= arr.length) return ret(decodeURIComponent(escape(
                    String.fromCharCode.apply(String,arr))))
            return readAs(I8, buf, function(c) {
                arr[i++] = c
                return readNext // break long-tail
            })
        }
        return readAs(I32, buf, function(s) {
            arr = new Array(s)
            return readNext()
        })
    },
    function (_, str, buf, ret) {
        arr = unescape(encodeURIComponent(str)); // encode to utf8
        var i = 0
        var writeNext = function() {
            if (i >= arr.length) return ret()
            return writeAs(I8, arr.charCodeAt(i++), buf, function() {
                return writeNext // break long-tail
            })
        }
        return writeAs(I32, arr.length, buf, writeNext)
    }
]

var ListCodec = [
    function (type, buf, ret) {
        var i, obj
        var readNext = function() {
            if (i >= obj.length) return ret(obj)
            return readAs(type[1], buf, function(r) {
                obj[i++] = r
                return readNext // break long-tail
            })
        }
        return readAs(I32, buf, function(s) {
            i = 0
            obj = new Array(s)
            return readNext()
        })
    },
    function (type, obj, buf, ret) {
        var i = 0
        var writeNext = function() {
            if (i >= obj.length) return ret()
            return writeAs(type[1], obj[i++], buf, function() {
                return writeNext // break long-tail
            })
        }
        return writeAs(I32, obj.length, buf, writeNext)
    }
]

var MapCodec = [
    function (type, buf, ret) {
        var i = 0
        var size, obj
        var readNext = function() {
            if (i >= size) return ret(obj)
            return readAs(type[1], buf, function(k) {
                return readAs(type[2], buf, function(v) {
                    obj[k] = v
                    i++
                    return readNext // break long-tail
                })
            })
        }
        return readAs(I32, buf, function(s) {
            size = s
            obj = {}
            return readNext()
        })
    },
    function (type, obj, buf, ret) {
        var objKeys = Object.keys(obj)
        var i = 0
        var writeNext = function() {
            if (i >= objKeys.length) return ret()
            var k = objKeys[i++]
            return writeAs(type[1], k, buf, function() {
                return writeAs(type[2], obj[k], buf, function() {
                    return writeNext // break long-tail
                })
            })
        }
        return writeAs(I32, objKeys.length, buf, writeNext)
    }
]

var StructCodec = [
    function (type, buf, ret) {
        var i = 1
        var obj = {}
        var readNext = function() {
            if (i >= type.length) return ret(obj)
            return readAs(type[i++], buf, function(item) {
                obj[type[i++]] = item
                return readNext
            })
        }
        return readNext();
    },
    function (type, obj, buf, ret) {
        var i = 1
        var writeNext = function() {
            if (i >= type.length) return ret();
            var j = i
            i += 2
            return writeAs(type[j], obj[type[j+1]], buf, writeNext)
        }
        return writeNext();
    }
]

var ExceptionCodec = [
    function (type, buf, ret) {
        var obj = {
            name: type[1]
        }
        var i = 2
        var readNext = function() {
            if (i >= type.length) return ret(obj)
            return readAs(type[i++], buf, function(item) {
                obj[type[i++]] = item
                return readNext
            })
        }
        return readNext();
    },
    function (type, obj, buf, ret) {
        var i = 2
        var writeNext = function() {
            if (i >= fields.length) return ret();
            var j = i
            i += 2
            return writeAs(type[j], obj[type[j+1]], writeNext)
        }
        return writeNext();
    }
]

var NullableCodec = [
    function (type, buf, ret) {
        return readAs(I8, buf, function(n) {
            if (n === 0) return ret(undefined)
            return readAs(type[1], buf, ret)
        })
    },
    function (type, obj, buf, ret) {
        if (obj === undefined) return writeAs(I8, 0, buf, ret)
        return writeAs(I8, 1, buf, function() {
            return writeAs(type[1], obj, buf, ret)
        })
    }
]

var InterfaceCodec = [
    function (type, buf, ret) {
        return readAs(I32, buf, function(id) {
            id = -id
            if (id > 0) {
                return buf.port.transport.registry.getSkeleton(id).object
            } else {
                return ret(createStub(type,
                        buf.port.transport.registry.getRemote(id)))
            }
        })
    },
    function (type, obj, buf, ret) {
        if (obj.isStub) {
            return writeAs(I32, obj.id, ret)
        }
        skel = buf.port.registry.getSkeleton(obj)
        skel.count++
        return writeAs(I32, skel.id, buf, ret)
    }
]

var VoidCodec = [
    function (_, buf, ret) { return ret(undefined) },
    function (_, obj, buf, ret) { return ret() }
]

var Types = {
    i8: [IntegerCodec, 1],
    i16: [IntegerCodec, 2],
    i32: [IntegerCodec, 4],
    String: [StringCodec],
    "void": [VoidCodec]
}

I8 = Types["i8"]
I16 = Types["i16"]
I32 = Types["i32"]
I64 = Types["i64"]
STRING = Types["String"]
VOID = Types["void"]

var readAs = function (type, buf, ret) {
    if (type.constructor === String) {
        type = Types[type]
    }
    return (type[0][0])(type, buf, ret)
}

var writeAs = function (type, obj, buf, ret) {
    if (type.constructor === String) {
        type = Types[type]
    }
    return (type[0][1])(type, obj, buf, ret)
}

var writeRequest = function (buf, header, oid, mid, args, argTypes) {
    //alert("requesting "+oid+"."+mid+":"+args)
    var i = 0
    var writeNext = function() {
        if (i >= argTypes.length) return
        return writeAs(argTypes[i], args[i], buf, function() {
            i++
            return writeNext
        })
    }

    return writeAs(I8, header, buf, function() {
        return writeAs(I32, oid, buf, function() {
            return writeAs(I16, mid, buf, function() {
                if (argTypes === undefined) return
                return writeNext()
            })
        })
    })
}

var createStubMethod = function(index, argTypes, retTypes) {
    return function() {
        var args = arguments
        var p = this.remote.registry.getPort()
        var self = this

        // async call
        if (retTypes === undefined) {
            p.writer = function(buf) {
                return writeRequest(buf, 1<<6,
                        self.remote.id, index, args, argTypes)
            }
            p.send()
            return
        }

        // sync call
        p.writer = function(buf) {
            return writeRequest(buf, 0,
                    self.remote.id, index, args, argTypes)
        }
        var ret = new Return(retTypes)
        p.sendAndWait(ret)
        if (ret.index === 0) {
            return ret.value
        } else {
            throw ret.value
        }
    }
}

var createStub = function (type, remote) {
    if (type.constructor === String) {
        type = Types[type]
    }

    // push stub class at the end of type info
    if (type.length % 3 === 1) {
        proto = {
            isStub: true,
            init: function(r) {
                this.remote = r
                r.ref()
            },
            dispose: function() {
                if (this.remote) {
                    this.remote.deref()
                }
                this.remote = undefined
            }
        }
        for (var i = 1; i+2 < type.length; i+=3) {
            proto[type[i]] = createStubMethod((i-1)/3, type[i+1], type[i+2])
        }
        type.push(defineClass(proto))
    }
    return new (type[type.length-1])(remote)
}

var Request = defineClass({
    init: function(obj, method, argTypes, retTypes) {
        this.object = obj
        this.method = method
        this.argTypes = argTypes
        if (retTypes) {
            this.ret = new Return(retTypes)
        }

        this.messageHead = undefined
        this.args = undefined
    },
    readArguments: function(buf, ret) {
        if (this.argTypes === undefined) return ret()
        var i = 0
        this.args = []
        var self = this
        var readNext = function() {
            if (i >= self.argTypes.length) return ret()
            return readAs(self.argTypes[i], buf, function(arg) {
                self.args[i++] = arg
                return readNext
            })
        }
        return readNext()
    },
    call: function() {
        if (!this.ret) {
            try {
                this.object[this.method].apply(this.object, this.args)
            } catch (e) {
                console.error("Failed to call async method:"+this.method)
            }
            return
        }

        try {
            this.ret.value = this.object[this.method].apply(this.object,
                    this.args)
            this.ret.index = 0
        } catch (e) {
            for (var i = 1; i < returnTypes.length; i++) {
                if (e.name === returnTypes[i][1]) {
                    this.ret.value = e
                    this.ret.index = i
                    return
                }
            }
            this.ret.index = -1
        }
    }
})

var Skeleton = defineClass({
    init: function(type, obj) {
        this.type = type
        this.object = obj
        this.id = undefined
    }
})

////////////////////////////////////////////////////////////////////////////////
// ROP
var Remote = defineClass({
    init: function() {
        this.id = undefined // negative
        this.registry = undefined
        this.count = 0 // import count.
        this.refCount = 0
    },
    ref: function() { this.refCount++ },
    deref: function() {
        if (--this.refCount <= 0 && this.registry) {
            this.registry.notifyRemoteDestroy(this.id, this.count)
        }
    }
})

var rpcThreadId = undefined
var asyncOnly = false

var async = function (cb) {
    var old = asyncOnly
    asyncOnly = true
    cb()
    asyncOnly = old
}

var Registry = defineClass({
    init: function() {
        this.nextSkeletonId = 1
        this.exportables = {}
        this.remotes = {}
        this.skeletons = {}
        this.skeletonByExportable = {}
        this.transport = undefined

        this.ports = {}
    },
    getPort: function(pid) {
        if (pid === undefined) {
            pid = rpcThreadId || 1
        }

        var p = this.ports[pid]
        if (p === undefined) {
            p = new Port(this)
            p.id = pid
            this.ports[pid] = p
        }
        return p
    },
    releasePort: function(p) {
        if (p.isActive()) {
            delete this.ports[p.id]
        }
    },
    createSkeleton: function() {},
    registerExportable: function(name, exp) {
        this.exportables[name] = exp
    },
    getRemote: function(id) {
        if (id.constructor === Number) {
            r = this.remotes[id]
            if (r === undefined) {
                r = new Remote()
                r.id = id
                r.registry = this
                this.remotes[id] = r
            }
            return r
        } else {
            var p = this.getPort()

            p.writer = function(buf) {
                return writeRequest(buf, 0, 0, 0, [id], [STRING])
            }
            var ret = new Return([I32])
            p.sendAndWait(ret)
            if (ret.index === 0) {
                var r = this.getRemote(-ret.value)
                r.count++
                return r
            } else {
                throw ret.value
            }
        }
    },
    notifyRemoteDestroy: function(id, count) {
        alert("sending nrd("+id+","+count+")")
        var self = this
        async( function() {
            var p = self.getPort(0)
            var args = arguments
            p.writer = function(buf) {
                return writeRequest(buf, 1<<6, 0, 1, args, [I32, I32])
            }
            p.send()
            self.remotes[id].registry = undefined
            delete self.remotes[id]
        })
    },
    getSkeleton: function(id) {
        if (id.constructor === Number) {
            return this.skeletons[id]
        } else {
            var skel = this.skeletonByExportable[id]
            if (skel === undefined) {
                if (id.name.constructor !== String) {
                    throw "Cannot create skeleton from unnamed object"
                }
                skel = new Skeleton(Types[id.name], id)
                skel.id = this.nextSkeletonId++
                this.skeletons[skel.id] = skel
                this.skeletonByExportable[id] = skel
            }
            return skel
        }
    },
    setTransport: function(trans) {
        this.transport = trans
        if (trans) {
            trans.registry = this
        }
    }
})

var runCont = function(cont) {
    while (cont) {
        if (cont.constructor == Array) {
            return cont[0]
        }
        cont = cont()
    }
}

var Transport = defineClass({
    init: function(host, onconnect) {
        this.xhrUrl = "http://"+host+"/rop/xhr"
        this.wsUrl = "ws://"+host+"/rop/ws"

        this.registry = undefined
        this.inBuffer = new Buffer()
        this.outBuffer = new Buffer()
        this.requestBuffer = new Buffer()
        this.inPort = undefined
        this.writeInPort = undefined
        this.readOutPort = undefined

        // open websocket
        var ws = new WebSocket(this.wsUrl)
        var self = this
        ws.onopen = function() {
            console.log("WebSocket open")
            self.webSocket = ws
            onconnect()
        }
        ws.onclose = function() {
            console.log("WebSocket closed")
            self.WebSocket = undefined
        }
        ws.onmessage = function(e) {
            //alert(e.data)
            self.receive(e)
        }
    },
    send: function(p) {
        // send via websocket

        // construct request message from port
        var msg = []
        buf = this.requestBuffer
        buf.port = p
        buf.write((p.id >> 24) & 0xff)
        buf.write((p.id >> 16) & 0xff)
        buf.write((p.id >> 8) & 0xff)
        buf.write(p.id & 0xff)
        w = function() { if (p.writer) return p.writer(buf) }
        while (w) {
            w = runCont(w)
            while (buf.size > 0) {
                msg.push(buf.read())
            }
        }
        p.writer = undefined

        msg = encode64(String.fromCharCode.apply(String, msg))
        this.webSocket.send(msg)
    },
    receive: function(e) {
        // receive via websocket
        console.log(e.data)
        var msg = decode64(e.data)
        var pid = msg.charCodeAt(0)
        pid = (pid << 8) + msg.charCodeAt(1)
        pid = (pid << 8) + msg.charCodeAt(2)
        pid = (pid << 8) + msg.charCodeAt(3)
        pid = -pid
        console.log("Getting message to port:"+pid)
        alert("Getting message to port:"+pid)

        var arr = []
        for (var i = 0; i < msg.length; i++) {
            arr.push(msg.charCodeAt(i))
        }
        inspect(arr)

        var p = this.registry.getPort(pid)
        var buf = this.inBuffer
        var r = function() { return p.readMessage(buf, function() {}) }
        var i = 4
        while (r) {
            console.log("reading... from:"+i);
            console.log(r)
            while (i < msg.length && buf.margin() > 0) {
                buf.write(msg.charCodeAt(i++))
            }
            r = runCont(r)
        }
        this.registry.releasePort(p)
        while (p.processRequest());
    },
    sendAndReceive: function(p) {
        // send request and wait for response via xmlhttprequest

        // construct request message from port
        var msg = []
        buf = this.requestBuffer
        buf.port = p
        buf.write((p.id >> 24) & 0xff)
        buf.write((p.id >> 16) & 0xff)
        buf.write((p.id >> 8) & 0xff)
        buf.write(p.id & 0xff)
        w = function() { if (p.writer) return p.writer(buf) }
        while (w) {
            console.log("wrigin...");
            w = runCont(w)
            while (buf.size > 0) {
                msg.push(buf.read())
            }
        }
        p.writer = undefined

        msg = encode64(String.fromCharCode.apply(String, msg))

        req = new XMLHttpRequest()
        req.open("POST", this.xhrUrl, false)
        req.setRequestHeader("Content-Type", "text/plain")
        req.send(msg)
        msg = req.responseText
        if (msg == "") {
            console.log("got empty response from xhr (it's ok)")
            return
        }

        msg = decode64(msg)
        var r = function() { return p.readMessage(buf, function() {}) }
        var i = 0
        while (r) {
            while (i < msg.length && buf.margin() > 0) {
                buf.write(msg.charCodeAt(i++))
            }
            console.log("reading...");
            r = runCont(r)
        }
    },
})

var Port = defineClass({
    init: function(reg) {
        this.registry = reg
        this.id = undefined
        this.returns = []
        this.requests = []
        this.lastRequest = undefined
        this.writer = undefined
        this.reader = undefined
    },
    send: function(ret) {
        if (ret) this.returns.push(ret)
        if (asyncOnly) {
            this.registry.transport.send(this)
        } else {
            this.registry.transport.sendAndReceive(this)
        }
        this.registry.releasePort(this)
    },
    sendAndWait: function(ret) {
        if (asyncOnly) {
            alert("Cannot use synchronous API in async mode")
            // TODO: flush port-out 
            this.registry.releasePort(this)
            throw "Cannot use synchronous API in async mode"
        }

        this.returns.push(ret)
        do {
            this.registry.transport.sendAndReceive(this)
            if (this.requests.length > 0) {
                this.processRequest()
            }
        } while (ret.index === undefined)
        this.registry.releasePort(this)
    },
    processRequest: function() {
        var req = this.requests.pop()
        if (!req) return false
        var lreq = this.lastRequest
        this.lastRequest = undefined

        var otid = rpcThreadId
        rpcThreadId = ((req.messageHead & (1<<6)) > 0) ? 1 : this.id
        req.call()
        rpcThreadId = otid

        lastRequest = req
        if (req.ret) {
            this.writer = function(buf) {
                return req.ret.write(buf, function() {})
            }
        }
    },
    isActive: function() {
        if (this.returns.length > 0) {
            return true
        }
        if (this.requests.length > 0) {
            return true
        }
        if ((this.writer && this.writer.cont) ||
                (this.reader && this.reader.cont)) {
            return true
        }
        return false
    },
    readMessage: function(buf, ret) {
        var methodIndex;
        var request;
        var self = this

        return readAs(I8, buf, function(messageHead) {
            if ((messageHead & (3<<6)) === (3<<6)) {
                var r = self.returns.pop()
                if (!r) throws "No return waiting"
                return r.read(messageHead, buf, ret)
            } else {
                return readAs(I32, buf, function(objectId) {
                    objectId = -objectId
                    if (objectId < 0) throw "Expected positive obj id"
                    return readAs(I16, buf, function(methodIndex) {
                        var skel = self.registry.getSkeleton(objectId)
                        if (!skel) throw "No such skeleton"
                        var base = methodIndex * 3 + 1
                        var req = new Request(skel.object, skel.type[base],
                                skel.type[base+1], skel.type[base+2])
                        req.messageHead = messageHead
                        return req.readArguments(buf, function() {
                            console.log("Got request:"+req)
                            self.requests.push(req)
                        })
                    })
                })
            }
        })
    }
})

var Return = defineClass({
    init: function(retTypes) {
        this.returnTypes = retTypes
        //this.index = undefined
        //this.value = undefined
    },
    read: function(messageHead, buf, ret) {
        var idx = messageHead & 63
        this.index = (idx == 63) ? -1 : idx

        if (!this.returnTypes[this.index]) {
            return ret(this)
        }

        var self = this
        return readAs(this.returnTypes[this.index], buf, function(r) {
            self.value = r
            return ret(self)
        })
    },
    write: function(buf, ret) {
        if (!this.returnTypes[this.index]) {
            return writeAs(I8, (3<<6)|63, buf, ret)
        }

        return writeAs(I8, (3<<6) + this.index, buf, function() {
            return writeAs(returnTypes[this.index], this.value, buf, ret)
        })
    }
})
