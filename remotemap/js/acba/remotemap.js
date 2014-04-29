// find and provide remote object
// rop object retrival works synchronously so that standard api can
// work directly without waiting on callback.
// this works at cost of many sync-xhr calls.
'use strict'
acba.define('js:acba/remotemap', [
    'log:acba/remotemap',
    'js:config',
    'js:rop',
], function (log, conf, rop) {

    //log.level = log.DEBUG

    var defaultLocation = 'rop-http://'+conf.defaultRopHost
    var remotemap

    var getLocation = function (rname) {
        if (rname === 'rop.RemoteMap') {
            return 'rop-http://localhost:1303'
        }
        var locs = remotemap.findLocations(rname, 'rop-http://', rop.sync)
        if (locs.length > 0) return locs[0]
        return defaultLocation
    }

    var loader = function (name, ret) {
        var sp = name.split(':', 2)
        var rtype = sp[0]
        var rname = sp[1] || rtype

        try {
            log.debug('resolving server for '+name)
            var loc = getLocation(rname)
            log.debug('getting rop connection to '+loc)
            var reg = acba('ropregistry:'+loc)
            log.debug('getting remote object')
            ret(rop.createStub(rtype, reg.getRemote(rname, rop.sync)))
        } catch (e) {
            log.warn(e)
            ret(acba.FAILED)
        }
    }

    loader('rop.RemoteMap', function (rm) {
        if (rm === acba.FAILED) {
            acba.set('js:acba/remotemap', acba.FAILED)
            return
        }

        remotemap = rm
        acba.set('js:acba/remotemap', loader)
    })
})
