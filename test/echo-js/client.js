'use strict'
require('./acba')
require('./log')
require('./acba/log')
require('../../node/rop')
require('../../node/rop_buffer')
require('../../js/rop_base')
require('./roptypes')

var callback = {
    type: 'test.Callback',
    // @rop void run (String msg);
    run: function (msg, ret) {
        console.log('Got callback msg:'+msg)
        test.echo(msg, ret)
    },
}

var test
acba.require(['js:rop', 'js:roptypes'], function (rop) {

    var trans = new rop.SocketTransport({
        host: 'localhost:1390'
    })

    trans.onopen = function () {
        trans.registry.getRemote('test.Test', function (ri, rv) {
            test = rop.createStub('test.Test', rv)
            test.setCallback(callback)
        })
    }
})
