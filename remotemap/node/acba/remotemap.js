'use strict'
acba.define('js:acba/remotemap', [
    'log:acba/remotemap',
    'js:config',
    'js:rop',
], function (log, conf, rop) {

    var defaultLocation = 'rop://localhost:'+conf.ropSocketServerPort
    var remotemap

    var getLocation = function (rname, ret) {
        if (rname === 'rop.RemoteMap') {
            return ret(0, 'rop://localhost:1302')
        }
        remotemap.findLocations(rname, 'rop://', function (ri, rv) {
            if (ri === 0) {
                if (rv.length > 0) {
                    return ret(0, rv[0])
                } else {
                    return ret(0, defaultLocation)
                }
            }
            ret(-1)
        }, rop.async)
    }

    var loader = function (name, ret) {
        var sp = name.split(':', 2)
        var rtype = sp[0]
        var rname = sp[1] || rtype

        log.debug('Resolving server for '+name)
        getLocation(rname, function (ri, loc) {
            if (ri !== 0) return ret(acba.FAILED);
            log.debug('Getting rop connection to '+loc)
            acba.require(['ropregistry:'+loc],
                function (reg) {
                    log.debug('Getting remote object')
                    reg.getRemote(rname, function (ri, rv) {
                        if (ri !== 0) {
                            log.warn('Failed to get remote object')
                            return ret(acba.FAILED)
                        }
                        ret(rop.createStub(rtype, rv))
                    })
                },
                function () {
                    ret(acba.FAILED);
                }
            )
        })
    }

    loader('rop.RemoteMap', function (rm) {
        remotemap = rm // can be acba.FAILED
        acba.set('js:acba/remotemap', loader)
    })
})
