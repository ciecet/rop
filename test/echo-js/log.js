'use strict';

/**
 * A module for printing log messages into console.
 *
 * Since this module was integrated with acba module, you can get
 * module-specific log object directly from the acba like following:
 *
 *     acba.define("module_name", ["log:module_name"], function(log) {
 *         log.level = log.DEBUG
 *         log.debug("This is a debug level message")
 *         ....
 *     })
 *
 * @class Log
 * @static
 */
acba.define('js:log', function () {
    var ERROR = 0
    var WARN = 1
    var INFO = 2
    var DEBUG = 3
    var TRACE = 4
    var funcs = ["error", "warn", "log", "log", "log"]
    var marks = ["!", "?", "#", " ", " "]

    var print = function (lvl) {
        return function (o) {
            if (this.level < lvl) return
            var f = funcs[lvl] || "error"
            var prefix = marks[lvl] + " " + this.name+": "
            console[f](prefix+o)
            if (typeof(o) !== "object") return
            if (o.name) {
                console[f](prefix+"    name: "+o.name)
            }
            if (o.message) {
                console[f](prefix+"    message: "+o.message)
            }
            if (o.stack) {
                console[f](prefix+"    stack: "+o.stack)
            }
        }
    }

    var inspect = function(o) {
        var msg = ""
        for (var k in o) {
            if (o.hasOwnProperty(k)) {
                msg+= k+" = "+o[k]+"\n"
            }
        }
        return msg
    }

    var logRoot = {
        /**
         * Log level constant.
         *
         * @property ERROR
         * @type Number
         * @default 0
         */
        ERROR: ERROR,
        /**
         * Log level constant.
         *
         * @property WARN
         * @type Number
         * @default 1
         */
        WARN: WARN,
        /**
         * Log level constant.
         *
         * @property INFO
         * @type Number
         * @default 2
         */
        INFO: INFO,
        /**
         * Log level constant.
         *
         * @property DEBUG
         * @type Number
         * @default 3
         */
        DEBUG: DEBUG,
        /**
         * Log level constant.
         *
         * @property TRACE
         * @type Number
         * @default 4
         */
        TRACE: TRACE,
        /**
         * This Log object's verbosity level. INFO by default.
         *
         * @property level
         * @type Number
         * @default INFO
         */
        level: DEBUG,
        name: "",
        /**
         * Create another log object inherited from this.
         *
         * @method spawn
         * @param {String} mod module name.
         * @param {log level constant} lvl value for specifying log level.
         * @return {Log} return a new Log object.
         */
        spawn: function (mod, lvl) {
            var log = Object.create(this)
            log.name = this.name + mod
            if (lvl) {
                log.level = lvl
            }
            return log
        },
        /**
         * Print log as ERROR log level.
         *
         * @method error
         * @param {String} o if not object type, return silently.
         */
        error: print(ERROR),
        /**
         * Print log as WARN log level.
         *
         * @method warn
         * @param {String} o if not object type, return silently.
         */
        warn: print(WARN),
        /**
         * Print log as INFO log level.
         *
         * @method info
         * @param {String} o if not object type, return silently.
         */
        info: print(INFO),
        /**
         * Print log as DEBUG log level.
         *
         * @method debug
         * @param {String} o if not object type, return silently.
         */
        debug: print(DEBUG),
        /**
         * Print log as TRACE log level.
         *
         * @method trace
         * @param {String} o if not object type, return silently.
         */
        trace: print(TRACE),
        /**
         * Return details on the given object.
         *
         * @method inspect
         * @param {Object} o object to be inspected.
         */
        inspect: inspect
    }

    return logRoot
})
