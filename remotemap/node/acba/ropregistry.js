'use strict'
acba.define('js:acba/ropregistry', [
    'log:ropregistry',
    'js:rop'
], function (log, rop) {

    return function (loc, ret) {
        console.log('Creating transport for '+loc)
        if (!loc.match(/^rop:\/\//)) {
            console.warn('Unsupported transport type')
            return ret(acba.FAILED)
        }

        var trans = new rop.SocketTransport(loc.substring(6))
        trans.onopen = function () {
            ret(trans.registry)
        }
        trans.onerror = function () {
            ret(acba.FAILED)
        }
    }
})
