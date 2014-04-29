"use strict";
acba.define('js:rop_base',
['log:rop', 'js:rop_buffer'],
function (log, RopBuffer) {
    // Prevent global access for peformance
    var Array = acba("global")["Array"]
    var String = acba("global")["String"]
    var Number = acba("global")["Number"]

    //log.level = log.TRACE

    // super comes from http://oranlooney.com/super-javascript/
    var defineClass = function (base, props) {
        var cls = function () {
            if (proto.init) {
                this.init.apply(this, arguments)
            }
        }
        var proto
        if (typeof(base) === 'function') {
            proto = Object.create(base.prototype)
            var ps = Object.getOwnPropertyNames(props)
            for (var i = 0; i < ps.length; i++) {
                var p = ps[i]
                var d = Object.getOwnPropertyDescriptor(props, p)
                Object.defineProperty(proto, p, d)
            }
            cls.super = base.prototype
        } else {
            proto = base
        }

        cls.prototype = proto
        proto.constructor = cls
        return cls
    }

    var I8Codec = [
        function (type, buf) { return buf.readI8() },
        function (type, i, buf) { buf.writeI8(i) }
    ]

    var I16Codec = [
        function (type, buf) { return buf.readI16() },
        function (type, i, buf) { buf.writeI16(i) }
    ]

    var I32Codec = [
        function (type, buf) { return buf.readI32() },
        function (type, i, buf) { buf.writeI32(i) }
    ]

    var I64Codec = [
        function (_, buf) {
            var v = []
            v.push(buf.readI32())
            v.push(buf.readI32())
            return v
        },
        function (_, v, buf) {
            buf.writeI32(v[0])
            buf.writeI32(v[1])
        }
    ]

    var BoolCodec = [
        function (_, buf) { return (buf.read() > 0) },
        function (_, val, buf) { buf.writeI8(val ? 1 : 0) }
    ]

    var F32Codec = [
        function (_, buf) { return buf.readF32() },
        function (_, val, buf) { buf.writeF32(val) }
    ]

    var F64Codec = [
        function (_, buf) { return buf.readF64() },
        function (_, val, buf) { buf.writeF64(val) }
    ]

    var StringCodec = [
        function (_, buf) { return buf.readString(buf.readI32()) },
        function (_, str, buf) { buf.writeString(str) }
    ]

    var ListCodec = [
        function (type, buf) {
            var len = buf.readI32()
            var l = new Array(len)
            var t = getType(type[1])
            for (var i = 0; i < len; i++) {
                l[i] = readAs(t, buf)
            }
            return l
        },
        function (type, l, buf) {
            var len = l.length
            buf.writeI32(len)
            var t = getType(type[1])
            for (var i = 0; i < len; i++) {
                writeAs(t, l[i], buf)
            }
        }
    ]

    var MapCodec = [
        function (type, buf) {
            var kt = getType(type[1])
            var vt = getType(type[2])
            var m = {}
            var len = buf.readI32()
            for (var i = 0; i < len; i++) {
                m[readAs(kt, buf)] = readAs(vt, buf)
            }
            return m
        },
        function (type, m, buf) {
            var kt = getType(type[1])
            var vt = getType(type[2])
            var objKeys = Object.keys(m)
            var len = objKeys.length
            buf.writeI32(len, buf)
            for (var i = 0; i < len; i++) {
                var k = objKeys[i]
                writeAs(kt, k, buf)
                writeAs(vt, m[k], buf)
            }
        }
    ]

    var StructCodec = [
        function (type, buf) {
            var obj = {}
            for (var i = 1; i < type.length; i += 2) {
                obj[type[i+1]] = readAs(type[i], buf)
            }
            return obj
        },
        function (type, obj, buf) {
            for (var i = 1; i < type.length; i += 2) {
                writeAs(type[i], obj[type[i+1]], buf)
            }
        }
    ]

    var Exception = function (name) {
        this.name = name
    }
    Exception.prototype.isException = true
    var isException = function (ex) {
        return (ex instanceof Exception)
    }
    var ExceptionCodec = [
        function (type, buf) {
            var ex = new Exception(type[1])
            for (var i = 2; i < type.length; i += 2) {
                ex[type[i+1]] = readAs(type[i], buf)
            }
            return ex
        },
        function (type, obj, buf) {
            for (var i = 2; i < type.length; i += 2) {
                writeAs(type[i], obj[type[i+1]], buf)
            }
        }
    ]

    var NullableCodec = [
        function (type, buf) {
            if (buf.read() === 0) return undefined
            return readAs(type[1], buf)
        },
        function (type, obj, buf) {
            if (obj === undefined || obj === null) {
                buf.writeI8(0)
                return
            }
            buf.writeI8(1)
            writeAs(type[1], obj, buf)
        }
    ]

    /**
     * Type: [ InterfaceCodec, [constants...], [methods...], stubclass ]
     */
    var InterfaceCodec = [
        function (type, buf) {
            var id = -buf.readI32()
            if (id > 0) {
                var skel = buf.port.registry.getSkeleton(id)
                if (skel === undefined) {
                    throw "Got unknown skeleton:"+id
                }
                return skel.object
            } else {

                if (log.level >= log.DEBUG) {
                    var i
                    for (i in Types) {
                        if (Types[i] === type) {
                            log.debug("got remote object of "+i)
                            break;
                        }
                    }
                }

                var r = buf.port.registry.getRemote(id)
                r.count++
                return createStub(type, r)
            }
        },
        function (type, obj, buf) {
            var reg = buf.port.registry
            if (obj.__isStub) {
                if (obj.remote.registry !== reg) {
                    throw "Cannot send remote object from difference source"
                }
                buf.writeI32(obj.remote.id)
                return
            }

            var skel = reg.getSkeletonBy(obj, type)
            skel.count++
            buf.writeI32(skel.id)
        }
    ]

    var VoidCodec = [
        function (_, buf) { return undefined },
        function (_, obj, buf) { return }
    ]

    var VariantCodec = [
        function (_, buf) {
            var h = buf.readI8()
            if (h >= -32) return h
            switch (h & 0xf0) {
            case 0xc0: case 0xd0: // string
                var len = h & 0x1f
                if (len === 0x1f) {
                    len = buf.readI32()
                }
                return buf.readString(len)
            case 0xa0: case 0xb0: // list
                var len = h & 0x1f
                if (len === 0x1f) {
                    len = buf.readI32()
                }
                var l = new Array(len)
                for (var i = 0; i < len; i++) {
                    l[i] = readAs(VARIANT, buf)
                }
                return l
            case 0x90: // map
                var len = h & 0xf
                if (len === 0xf) {
                    len = buf.readI32()
                }
                var m = {}
                for (var i = 0; i < len; i++) {
                    m[readAs(VARIANT, buf)] = readAs(VARIANT, buf)
                }
                return m
            case 0x80: break
            default: return undefined
            }

            switch (h & 0xf) {
            case 2: return false
            case 3: return true
            case 4: return buf.readI8()
            case 5: return buf.readI16()
            case 6: return buf.readI32()
            case 7: // i64
                var i = buf.readI32() * 4 * 1024 * 1024 * 1024
                return i + buf.readUI32()
            case 8: return buf.readF32()
            case 9: return buf.readF64()
            default: return undefined
            }
        },
        function (_, obj, buf) {
            switch (typeof(obj)) {
            case "boolean":
                buf.writeI8(obj ? 0x83 : 0x82, buf);
                break;
            case "number":
                if ((obj >> 0) === obj) { // if within 32bit signed integer
                    buf.writeI8(0x86)
                    buf.writeI32(obj)
                } else { // otherwise, treat as double
                    buf.writeI8(0x89)
                    buf.writeF64(obj)
                }
                break
            case "string":
                buf.writeI8(0xc0 | 31)
                writeAs(STRING, obj, buf)
                break
            case "object":
                if (obj === null) {
                    buf.writeI8(0x80) // null
                } else if (obj.constructor === Array) { // list
                    buf.writeI8(0xa0 | 31)
                    writeAs([ListCodec, VARIANT], obj, buf)
                } else { // map
                    buf.writeI8(0x90 | 15)
                    writeAs([MapCodec, VARIANT, VARIANT], obj, buf)
                }
                break
            case "undefined":
            default:
                buf.writeI8(0x80) // null
                break
            }
        }
    ]

    var Types = {
        i8: [I8Codec],
        i16: [I16Codec],
        i32: [I32Codec],
        i64: [I64Codec],
        String: [StringCodec],
        void: [VoidCodec],
        bool: [BoolCodec],
        f32: [F32Codec],
        f64: [F64Codec],
        Variant: [VariantCodec]
    }

    var Codecs = {
        Nullable: NullableCodec,
        List: ListCodec,
        Map: MapCodec,
        Struct: StructCodec,
        Exception: ExceptionCodec,
        Interface: InterfaceCodec
    }

    var I8 = Types.i8
    var I16 = Types.i16
    var I32 = Types.i32
    var I64 = Types.i64
    var STRING = Types.String
    var VOID = Types.void
    var BOOLEAN = Types.bool
    var F32 = Types.f32
    var F64 = Types.f64
    var VARIANT = Types.Variant

    var getType = function (type) {
        var org = type
        if (type.constructor === String) {
            type = Types[type]
        }
        if (type === undefined) {
            throw 'Unknown type:"'+org+'"'
        }
        return type
    }

    var readAs = function (type, buf) {
        type = getType(type)
        return (type[0][0])(type, buf)
    }

    var writeAs = function (type, obj, buf) {
        type = getType(type)
        type[0][1](type, obj, buf)
    }

    var writeRequest = function (buf, header, oid, mid, args, argTypes) {
        //alert("requesting "+oid+"."+mid+":"+args)
        buf.writeI8(header)
        buf.writeI32(oid)
        buf.writeI16(mid)
        for (var i = 0; i < argTypes.length; i++) {
            writeAs(argTypes[i], args[i], buf)
        }
    }

    var createStubMethod = function (index, name, argTypes, retTypes) {
        // if async method
        if (retTypes === undefined) {
            return function () {
                var args = arguments
                var reg = this.remote.registry
                var self = this

                var ctx = args[argTypes.length]
                var p = reg.getPort(ctx && ctx.portId)
                p.writer = function (buf) {
                    writeRequest(buf, 1<<6, self.remote.id, index, args,
                            argTypes)
                }
                log.info('async method '+this.remote.id+"."+name+
                        " from port: "+p.id)
                p.send()
            }
        }

        // if sync method
        return function () {
            var args = arguments
            var reg = this.remote.registry
            var self = this

            var cb = args[argTypes.length] // either callback or ctx
            var ctx = args[argTypes.length+1] || cb
            var p = reg.getPort(ctx && ctx.portId)

            // if async call
            if (p.isAsync()) {
                p.writer = function (buf) {
                    writeRequest(buf, 1<<6, self.remote.id, index, args,
                            argTypes)
                }
                var ret = new Return(retTypes, cb)
                log.info('async call on sync method '+this.remote.id+"."+name+
                        " from port: "+p.id)
                p.send(ret)
                return
            }

            // if sync call
            p.writer = function (buf) {
                writeRequest(buf, 0, self.remote.id, index, args, argTypes)
            }
            var ret = new Return(retTypes)
            log.info('sync method '+this.remote.id+"."+name+" from port: "+
                    p.id)
            p.sendAndWait(ret)
            log.debug('returned '+ret.index+' '+ret.value)
            if (ret.index === 0) {
                return ret.value
            } else {
                if (ret.index < 0) {
                    throw "Remote call failed unexpectedly."
                } else {
                    throw ret.value
                }
            }
        }
    }

    var stubProtoProto = {
        __isStub: true,
        __dispose: function () {
            if (!refFactory) {
                if (this.remote) {
                    this.remote.deref()
                }
                this.remote = undefined
            } else {
                log.debug("explicit __dispose() call was ignored since "+
                        "gc is active.")
            }
        },
        addStubListener: function (l) {
            if (this.__listeners === undefined) {
                this.__listeners = []
                var self = this
                this.__disposer = function () {
                    var lnrs = self.__listeners
                    self.remote.registry.removeRegistryListener(self.__disposer)
                    delete self.__listeners
                    delete self.__disposer
                    for (var i = 0; i < lnrs.length; i++) {
                        try {
                            lnrs[i].stubRevoked(self)
                        } catch (e) {
                            log.error(e)
                        }
                    }
                }
                this.remote.registry.addRegistryListener(this.__disposer)
            }
            this.__listeners.push(l)
        },
        removeStubListener: function (l) {
            if (this.__listeners === undefined) return
            for (var i = 0; i < this.__listeners.length; i++) {
                if (this.__listeners[i] === l) {
                    this.__listeners.splice(i, 1)
                    break
                }
            }
            if (this.__listeners.length === 0) {
                this.remote.registry.removeRegistryListener(this.__disposer)
                delete this.__disposer
                delete this.__listeners
            }
        }
    }

    var getStubPrototype = function (type) {
        type = getType(type)

        // type[3] is a stub prototype for the type.
        var proto = type[3]
        if (!proto) {
            proto = Object.create(stubProtoProto)

            var consts = type[1]
            if (consts) {
                for (var i = 0; i < consts.length; i+=2) {
                    proto[consts[i]] = consts[i+1]
                }
            }

            var methods = type[2]
            for (var i = 0; i < methods.length; i+=3) {
                proto[methods[i]] = createStubMethod(i/3, methods[i],
                        methods[i+1] || [], methods[i+2])
            }
            type[3] = proto
        }
        return proto
    }

    var getConsts = function (type) {
        type = getType(type)
        var consts = type[1]
        if (!consts) return {}

        // type[4] is a constants holder for the type.
        var holder = type[4]
        if (!holder) {
            holder = {}
            for (var i = 0; i < consts.length; i+=2) {
                holder[consts[i]] = consts[i+1]
            }
            type[4] = Object.freeze(holder)
        }
        return holder
    }

    var createStub = function (type, remote) {
        var stub = Object.create(getStubPrototype(type))
        stub.remote = remote
        remote.ref()
        if (refFactory) {
            stub.phantom = refFactory.createRef(remote)
        }
        return stub
    }

    var LocalCall = defineClass({
        init: function (obj, method, argTypes, retTypes) {
            this.object = obj
            this.method = method
            this.argTypes = argTypes
            if (retTypes) {
                this.ret = new Return(retTypes)
            }

            this.messageHead = undefined
            this.args = []
        },
        loadArguments: function (buf) {
            if (this.argTypes === undefined) return
            var len = this.argTypes.length
            //this.args = new Array(len)
            for (var i = 0; i < len; i++) {
                //this.args[i] = readAs(this.argTypes[i], buf)
                this.args.push(readAs(this.argTypes[i], buf))
            }
        },
        callAsyncMethod: function (ctx) {
            try {
                this.args.push(ctx)
                log.info('processing async method '+this.object.__skeletonId+
                        '.'+this.method+' from port:'+ctx.portId)
                this.object[this.method].apply(this.object, this.args)
            } catch (e) {
                log.error("Exception occurred in async method:"+this.method)
                log.error(e)
            }
        },
        callSync: function (ctx, ret) {
            if (!this.ret) {
                this.callAsyncMethod(ctx)
                ret()
                return
            }

            var ri, rv
            try {
                this.args.push(ctx)
                log.info('processing sync method '+this.object.__skeletonId+
                        '.'+this.method+' from port:'+ctx.portId+
                        ' in browser mode')
                rv = this.object[this.method].apply(this.object, this.args)
                ri = 0
            } catch (e) {
                log.error("Exception occurred in sync method:"+this.method)
                log.error(e)
                ri = -1
                rv = undefined
                for (var i = 1; i < this.ret.returnTypes.length; i++) {
                    if (e.name === this.ret.returnTypes[i]) {
                        rv = e
                        ri = i
                        break
                    }
                }
            }
            ret(ri, rv)
        },
        callAsync: function (ctx, ret) {
            if (!this.ret) {
                this.callAsyncMethod(ctx)
                ret()
                return
            }

            try {
                this.args.push(ret)
                this.args.push(ctx)
                log.info('processing sync method '+this.object.__skeletonId+
                        '.'+this.method+' from port:'+ctx.portId+
                        ' in node mode')
                this.object[this.method].apply(this.object, this.args)
            } catch (e) {
                log.error("Exception occurred in sync method:"+this.method)
                log.error(e)

                var ri = -1
                var rv = undefined
                for (var i = 1; i < this.ret.returnTypes.length; i++) {
                    if (e.name === this.ret.returnTypes[i]) {
                        rv = e
                        ri = i
                        break
                    }
                }
                ret(ri, rv) // assume that ret was not yet called
            }
        }
    })

    var Skeleton = function (obj, type) {
        this.type = type
        this.object = obj
        this.id = undefined
        this.count = 0
    }

    var Remote = defineClass({
        init: function () {
            this.id = undefined // negative
            this.registry = undefined
            this.count = 0 // import count.
            this.refCount = 0
        },
        ref: function () { this.refCount++ },
        deref: function () {
            if (--this.refCount <= 0 && this.registry) {
                this.registry.notifyRemoteDestroy(this.id, this.count)
            }
        }
    })

    // odd: async, even: sync
    var nextAsyncPortId = 1 // increase by 2
    var nextSyncPortId = 2 // increase by 2

    var createAsyncPortId = function () {
        var pid = nextAsyncPortId
        nextAsyncPortId += 2
        return pid
    }

    var createSyncPortId = function () {
        var pid = nextSyncPortId
        nextSyncPortId += 2
        return pid
    }

    // create calling context which is independent to transport/registry.
    // context causes affected rop calls to be delivered in its dedicated
    // port. context object can be passed as additional argument to rop call,
    // or called as function which takes a function.
    var createContext = function (pid) {
        var ctx = function (func) {
            var opid = portId
            portId = pid
            try {
                func()
            } catch (e) {
                portId = opid
                throw e
            }
            portId = opid
        }
        ctx.portId = pid
        return ctx
    }

    // default context which is exposed as rop.async and rop.sync
    var defaultAsyncContext = createContext(createAsyncPortId())
    var defaultSyncContext = createContext(createSyncPortId())

    var portId = undefined

    // current registry where skeleton is processing the request
    var registry = undefined

    var exports = {}

    var addExportable = function (name, obj) {
        if (!obj) {
            obj = name
            name = obj.type
        }
        if (!name || !obj) throw "Illegal argument"
        if (!obj.type) throw 'Cannot export object without type property'
        log.info('exporting '+name)
        exports[name] = obj
    }

    var removeExportable = function (name) {
        delete exports[name]
    }

    var nextSkeletonId = 1
    var Registry = defineClass({
        init: function (mode) {
            this.remotes = {}
            this.skeletons = {
                0: { // registry skeleton
                    id: 0,
                    object: this,
                    type: [undefined, undefined, [
                        "__getRemote", [STRING], [I32],
                        "__notifyRemoteDestroy", [I32, I32], undefined
                    ]]
                }
            }
            this.transport = undefined
            this.localData = {}
            this.disposeListeners = []
            this.ports = {}
            this.portCreateCount = 0
            this.mode = (mode || 'browser')

            if (log.level >= log.TRACE) {
                var self = this
                setInterval(function () {
                    log.trace('BEGIN STATUS')
                    var i, c = 0, d = 0
                    for (i in self.remotes) { c++ }
                    log.trace('    remotes: '+c)
                    c = 0
                    for (i in self.skeletons) { c++}
                    log.trace('    skels: '+c)
                    c = 0
                    for (i in self.ports) { c++}
                    log.trace('    ports: '+c)
                    log.trace('END STATUS')
                }, 5000)
            }
        },
        get mode () { return this.__mode },
        set mode (m) {
            switch (m) {
            case 'node':
                this.defaultContext = defaultAsyncContext
                this.localCallExecutor = LocalCall.prototype.callAsync
                break
            case 'browser':
                this.defaultContext = defaultSyncContext
                this.localCallExecutor = LocalCall.prototype.callSync
                break
            default:
                throw 'Unknown mode '+m
            }
            this.__mode = m
        },
        getPort: function (pid) {
            if (!pid) {
                // use default prot id if initially calling rop request,
                // or under rop context other than this repository
                if (portId === undefined || (registry && registry !== this)) {
                    pid = this.defaultContext.portId
                } else {
                    pid = portId
                }
            }

            var p = this.ports[pid]
            if (!p) {
                if (++this.portCreateCount > 20) {
                    log.debug("trimming ports")
                    this.portCreateCount = 0
                    for (var _pid in this.ports) {
                        var _p = this.ports[_pid]
                        if (_p.isActive()) continue
                        delete this.ports[_pid]
                        log.debug("removed port:"+_pid)
                    }
                }

                p = new Port(pid, this)
                this.ports[pid] = p
            }
            return p
        },
        getRemote: function (id, cb, ctx) {
            if (id.constructor === Number) {
                var r = this.remotes[id]
                if (r === undefined) {
                    r = new Remote()
                    r.id = id
                    r.registry = this
                    this.remotes[id] = r
                }
                return r
            }

            ctx = ctx || cb // handle sync call in node mode
            var p = this.getPort(ctx && ctx.portId)
            p.writer = function (buf) {
                writeRequest(buf, 0, 0, 0, [id], [STRING])
            }

            var self = this
            if (p.isAsync()) {
                // call in asynchronous way
                if (!cb) throw 'No return callback given'
                var ret = new Return([I32], function (st, r) {
                    if (st === 0) {
                        r = self.getRemote(-r)
                        r.count++
                    }
                    cb(st, r)
                })
                log.debug('async 0.getRemote('+id+') to port: '+p.id)
                p.send(ret)
                return
            }

            var ret = new Return([I32])
            log.debug('0.getRemote('+id+') to port: '+p.id)
            p.sendAndWait(ret)
            log.debug("0.getRemote returns "+ret.index)
            if (ret.index === 0) {
                var r = this.getRemote(-ret.value)
                r.count++
                return r
            } else {
                log.error("Failed to get remote object named "+id)
                throw "No such object"
            }
        },
        "__getRemote": function (name, ret, ctx) {
            var ex = exports[name]
            if (ex === undefined) {
                if (ctx) ret(-1)
                return -1
            }
            var skel = this.getSkeletonBy(ex)
            skel.count++
            log.info("getRemote("+name+") returns "+skel.id)
            if (ctx) ret(0, skel.id)
            return skel.id
        },
        notifyRemoteDestroy: function (id, count) {
            log.info("no longer using remote object:"+id+" count:"+count)
            var self = this
            var args = arguments
            defaultAsyncContext( function () {
                var p = self.getPort(0)
                p.writer = function (buf) {
                    writeRequest(buf, 1<<6, 0, 1, args, [I32, I32])
                }
                p.send()
                self.remotes[id].registry = undefined
                delete self.remotes[id]
            })
        },
        "__notifyRemoteDestroy": function (id, cnt) {
            id = -id
            var skel = this.getSkeleton(id)
            if (!skel) throw "Remote destroy notified for unknown obj:"+id
            skel.count -= cnt
            log.trace("no longer used by remote:"+id)
            if (skel.count === 0) {
                this.skeletons[id] = undefined
                skel.object.__skeletonId = undefined
            }
        },
        getSkeleton: function (id) {
            return this.skeletons[id]
        },
        getSkeletonBy: function (obj, type) {
            var id = obj.__skeletonId
            if (id === undefined) {
                id = nextSkeletonId++
                obj.__skeletonId = id
            }

            var skel = this.skeletons[id]
            if (skel) return skel

            skel = new Skeleton(obj, type)
            if (obj.type) skel.type = getType(obj.type)

            skel.id = id
            this.skeletons[id] = skel
            return skel
        },
        getLocal: function (key) {
            return this.localData[key]
        },
        setLocal: function (key, val) {
            this.localData[key] = val
        },
        addRegistryListener: function (l) {
            if (this.disposeListeners.indexOf(l) >= 0) return
            this.disposeListeners.push(l)
        },
        removeRegistryListener: function (l) {
            var lnrs = this.disposeListeners
            var i = lnrs.indexOf(l)
            if (i === -1) return
            lnrs.splice(i, 1)
        }
    })

    var AbstractTransport = defineClass({
        __io: undefined,
        init: function (opts) {
            var io = opts.io
            this.registry = new Registry(opts.mode || (
                    io.exchange ? 'browser' : 'node'))
            this.registry.transport = this
            this.outBuffer = new RopBuffer()
            this.exchangeSessions = {}

            var sessionId, onready, ws

            this.__io = io
            var self = this
            // replace onclose
            io.onclose = function () {
                log.info('Closing transport')
                self.__io = undefined
                var lnrs = self.registry.disposeListeners
                self.registry.disposeListeners = []
                for (var i = 0; i < lnrs.length; i++) {
                    var lnr = lnrs[i]
                    try {
                        lnr()
                    } catch (e) {
                        log.error(e)
                    }
                }
                io.onmessage = undefined
                io.onclose = undefined
            }
            io.onmessage = function (msg, resp) {
                self.receive(msg, resp)
            }
        },
        send: function (p) {
            // construct request message from port
            var buf = this.outBuffer
            buf.port = p
            buf.writeI32(p.id)
            if (p.writer) {
                try {
                    p.writer(buf)
                } catch (e) {
                    p.writer = undefined
                    buf.clear()
                    throw e
                }

                p.writer = undefined
            }

            var sess = this.exchangeSessions[p.id]
            if (sess) {
                switch ((buf.peek(4) & 0xff) >> 6) {
                case 0: sess.callDepth++; break
                case 3: sess.callDepth--; break
                }
                if (sess === this.currentExchangeSession &&
                        sess.responses.length === 0) {
                    // corresponding respond function available
                    this.pendingRespond(buf)
                    buf.clear()
                    this.pendingRespond = undefined
                    this.currentExchangeSession = undefined

                    // close session
                    if (sess.callDepth == 0) {
                        delete this.exchangeSessions[p.id]
                    }

                } else {
                    var buf2 = new RopBuffer(buf)
                    buf.clear()
                    sess.responses.push(buf2)
                }
            } else {
                this.__io.send(buf)
                buf.clear()
            }
        },
        receive: function (buf, resp) {
            if (buf.size() < 4) throw "Empty message"

            var pid = -buf.readI32()
            log.debug("Reading "+buf.size() + " from port:"+pid)
            if (log.level >= log.TRACE) {
                log.trace(buf.dump())
            }

            var sess
            if (resp) {
                sess = this.exchangeSessions[pid]
                if (!sess) {
                    sess = {
                        portId: pid,
                        callDepth: 0,
                        responses: []
                    }
                    this.exchangeSessions[pid] = sess
                }

                // client may send empty message to get response only.
                if (buf.size() === 0) {
                    this.sendNextResponse(resp, sess)
                    return
                }
            }

            var p = this.registry.getPort(pid)
            buf.port = p
            var retOrReq = p.readMessage(buf)
            if (buf.size() > 0) {
                log.warn("Received message has trailing " + buf.size() +
                        " bytes")
                buf.clear()
            }
            if (retOrReq.constructor === Return) {
                var ret = retOrReq
                if (ret.callback) {
                    try {
                        ret.callback.call(undefined, ret.index, ret.value)
                    } catch (e) {
                        log.error(e)
                    }
                }
                if (resp) sess.callDepth--
            } else {
                var lc = retOrReq
                if (resp && (((lc.messageHead>>6) & 3) === 0)) {
                    sess.callDepth++
                }
            }

            if (resp) {
                if (sess.callDepth === 0) {
                    delete this.exchangeSessions[pid]
                    resp(undefined)
                } else {
                    this.sendNextResponse(resp, sess)
                }
            }

            p.processRequests()
        },
        sendNextResponse: function (resp, sess) {
            var pendings = sess.responses
            if (pendings.length === 0) {
                this.pendingRespond = resp
                this.currentExchangeSession = sess
                return
            }

            resp(pendings.shift())

            if (sess.callDepth == 0 && pendings.length == 0) {
                delete this.exchangeSessions[sess.portId]
            }
        },
        sendAndReceive: function (p) {
            // send request and wait for response via xmlhttprequest

            // construct request message from port
            var buf = this.outBuffer
            buf.port = p
            buf.writeI32(p.id)
            if (p.writer) {
                try {
                    p.writer(buf)
                } catch (e) {
                    p.writer = undefined
                    buf.clear()
                    throw e
                }
                p.writer = undefined
            }
            if (log.level >= log.TRACE) {
                log.trace('send message dump:'+buf.size())
                log.trace(buf.dump())
            }

            var buf = this.__io.exchange(buf)
            if (buf !== this.outBuffer) this.outBuffer.clear()
            if (buf === undefined) return

            buf.port = p

            var pid = -buf.readI32()
            if (pid !== p.id) {
                log.error("xhr response has a request to other port:"+pid)
            }
            log.debug("Reading "+buf.size() + " from port:"+pid)
            if (log.level >= log.TRACE) {
                log.trace(buf.dump())
            }

            var retOrReq = p.readMessage(buf)
            if (buf.size() > 0) {
                log.warn("Received message has trailing " + buf.size() +
                        " bytes")
                buf.clear()
            }

            if (retOrReq.constructor !== Return) return
            var ret = retOrReq
            if (ret.callback) {
                // should never occur.
                log.error("Got async return via xhr.");
                setTimeout(function () {
                    ret.callback.call(undefined, ret.index, ret.value)
                }, 0)
            }
        },
        close: function () {
            if (this.__io) this.__io.close()
        }
    })

    var Port = defineClass({
        init: function (pid, reg) {
            this.registry = reg
            this.id = pid
            this.context = createContext(pid)
            this.__otherId = undefined
            this.returns = []
            this.localCalls = []
            //this.lastLocalCall = undefined
            this.writer = undefined
            this.processingState = 0 // 0:idle, 1:processing, 2:async
            if (this.isAsync()) {
                var self = this
                this.processRequestsOnTimeout = function () {
                    self.processRequests()
                }
            }
        },
        isAsync: function () {
            if (this.id > 0) {
                return (this.id & 1) === 1
            } else {
                return this.registry.defaultContext === defaultAsyncContext
            }
        },
        get otherId () {
            var oid = this.__otherId
            if (oid === undefined) {
                if (this.registry.defaultContext === defaultAsyncContext) {
                    oid = createAsyncPortId()
                } else {
                    oid = createSyncPortId()
                }
                this.__otherId = oid
            }
            return oid
        },
        send: function (ret) {
            if (ret) this.returns.push(ret)
            if (this.isAsync()) {
                this.registry.transport.send(this)
            } else {
                this.registry.transport.sendAndReceive(this)
            }
        },
        sendAndWait: function (ret) {
            if (this.isAsync()) {
                // TODO: flush port-out
                throw "Cannot use synchronous API in async mode"
            }

            this.returns.unshift(ret)
            do {
                this.registry.transport.sendAndReceive(this)
                this.processRequest(this.registry.localCallExecutor, false)
            } while (ret.index === undefined)
        },
        processRequests: function () {
            while(this.processRequest(this.registry.localCallExecutor, true)) {
                if (this.processingState === 1) {
                    this.processingState = 2
                    break
                }
            }
        },
        processRequest: function (executor, sendReturn) {
            if (this.processingState !== 0) return false
            var lc = this.localCalls.shift()
            if (!lc) return false
            this.processingState = 1
            //var llc = this.lastLocalCall // not being used currently
            //this.lastLocalCall = undefined

            var opid = portId
            var oreg = registry

            var ctx
            if (((lc.messageHead>>6) & 3) === 0) {
                portId = this.id
                ctx = this.context
            } else {
                portId = this.otherId
                ctx = this.registry.getPort(portId).context
            }
            registry = this.registry

            log.info(lc.object.__skeletonId+"."+lc.method+" in port: "+this.id)
            var self = this
            executor.call(lc, ctx, function (ri, rv) {
                var ret = lc.ret
                if (ret) {
                    ret.index = ri
                    ret.value = rv
                    self.writer = function (buf) {
                        ret.write(buf)
                    }
                    if (sendReturn) self.registry.transport.send(self)
                }

                if (self.processingState === 2) {
                    setTimeout(self.processRequestsOnTimeout, 0)
                }
                self.processingState = 0 // back to idle
            })

            portId = opid
            registry = oreg
            //this.lastLocalCall = lc
            return true
        },
        isActive: function () {
            if (this.returns.length > 0) {
                return true
            }
            if (this.localCalls.length > 0) {
                return true
            }
            if ((this.writer && this.writer.cont) ||
                    (this.reader && this.reader.cont)) {
                return true
            }
            return false
        },
        readMessage: function (buf) {
            var h = buf.read()
            switch (h >> 6) {
            case 3: // return
                var r = this.returns.shift()
                if (!r) {
                    log.error("No return waiting.")
                    return
                }
                r.loadReturn(h, buf)
                return r

            default: // local call
                var oid = -buf.readI32()
                if (oid < 0) {
                    log.error("Expected positive obj id (got "+oid+")")
                    return
                }
                var mid = buf.readI16()

                var skel = this.registry.getSkeleton(oid)
                if (!skel) throw "No such skeleton. oid:"+oid
                var methods = skel.type[2]
                var base = mid * 3
                if (base+2 >= methods.length) {
                    throw "Method index out of range:"+mid
                }
                var lc = new LocalCall(skel.object, methods[base],
                        methods[base+1], methods[base+2])
                lc.messageHead = h
                lc.loadArguments(buf)
                log.trace("Got request:"+lc+" in port:"+ this.id)
                this.localCalls.push(lc)
                return lc
            }
        }
    })

    var Return = defineClass({
        init: function (retTypes, callback) {
            this.returnTypes = retTypes
            this.callback = callback
            //this.index = undefined
            //this.value = undefined
        },
        loadReturn: function (messageHead, buf) {
            var idx = messageHead & 63
            this.index = (idx === 63) ? -1 : idx

            if (!this.returnTypes[this.index]) return
            this.value = readAs(this.returnTypes[this.index], buf)
        },
        write: function (buf) {
            var idx = this.index & 63
            var rt = this.returnTypes[idx]
            if (!rt) {
                buf.writeI8((3<<6)|63, buf) // send error
                return
            }

            buf.writeI8((3<<6) | idx, buf)
            writeAs(rt, this.value, buf)
        }
    })

    var refFactory = undefined

    return {
        addExportable: addExportable,
        removeExportable: removeExportable,
        types: Types,
        codecs: Codecs,
        createStub: createStub,
        getStubPrototype: getStubPrototype,
        getConsts: getConsts,
        async: defaultAsyncContext,
        sync: defaultSyncContext,
        createAsyncContext: function () {
            return createContext(createAsyncPortId())
        },
        createSyncContext: function () {
            return createContext(createSyncPortId())
        },
        AbstractTransport: AbstractTransport,
        isException: isException,
        Exception: Exception,
        isStub: function (s) { return (s && s.__isStub) },
        currentPortId: function () { return portId },
        currentRegistry: function () { return registry },
        defineClass: defineClass
    }
})
