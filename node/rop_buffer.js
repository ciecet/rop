"use strict";
acba.define('js:rop_buffer', ['log:rop'], function (log) {
    // Prevent global access for peformance
    var String = acba("global")["String"]

    var RopBuffer = function (szStrBuf) {
        this.constructor = RopBuffer
        this.rpos = 0
        this.wpos = 0
        if (szStrBuf === undefined || typeof(szStrBuf) === 'number') {
            this.buffer = new Buffer(szStrBuf || this.DEFAULT_SIZE)
            this.capacity = this.buffer.length
            return
        }
        if (szStrBuf.constructor === String) {
            this.buffer = new Buffer(szStrBuf, 'binrary')
            this.capacity = this.buffer.length
            this.wpos = this.capacity
            return
        }
        if (szStrBuf.constructor === RopBuffer) {
            this.buffer = new Buffer(szStrBuf.size())
            szStrBuf.buffer.copy(this.buffer, 0, szStrBuf.rpos, szStrBuf.wpos)
            this.capacity = this.buffer.length
            this.wpos = this.capacity
            return
        }
        if (szStrBuf.constructor === Buffer) {
            this.buffer = szStrBuf
            this.capacity = this.buffer.length
            this.wpos = this.capacity
            return
        }
        throw 'Illegal argument:'+szStrBuf
    }

    RopBuffer.prototype = {
        DEFAULT_SIZE: 4*1024,
        read: function () { return this.buffer[this.rpos++] },
        write: function (i) { this.buffer[this.wpos++] = i },
        peek: function (i) { return this.buffer[this.rpos + i] },
        margin: function () { return this.capacity - this.wpos },
        drop: function (s) { this.rpos += s },
        grow: function (s) { this.wpos += s },
        size: function () { return this.wpos - this.rpos },
        clear: function () { this.wpos = this.rpos = 0 },
        ensureMargin: function (m) {
            if (this.margin() >= m) return

            // to begin with, try to move payload to front.
            var mincap = this.wpos - this.rpos + m
            if (this.rpos >= this.capacity / 2 &&
                    mincap <= this.capacity) {
                log.info("compacting buffer... off:" + this.rpos +
                        " cap:" + this.capacity)
                this.buffer.copy(this.buffer, 0, this.rpos, this.wpos)
                this.wpos -= this.rpos
                this.rpos = 0
                return
            }

            // reallocate buffer
            log.info("enlarging buffer... mincap:" + mincap +
                    " cap:" + this.capacity)
            var newcap = this.capacity * 2
            while (newcap < mincap) { newcap *= 2 }
            var oldbuf = this.buffer
            this.buffer = new Buffer(newcap)
            this.capacity = this.buffer.length
            oldbuf.copy(this.buffer, 0, this.rpos, this.wpos)
            this.wpos -= this.rpos
            this.rpos = 0
        },
        readI8: function () {
            // seems faster than dataview.getInt8
            return this.buffer[this.rpos++] << 24 >> 24
        },
        writeI8: function (v) {
            this.ensureMargin(1)
            this.buffer[this.wpos++] = v
        },
        readI16: function () {
            var v = this.buffer.readInt16BE(this.rpos, true)
            this.rpos += 2
            return v
        },
        writeI16: function (v) {
            this.ensureMargin(2)
            this.buffer.writeInt16BE(v, this.wpos, true)
            this.wpos += 2
        },
        readI32: function () {
            var v = this.buffer.readInt32BE(this.rpos, true)
            this.rpos += 4
            return v
        },
        readUI32: function () {
            var v = this.buffer.readUInt32BE(this.rpos, true)
            this.rpos += 4
            return v
        },
        writeI32: function (v) {
            this.ensureMargin(4)
            this.buffer.writeInt32BE(v, this.wpos, true)
            this.wpos += 4
        },
        readF32: function () {
            var v = this.buffer.readFloatBE(this.rpos, true)
            this.rpos += 4
            return v
        },
        writeF32: function (v) {
            this.ensureMargin(4)
            this.buffer.writeFloatBE(v, this.wpos, true)
            this.wpos += 4
        },
        readF64: function () {
            var v = this.buffer.readDoubleBE(this.rpos, true)
            this.rpos += 8
            return v
        },
        writeF64: function (v) {
            this.ensureMargin(8)
            this.buffer.writeDoubleBE(v, this.wpos, true)
            this.wpos += 8
        },
        /* node specific */
        readSubbuffer: function (len) {
            if (len === undefined) len = this.size()
            var e = this.rpos + len
            var b = this.buffer.slice(this.rpos, e)
            this.rpos = e
            return b
        },
        /* node specific */
        writeBuffer: function (buf) {
            this.ensureMargin(buf.length)
            buf.copy(this.buffer, this.wpos)
            this.wpos += buf.length
        },
        /* writeArray: function (arr) {
            this.ensureMargin(arr.length)
            this.byteArray.set(arr, this.wpos)
            this.wpos += arr.length
        }, */
        readString: function (len) {
            var e = this.rpos + len
            var str = this.buffer.toString('utf8', this.rpos, e)
            this.rpos = e
            return str
        },
        writeString: function (v) {
            var len = Buffer.byteLength(v, 'utf8')
            this.writeI32(len)
            this.ensureMargin(len)
            this.buffer.write(v, this.wpos, len, 'utf8')
            this.wpos += len
        },
        dump: function () {
            var arr = []
            for (var i = this.rpos; i < this.wpos; i++) {
                arr.push(this.buffer[i])
            }
            return new String(arr)
        }
    }

    return RopBuffer
})
