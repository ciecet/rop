'use strict'
acba.define('js:acba/ropregistry', [
    'log:ropregistry',
    'js:rop'
], function (log, rop) {

    return function (loc, ret) {
        console.log('Creating transport for '+loc)
        if (!loc.match(/^rop-http:\/\//)) {
            console.warn('Unsupported transport type')
            return ret(acba.FAILED)
        }

        try {
            var trans = new rop.HttpTransport({
                host: loc.substring(11),
                synchronous: true
            })
            trans.registry.mode = 'node'
            ret(trans.registry)
        } catch (e) {
            log.warn('Failed to make rop connection. '+loc)
            console.log(e)
            ret(acba.FAILED)
        }
    }
});
