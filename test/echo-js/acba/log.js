'use strict'
acba.define('js:acba/log', ['js:log'], function (log) {
    return function (name, ret) {
        ret(log.spawn(name))
    }
})
