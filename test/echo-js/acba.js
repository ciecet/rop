// AltiCast BAse foundation
//
// this provides basic module management function.
// a module is a arbitrary value which is addressiable by its name in
// system-wide.

"use strict";

/**
 * Usage:
 * 
 * acba.define("modname", ["list of required modules", ...], <generator>)
 *
 *      defines a module named "modname" with given <generator>.
 *      <generator> can be either function or arbitrary object.
 *      in case of function call, module objects are passed as arguemnts
 *      which corresponds to the "list of required modules".
 *      the function may return the module object directly or undefined vaule
 *      to indicate that the module is being loaded asynchronously,
 *      when the module object became valid, generator function should call
 *      acba.set("modname", <modobj>).
 *
 * acba.require(["list of required modules", ...], onsuccess, onerror)
 *
 *      Execute the given function with module objects when they are all
 *      avaialble. It could be synchronous or asynchronous depending on the
 *      availablity of required modules.
 *      Note that synchronous behavior doens't propagate raised exception.
 *
 * acba.get("modname") (aliased as acba('modname'))
 *
 *      Returns module object of the given name synchronously.
 *      If not found or can be passed later, it throws an exception.
 *
 * acba.tryGet("modname")
 *
 *      The same as acba.get() but doesn't throw an exception.
 *      Instead, this may return acba.FAILED or acba.LOADING
 *
 * When given name of module is in the form of "rop:<type>[:<name>]",
 * acba implicitly defines the module object as stub of given information,
 * which is transferred from the module object "js:defaultRopTransport"
 * If <name> is omitted, <type> is used for <name> as well.
 *
 * When given name of module is in the form of "js:<name>" or "js:<url>",
 * acba implicitly loads the definition from the external javascript file.
 * if <name> was given, the url will be acba.jsbase+<name>+".js"
 */
(function (global){

    // silently return if acba was loaded already.
    if (typeof(global.acba) === "function") return
    var acbaProperties = global.acba

    // special node for representing module status.
    var LOADING = ["LOADING"]
    var FAILED = ["FAILED"]

    // module registry mapping from module name to module object.
    // exceptionally, object could be LOADING or FAILED to indicate the
    // current state.
    var modules = {
        "global": global
    }

    // loads module from given (canonical) name
    var loaders = []

    // pending define requests which were not yet evaluated.
    var defines = {}

    // pending require requests which were not yet executed.
    var requires = {}

    var printException = function (e) {
        if (e.constructor === String) {
            console.log(e)
            return
        }

        if (e.name || e.message || e.stack) {
            console.warn("name: "+e.name)
            console.warn("message: "+e.message)
            console.warn("stacktrace: "+e.stack)
            return
        }

        console.log(e)
    }

    var get = function (name) {
        var m = tryGet(name)
        if (m === LOADING) {
            throw "acba.get("+name+") failed. still loading."
        }
        if (m === FAILED) {
            throw "acba.get("+name+") failed."
        }
        return m
    }

    var tryGet = function (name) {
        var m = modules[name]
        if (m !== undefined) {
            return m
        }

        var cname = aliases[name] || name
        for (var i in loaders) {
            try {
                var loading = loaders[i](cname, function (m) {
                    set(name, m)
                })
                if (!loading) continue

                m = modules[name]
                if (m === undefined) {
                    m = LOADING
                    set(name, m)
                }
                return m
            } catch (e) {
                printException(e)
                // continue to next loader
            }
        }

        console.warn("Unknown module: "+name)

        return FAILED
    }

    // returns true iff no further action necessary.
    var tryRun = function (r) {
        var rnames = r[0]
        var robjs = r[1]
        var hasLoading = false
        var hasFailed = false

        // given require was already handled.
        if (r[4]) {
            return true
        }

        for (var i in rnames) {
            var m = modules[rnames[i]]
            if (m === undefined) {
                m = tryGet(rnames[i])
            }
            robjs[i] = m
            if (m === LOADING) {
                hasLoading = true
            }
            if (m === FAILED) {
                hasFailed = true
            }
        }

        if (hasLoading && !hasFailed) return false
        r[4] = true

        try {
            if (hasFailed) {
                r[3].apply(undefined, robjs)
            } else {
                r[2].apply(undefined, robjs)
            }
        } catch (e) {
            printException(e)
        }
        return true
    }

    var set = function (name, obj) {
        var m = modules[name]
        if (m === undefined) {
            if (obj === undefined) {
                throw "Expected object, LOADING or FAILED for module "+name
            }
        } else if (m === LOADING) {
            if (obj === LOADING) {
                throw "Expected object or FAILED for module "+name
            }
        } else {
            throw "Cannot reset module "+name
        }
        modules[name] = obj
        if (obj === LOADING) return
        if (obj === FAILED) {
            console.warn("Cannot load module: "+name)
        } else if (obj === undefined) {
            console.warn("Set undefined for module: "+name)
        } else {
            console.log("Added module: "+name)
        }

        // notify to relevant requirers
        var rs = requires[name]
        if (rs === undefined) return
        delete requires[name]
        notify(rs)
    }

    var waiters = []
    var notify = function (rs) {
        if (waiters.length > 0) {
            waiters.push.apply(waiters, rs)
            return
        }
        waiters.push.apply(waiters, rs)
        while (waiters.length > 0) {
            var w = waiters.shift()
            tryRun(w)
        }
    }

    var define = function (name, reqs, gen) {
        if (modules.hasOwnProperty(name)) {
            console.warn("Module "+name+" was already added, "+
                    "still loading or once failed to load.")
            return
        }
        if (defines.hasOwnProperty(name)) {
            console.warn("Overriding previous definition of "+name)
        }
        defines[name] = [reqs, gen]
    }

    var _require = function (reqs, run, err) {
        if (typeof(run) !== "function") {
            run = function () {
                console.log("require("+reqs+") is complete.")
            }
        }
        if (typeof(err) !== "function") {
            err = function () {
                console.warn("require("+reqs+") failed w/o callback.")
            }
        }
        // last item indicate that this tuple was handled or not.
        var tuple = [reqs, new Array(reqs.length), run, err, false]
        if (!tryRun(tuple)) {
            for (var i in reqs) {
                var rname = reqs[i]
                var rs = requires[rname] || []
                rs.push(tuple)
                requires[rname] = rs
            }
        }
    }

    // try load value or its definitions
    loaders.push(function (name, ret) {
        var col = name.indexOf(':')
        if (col < 0) return false
        var prefix = name.substring(0, col)
        var rname = name.substring(col+1)

        if (prefix !== 'js') {
            _require(['js:acba/'+prefix], function (loader) {
                loader(rname, ret)
            })
            return true
        }

        if (defines.hasOwnProperty(name)) return false
        var loc

        if (global.document === undefined) {
            if (rname[0] === '/') {
                loc = rname
            } else {
                loc = acba.jsbase + rname + '.js'
            }
            console.log("loading... "+loc)
            require(loc)
        } else {
            if (rname.match(/^https?:|^file:/)) {
                loc = rname
            } else {
                loc = acba.jsbase + rname + ".js"
            }
            console.log("loading... "+loc)
            var req = new XMLHttpRequest();
            req.open("GET", loc, false);
            req.send();
            try {
                eval(req.responseText)
            } catch (e) {
                printException(e)
                return false
            }
        }

        if (!defines.hasOwnProperty(name)) {
            console.warn(loc + " does not provide definition of "+name)
            return false
        }

        // return false to call next loader
        return false
    })

    // get value from definition
    loaders.push(function (name, ret) {
        var d = defines[name]
        if (!d) return false
        delete defines[name]

        _require(d[0],
            function () { // on success
                if (typeof(d[1]) === "function") {
                    try {
                        var r = d[1].apply(undefined, arguments)
                        if (r !== undefined) {
                            ret(r)
                        }
                    } catch (e) {
                        printException(e)
                        ret(FAILED)
                    }
                } else {
                    ret(d[1])
                }
            },
            function () { // on fail
                ret(FAILED)
            }
        )
        return true
    })

    var aliases = {}

    var list = function () {
        console.log("============================================")
        console.log(" Definitions (not yet evaluated)")
        console.log("--------------------------------------------")

        for (var i in defines) {
            console.log("    "+i)
        }

        console.log("============================================")
        console.log(" Module Loading Status ")
        console.log("--------------------------------------------")

        for (var i in modules) {
            var m = modules[i]
            if (m === LOADING) {
                console.warn(i+" => LOADING")
            } else if (m === FAILED) {
                console.warn(i+" => FAILED")
            } else {
                console.log(i+" => "+typeof(m))
            }
        }

        console.log("============================================")
    }

    Object.defineProperty(global, "acba", { value: get })
    Object.defineProperties(get, {
        LOADING: {value:LOADING},
        FAILED: {value:FAILED},
        jsbase: {value:"",writable:true},
        aliases: {value:aliases},
        get: {value:get},
        tryGet: {value:tryGet},
        set: {value:set},
        define: {value:function (name, reqs, gen) {
            if (Object(reqs).constructor !== Array) {
                gen = reqs
                reqs = []
            }
            define(name, reqs, gen)
        }},
        require: {value:function (reqs, run, err) {
            if (Object(reqs).constructor === String) {
                reqs = [reqs]
            }
            _require(reqs, run, err)
        }},
        list: {value:list}
    })
    for (var k in acbaProperties) {
        Object.defineProperty(get, k, {value: acbaProperties[k]})
    }
})(Function('return this')())
