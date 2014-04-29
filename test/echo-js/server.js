'use strict'
require('./acba')
require('./log')
require('./acba/log')
require('../../node/rop')
require('../../node/rop_buffer')
require('../../js/rop_base')
require('./roptypes')

acba.require(['js:rop', 'js:roptypes'], function (rop, conf) {

    var test = {
        type: 'test.Test',
        // @rop async setCallback (Callback cb);
        setCallback: function (cb) {
            setTimeout(function () {
                console.log('Calling callback')
                cb.run('Hello', function (ri) {
                    console.log('Callback returned '+ri)
                })
            }, 2000)
        },
        // @rop String echo (String msg);
        echo: function (msg, ret) {
            console.log('echo:'+msg)
            ret(0, msg)
        },
    }

    rop.addExportable(test);

    var httpServer = new rop.HttpServer(8090)
    var socketServer = new rop.SocketServer(1390)
})
