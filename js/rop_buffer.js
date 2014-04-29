"use strict";
acba.define('js:rop_buffer', ['log:rop'], function (log) {
    // Prevent global access for peformance
    var String = acba("global")["String"]

    var Buffer = function (szStrBuf) {
        this.rpos = 0
        this.wpos = 0
        if (szStrBuf === undefined || typeof(szStrBuf) === 'number') {
            this.buffer = new ArrayBuffer(szStrBuf || this.DEFAULT_SIZE)
            this.updateView()
            return
        }
        if (szStrBuf.constructor === String) {
            this.buffer = new ArrayBuffer(szStrBuf.length)
            this.updateView()
            this._writeRawString(szStrBuf)
            return
        }
        if (szStrBuf.constructor === Buffer) {
            this.buffer = szStrBuf.buffer.slice(szStrBuf.rpos, szStrBuf.wpos)
            this.updateView()
            this.wpos = this.capacity
            return
        }
        if (szStrBuf.constructor === ArrayBuffer) {
            this.buffer = szStrBuf
            this.updateView()
            this.wpos = this.capacity
            return
        }
        throw 'Illegal argument:'+szStrBuf
    }

    Buffer.prototype = {
        constructor: Buffer,
        DEFAULT_SIZE: 4*1024,
        updateView: function () {
            this.byteArray = new Uint8Array(this.buffer)
            this.dataView = new DataView(this.buffer)
            this.capacity = this.byteArray.length
        },
        read: function () { return this.byteArray[this.rpos++] },
        write: function (i) { this.byteArray[this.wpos++] = i },
        peek: function (i) { return this.byteArray[this.rpos + i] },
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
                this.byteArray.set(this.byteArray.subarray(
                        this.rpos, this.wpos))
                this.wpos -= this.rpos
                this.rpos = 0
                return
            }

            // reallocate buffer
            log.info("enlarging buffer... mincap:" + mincap +
                    " cap:" + this.capacity)
            var newcap = this.capacity * 2
            while (newcap < mincap) { newcap *= 2 }
            var oldByteArray = this.byteArray
            this.buffer = new ArrayBuffer(newcap)
            this.updateView();
            this.byteArray.set(oldByteArray.subarray(
                    this.rpos, this.wpos))
            this.wpos -= this.rpos
            this.rpos = 0
        },
        readI8: function () {
            // seems faster than dataview.getInt8
            return this.byteArray[this.rpos++] << 24 >> 24
        },
        writeI8: function (v) {
            this.ensureMargin(1)
            this.byteArray[this.wpos++] = v
        },
        readI16: function () {
            var v = this.dataView.getInt16(this.rpos)
            this.rpos += 2
            return v
        },
        writeI16: function (v) {
            this.ensureMargin(2)
            this.dataView.setInt16(this.wpos, v)
            this.wpos += 2
        },
        readI32: function () {
            var v = this.dataView.getInt32(this.rpos)
            this.rpos += 4
            return v
        },
        readUI32: function () {
            var v = this.dataView.getUint32(this.rpos)
            this.rpos += 4
            return v
        },
        writeI32: function (v) {
            this.ensureMargin(4)
            this.dataView.setInt32(this.wpos, v)
            this.wpos += 4
        },
        readF32: function () {
            var v = this.dataView.getFloat32(this.rpos)
            this.rpos += 4
            return v
        },
        writeF32: function (v) {
            this.ensureMargin(4)
            this.dataView.setFloat32(this.wpos, v)
            this.wpos += 4
        },
        readF64: function () {
            var v = this.dataView.getFloat64(this.rpos)
            this.rpos += 8
            return v
        },
        writeF64: function (v) {
            this.ensureMargin(8)
            this.dataView.setFloat64(this.wpos, v)
            this.wpos += 8
        },
        /* browser specific */
        readSubarray: function (len) {
            if (len === undefined) len = this.size()
            var a = this.byteArray.subarray(this.rpos, this.rpos+len)
            this.rpos += len
            return a
        },
        _writeRawString: function (str) {
            this.ensureMargin(str.length)
            var w = this.wpos
            var e = w + str.length
            var i = 0
            while (w < e) {
                this.byteArray[w++] = str.charCodeAt(i++)
            }
            this.wpos = e
        },
        /* writeArray: function (arr) {
            this.ensureMargin(arr.length)
            this.byteArray.set(arr, this.wpos)
            this.wpos += arr.length
        }, */
        readString: function (len) {
            return decodeURIComponent(escape(String.fromCharCode.apply(String,
                    this.readSubarray(len))))
        },
        writeString: function (v) {
            var str = unescape(encodeURIComponent(v)); // encode to utf8
            this.writeI32(str.length)
            this._writeRawString(str)
        },
        dump: function () {
            var arr = []
            for (var i = this.rpos; i < this.wpos; i++) {
                arr.push(this.byteArray[i])
            }
            return new String(arr)
        }
    }

    return Buffer
})
