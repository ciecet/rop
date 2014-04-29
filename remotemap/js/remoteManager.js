// find and provide remote object
'use strict'
acba.define('js:remoteManager', [
    'log:remoteManager',
    'js:config',
    'js:rop',
], function (log, conf, rop) {

    var STATE_UNREACHABLE = -1
    var STATE_DISCONNECTED = 0
    var STATE_CONNECTING = 1
    var STATE_CONNECTED = 2

    var nextHandle = 0

    // handle_key -> {
    //     valid: true until this request is valid.
    //     remoteName: name given on request,
    //     location: undefined if automatic, non-undefined if explicit,
    //     remote: remote object
    //     callback: function given on request,
    // }
    var requests = {}

    // location -> {
    //     location: location for the connection,
    //     state: one of above except STATE_RESOLVING,
    //     transport: transport instance only when connected,
    //     remotes: cache mapping remoteName to remoteObject,
    // }
    var connections = {}

    // cache for query result from remoteMap
    // mapping remoteName -> location
    // NOTE: not being used yet.
    var locationCache = {}

    var getRemote = function (conn, rname) {
        var r = conn.remotes[rname]
        if (r) return r
        if (r === null) return undefined
        conn.transport.registry.getRemote(rname, function (ri, rv) {
            if (ri !== 0) {
                conn.remotes[rname] = null
                return
            }

            conn.remotes[rname] = rv

            // increate ref to avoid destroy
            rv.ref()

            updateRequests()
        })
        return undefined
    }

    var updateRequests = function (reqs) {
        for (var h in requests) {
            process(requests[h])
        }
    }

    var makeConnection = function (conn) {
        if (conn.state !== STATE_DISCONNECTED) return
        conn.state = STATE_CONNECTING

        var loc = conn.location.match(/^([^:]*:\/\/)([^:]*):([^\/]*)(.*)/)
        switch (loc[1]) {
        case 'rop-http://':
            var host = loc[2]
            var port = loc[3]
            var path = loc[4]

            var trans = new rop.HttpTransport(host+':'+port,
                function () {
                    log.info('Connection established to '+conn.location)
                    conn.state = STATE_CONNECTED
                    conn.transport = trans
                    updateRequests()
                },
                function () {
                    log.info('Disconnected with '+conn.location)
                    conn.state = STATE_DISCONNECTED
                    conn.transport = undefined
                    conn.remotes = {}
                    makeConnection(conn)
                    updateRequests(conn)
                },
                function () {
                    log.info('Failed to connect to '+conn.location)
                    conn.state = STATE_DISCONNECTED
                    setTimeout(makeConnection, 5000, conn)
                }
            )
            trans.registry.mode = 'node'
            break

        default:
            log.warn('Gave up connection to '+conn.location+
                    '. unsupported scheme: '+loc[1])
            conn.state = STATE_UNREACHABLE
            updateRequests()
            break
        }
    }

    var getConnection = function (loc) {
        var conn = connections[loc]
        if (!conn) {
            conn = {
                location: loc,
                state: STATE_DISCONNECTED,
                transport: undefined,
                remotes: {}
            }
            connections[loc] = conn
            makeConnection(conn)
        }

        return conn.transport && conn
    }

    var getLocation = function (req) {
        // TODO: integrate with remote map
        return (req.location || remoteManager.defaultLocation)
    }

    var process = function (req) {

        if (!req.valid) return

        var loc, conn, r

        loc = getLocation(req)
        if (loc) {
            conn = getConnection(loc)
            if (conn) {
                r = getRemote(conn, req.remoteName)
            }
        }

        if (req.remote === r) return

        if (req.remote) {
            req.remote = undefined
            try {
                req.callback(undefined)
            } catch (e) {}
        }

        // the above callback could invalidate the handle.
        if (!req.valid) return

        req.remote = r

        if (req.remote) {
            try {
                req.callback(r)
            } catch (e) {}
        }
    }

    var remoteManager = {
        defaultLocation: 'rop-http://'+conf.defaultRopHost,
        // if loc is undefined, use remotemap or default location.
        monitorRemote: function (rname, loc, func) {

            // adjust arguments
            if (typeof(loc) === 'function') {
                func = loc
                loc = undefined
            }

            // create a request object
            var h = nextHandle++
            var req = {
                valid: true,
                remoteName: rname,
                location: loc,
                callback: func,
            }
            requests[h] = req

            process(req)
        },
        unmonitorRemote: function (h) {
            var req = requests[h]
            if (!req) return
            req.valid = false
            delete requests[h]
            // TODO: remove transport?
        },
    }

    return remoteManager
})
