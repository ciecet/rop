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

var p = function(o) {
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

var Interrupt = defineClass({
    init: function(reason, cont) {
        this.reason = reason
        this.cont = cont
    }
})

var IntegerCodec = [
    function (type, buf, ret) {
        var size = type[1]
        var tryRead = function() {
            if (buf.size < size) throw new Interrupt("stopped", tryRead)
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
                throw new Interrupt("stopped", tryWrite)
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
            if (i >= fields.length) return ret();
            var j = i
            i += 2
            return writeAs(type[j], obj[type[j+1]], writeNext)
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
        skel = buf.port.transport.registry.getSkeleton(obj)
        skel.stamp++
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
    void: [VoidCodec]
}

I8 = Types["i8"]
I16 = Types["i16"]
I32 = Types["i32"]
STRING = Types["String"]
VOID = Types["void"]

Types.Person = [StructCodec,
    STRING, "name",
    "EchoCallback", "callback",
]
Types.TestException = [ExceptionCodec, "TestException",
    [NullableCodec, I32], "i"
]
Types.EchoCallback = [InterfaceCodec,
    "call", [STRING], undefined // async
]
Types.Echo = [InterfaceCodec,
    "echo", [STRING], [STRING],
    "concat", [[ListCodec, STRING]], [STRING],
    "touchmenot", undefined, ["TestException"],
    "recursiveEcho", [STRING, "EchoCallback"], [VOID],
    "hello", ["Person"], [VOID]
]

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
        var p = this.remote.registry.transport.getPort()
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
            alert("writing "+self.remote.id+"."+index+":"+args)
            return writeRequest(buf, 0,
                    self.remote.id, index, args, argTypes)
        }
        var ret = new Return(retTypes)
        p.sendAndWait(ret)
        if (ret.index === 0) {
            return ret.value
        } else if (ret.index === null) {
            throw "Unknown Error"
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
        this.ret = new Return(retTypes)

        this.messageHeader = undefined
        this.args = undefined
    },
    readArguments: function(buf) {
        if (this.argTypes === undefined) return
        var i = 0
        this.args = []
        var self = this
        var readNext = function() {
            if (i >= self.argTypes.length) return
            return readAs(self.argTypes[i], buf, function(arg) {
                self.args[i++] = arg
                return readNext
            })
        }
        return readNext()
    },
    call: function() {
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
            this.ret.value = null
            this.ret.index = null
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

/*
var buf = new Buffer()
var data = {3:["sooin", "okie"], 4:["수인", "현옥"]}
var result
var type = [MapCodec, I32, [ListCodec, STRING]]
var sender = writeAs(type, data, buf, function(){})
while (sender) sender = sender()
var receiver = readAs(type, buf, function(r) { result = r })
while (receiver) receiver = receiver()
p(result)
*/

////////////////////////////////////////////////////////////////////////////////
// ROP
var Remote = defineClass({
    init: function() {
        this.id = undefined // negative
        this.registry = undefined
        this.refCount = 0
    },
    deref: function() {
        this.refCount--
        if (this.refCount <= 0 && this.registry) {
            registry.notifyRemoteDestroy(this.id)
        }
    }
})

var Registry = defineClass({
    init: function() {
        this.nextSkeletonId = 1
        this.exportables = {}
        this.remotes = {}
        this.skeletons = {}
        this.skeletonByExportable = {}
        this.transport = undefined
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
            var p = this.transport.getPort()

            p.writer = function(buf) {
                return writeRequest(buf, 0, 0, 0, [id], [STRING])
            }
            var ret = new Return([I32])
            p.sendAndWait(ret)
            if (ret.index === 0) {
                return this.getRemote(-ret.value)
            } else if (ret.index === null) {
                throw "Unknown Error"
            } else {
                throw ret.value
            }
        }
    },
    notifyRemoteDestroy: function(id) {},
    getSkeleton: function(id) {
        if (id.constructor === Number) {
            return this.skeletons[id]
        } else {
            var skel = this.skeletonByExportable[id]
            if (skel === undefined) {
                skel = id.createSkeleton()
                skel.id = this.nextSkeletonId++
                skeletons[skel.id] = skel
                skeletonByExportable[id] = skel
            }
            return skel
        }
    }
})

var rpcThreadId = undefined

// Due to the limitation from javascript/browser,
// Transport is the only descrete implementation,
// which uses both websocket and xmlhttprequest.
var Transport = defineClass({
    init: function(url) {
        this.registry = new Registry()
        this.registry.transport = this

        this.ports = {}

        this.inBuffer = new Buffer()
        this.outBuffer = new Buffer()
        this.requestBuffer = new Buffer()
        this.inPort = undefined
        this.writeInPort = undefined
        this.readOutPort = undefined

        this.url = url
        // open websocket
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
    send: function(p) {
        // send request via websocket
    },
    sendAndReceive: function(p) {
        // send request and wait for response via xmlhttprequest

        // construct request message from port
        var msg = []
        buf = this.requestBuffer
        buf.write((p.id >> 24) & 0xff)
        buf.write((p.id >> 16) & 0xff)
        buf.write((p.id >> 8) & 0xff)
        buf.write(p.id & 0xff)
        w = function() { return p.writer(buf) }
        while (w) {
            w = this.runCont(w)
            while (buf.size > 0) {
                msg.push(buf.read())
            }
        }

        msg = btoa(String.fromCharCode.apply(String, msg))
        alert("sending... "+msg)

        req = new XMLHttpRequest()
        req.open("POST", this.url, false)
        req.setRequestHeader("Content-Type", "text/plain")
        req.send(msg)
        msg = req.responseText
        alert("got "+msg)

        msg = atob(msg)
        var arr = []
        for (var i in msg) {
            arr.push(msg.charCodeAt(i))
        }
        window.p(arr)

        h = msg.charCodeAt(0)
        if ((h&(3<<6)) === 3<<6) {
            var ret = p.returns.shift()
            if (!ret) throw "No Return waiting"
            r = function() { return ret.read(h&63, buf, function() {}) }
            var i = 1
            while (r) {
                while (i < msg.length && buf.margin() > 0) {
                    buf.write(msg.charCodeAt(i++))
                }
                r = this.runCont(r)
            }
            if (buf.size > 0) {
                alert("return remaining... "+buf.size) 
                buf.reset()
            }
        } else {
            alert("Not supported");
        }
    },
    runCont: function(cont) {
        try {
            while (cont !== undefined) cont = cont(buf)
        } catch (e) {
            if (e.constructor !== Interrupt) {
                throw e
            }
            console.log(e.reason)
            return cont
        }
    }
})

var Port = defineClass({
    init: function(trans) {
        this.transport = trans
        this.id = undefined
        this.returns = []
        this.requests = []
        this.lastRequest = undefined
        this.writer = undefined
        this.reader = undefined
    },
    send: function(ret) {
        if (ret) this.returns.push(ret)
        this.transport.send(this)
        this.transport.releasePort(this)
    },
    sendAndWait: function(ret) {
        this.returns.push(ret)
        this.transport.sendAndReceive(this)
            //if (this.requests.length > 0) {
            //    processRequest()
            //    continue
            //}
            //if (ret.isValid) break
        //    this.transport.receive(this)
        this.transport.releasePort(this)
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
        if (req.ret.index !== undefined) {
            this.writer = new Task(function() {
            })
            
            transport.send(req.ret)
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
    }
})

var Return = defineClass({
    init: function(retTypes) {
        this.returnTypes = retTypes
        //this.index = undefined
        //this.value = undefined
    },
    read: function(idx, buf, ret) {
        this.index = idx
        if (this.returnTypes[idx] === undefined) {
            return ret(this)
        }

        var self = this
        return readAs(this.returnTypes[idx], buf, function(r) {
            self.value = r
            return ret(self)
        })
    },
    write: function(buf, ret) {
        return writeAs(I8, (3<<6) + this.index, buf, function() {
            return writeAs(returnTypes[this.index], this.value, buf, ret)
        })
    }
})

var readMessage = function(port, buf, ret) {
    var methodIndex;
    var request;

    return readAs(I8, buf, function(messageHead) {
        if (messageHead & (3<<6) === (3<<6)) {
            var r = port.returns.pop()
            if (!r) throw new Interrupt("ABORTED")
            return r.read(messageHead&((1<<6)-1), buf, ret)
        } else {
            return readAs(I32, buf, function(objectId) {
                objectId = -objectId
                if (objectId < 0) throw new Interrupt("ABORTED")
                return readAs(I16, buf, function(methodIndex) {
                    var skel = port.transport.registry.getSkeleton(objectId)
                    if (!skel) throw new Interrupt("ABORTED")
                    var req = new Request(skel.object,
                            skel.methods[methodIndex])
                    req.messageHead = messageHead
                    return req.readArguments(buf, function() {
                        port.requests.push(req)
                    })
                })
            })
        }
    })
}

var trans = new Transport("http://localhost:8080")
var reg = trans.registry
var rr = reg.getRemote("Echo")
p(rr)
var e = createStub("Echo", rr)
alert(e.echo("한글테스트"))

//req = new XMLHttpRequest()
//req.open("POST", "http://192.168.10.3:8080", false)
//req.setRequestHeader("Content-Type", "text/plain")
//req.send("hi there!")
//alert("got "+req.responseText)
