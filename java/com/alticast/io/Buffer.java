package com.alticast.io;

import java.util.*;
import java.io.*;
import com.alticast.rop.*;

public class Buffer implements Reusable, LogConsts, Cloneable {

    // for adding prefix such as message length
    private static final int RESERVED_SIZE = 10;
    // must be bigger than RESERVED_SIZE * 2 (for heuristic compaction)
    private static final int INITIAL_CAPACITY = 512;

    protected byte[] buffer = new byte[INITIAL_CAPACITY];
    protected int begin = RESERVED_SIZE; // inclusive
    protected int end = RESERVED_SIZE; // exclusive

    public Object appData;
    public Object appData2;
    public Buffer next;
    public Buffer next2;

    public Buffer () {}

    public Buffer (Buffer buf) {
        writeBytes(buf.buffer, buf.begin, buf.size());
    }

    public byte[] getRawBuffer () { return buffer; }
    public int getReadPosition () { return begin; }
    public int getWritePosition () { return end; }

    public void reuse () {
        clear();
        appData = null;
        appData2 = null;
        next = null;
        next2 = null;
    }

    // unsafe methods (doesn't check capacity)

    protected int read () { return buffer[begin++] & 0xff; }
    public void readBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buf[off++] = buffer[begin++];
        }
    }
    protected void write (int d) { buffer[end++] = (byte)d; }
    public void grow (int s) { end += s; }
    public void drop (int s) { begin += s; }

    public int capacity () { return buffer.length; }
    public int size () { return end - begin; }
    public int margin () { return buffer.length - end; }
    public void clear () { begin = end = RESERVED_SIZE; }

    public void ensureMargin (int m) {
        if (margin() >= m) return;

        // to begin with, try to compact within buffer.
        int mincap = RESERVED_SIZE + size() + m;
        if (begin >= capacity() / 2 && mincap <= capacity()) {
            if (D) Log.debug("Compacting buffer... off:"+begin+
                    " cap:"+capacity());
            System.arraycopy(buffer, begin, buffer, RESERVED_SIZE, size());
            end = RESERVED_SIZE + size();
            begin = RESERVED_SIZE;
            return;
        }

        // reallocate
        if (D) Log.debug("Enlarging buffer... mincap:"+mincap+
                " cap:"+capacity());
        int newcap = capacity() * 2;
        while (newcap < mincap) newcap *= 2;
        byte[] nbuf = new byte[newcap];
        if (begin < RESERVED_SIZE) {
            System.arraycopy(buffer, begin, nbuf, begin, size());
        } else {
            System.arraycopy(buffer, begin, nbuf, RESERVED_SIZE, size());
            end = RESERVED_SIZE + size();
            begin = RESERVED_SIZE;
        }
        buffer = nbuf;
    }

    public boolean readBool () {
        return read() != 0;
    }
    
    public byte readI8 () {
        return (byte)read();
    }

    public short readI16 () {
        int i = read();
        i = (i << 8) + read();
        return (short)i;
    }

    public int readI32 () {
        int i = read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        return i;
    }

    public long readI64 () {
        long i = read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        i = (i << 8) + read();
        return i;
    }
    
    public float readF32 () {
        int ieee754Val = read();
        
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();

        float ret = Float.intBitsToFloat(ieee754Val);

        return ret;
    }
    
    public double readF64 () {
        long ieee754Val = read();
        
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        ieee754Val = (ieee754Val << 8) + read();
        
        double ret = Double.longBitsToDouble(ieee754Val);
        
        return ret;
    }
    
    public void writeBool (boolean b) {
        ensureMargin(1);
        write(b ? 1 : 0);
    }

    public void writeI8 (int i) {
        ensureMargin(1);
        write(i);
    }

    public void writeI16 (int i) {
        ensureMargin(2);
        write(i >> 8);
        write(i);
    }

    public void writeI32 (int i) {
        ensureMargin(4);
        write(i >> 24);
        write(i >> 16);
        write(i >> 8);
        write(i);
    }

    public void writeI64 (long i) {
        ensureMargin(8);

        write((int)(i >> 56));
        write((int)(i >> 48));
        write((int)(i >> 40));
        write((int)(i >> 32));
        write((int)(i >> 24));
        write((int)(i >> 16));
        write((int)(i >> 8));
        write((int)(i));
    }
    
    public void writeF32 (float f) {
        int i = Float.floatToIntBits(f);
        
        ensureMargin(4);
        write(i >> 24);
        write(i >> 16);
        write(i >> 8);
        write(i);
    }
    
    public void writeF64 (double d) {
        long l = Double.doubleToLongBits(d);
        
        ensureMargin(8);
        write((int)(l >> 56));
        write((int)(l >> 48));
        write((int)(l >> 40));
        write((int)(l >> 32));
        write((int)(l >> 24));
        write((int)(l >> 16));
        write((int)(l >> 8));
        write((int)(l));
    }

    public void writeBytes (byte[] buf) {
        writeBytes(buf, 0, buf.length);
    }

    public void writeBytes (byte[] buf, int off, int len) {
        ensureMargin(len);
        while (len-- > 0) {
            buffer[end++] = buf[off++];
        }
    }

    public void writeBuffer (Buffer buf) {
        writeBuffer(buf, buf.size());
    }

    public void writeBuffer (Buffer buf, int sz) {
        writeBytes(buf.buffer, buf.begin, sz);
    }

    public void preWriteI32 (int i) {
        buffer[--begin] = (byte)(i);
        buffer[--begin] = (byte)(i >> 8);
        buffer[--begin] = (byte)(i >> 16);
        buffer[--begin] = (byte)(i >> 24);
    }

    public int peek (int i) { return buffer[begin+i]; }

    public void writeTo (OutputStream os) throws IOException {
        if (D) Log.debug("send "+size()+" bytes");
        String s = "";
        for (int i = begin; i < end; i++) {
            s += (buffer[i] & 0xff)+" ";
        }
        if (D) Log.debug(s);
        os.write(buffer, begin, size());
        clear();
    }

    public void readFrom (InputStream is, int size) throws IOException {
        ensureMargin(size);
        int n = 0;
        do {
            int r = is.read(buffer, end, size - n);
            if (r == -1) {
                throw new IOException("EOF");
            }
            n += r;
            grow(r);
        } while (n < size);
    }

    /**
     * Take len bytes and returns as sub-buffer.
     * Returned buffer shares internal buffer with this instance.
     * Thus, writing on the buffer is highly discouraged.
     */
    public Buffer readBuffer (int len) {
        try {
            Buffer sbuf = (Buffer)clone();
            sbuf.end = sbuf.begin + len;
            drop(len);
            return sbuf;
        } catch (CloneNotSupportedException e) {
            return null; // never happen
        }
    }

    public void dump () {
        if (!D) return;

        StringBuffer str = new StringBuffer();
        for (int i = begin; i < end; i++) {
            str.append(buffer[i]);
            str.append(", ");
        }
        Log.debug(str);
    }

    public void dumpHex () {
        if (!D) return;

        StringBuffer str = new StringBuffer();
        for(int i = begin; i < end; i++) {
            str.append("0x"+Integer.toString(buffer[i]&0xff, 16));
            str.append(", ");
        }
        Log.debug(str);
    }

    public int readFrom (int fd) {
        ensureMargin(1);
        int r = NativeIo.tryRead(fd, buffer, end, margin());
        if (r > 0) {
            grow(r);
        }
        return r;
    }

    public int writeTo (int fd) {
        int w = NativeIo.tryWrite(fd, buffer, begin, size());
        if (D) Log.debug("write to fd:"+fd+" returned "+w);
        if (w > 0) {
            drop(w);
        }
        return w;
    }

    public void writeBase64 (Buffer buf) {
        writeBase64(buf, buf.size());
    }

    public void writeBase64 (Buffer buf, int sz) {
        int len = (sz + 2) / 3 * 4;
        ensureMargin(len);
        if (Base64.encode(buf.buffer, buf.begin, buffer, end, sz) != len) {
            throw new IllegalStateException("base64 encode size differs");
        }
        grow(len);
    }

    public void readBase64 (Buffer buf) {
        readBase64(buf, size());
    }

    public void readBase64 (Buffer buf, int sz) {
        buf.ensureMargin((sz + 3) / 4 * 3);
        buf.grow(Base64.decode(buffer, begin, buf.buffer, buf.end, sz));
        drop(sz);
    }

    public void writeRawString (String s) {
	int len = s.length();
        ensureMargin(len);
	for (int i = 0 ; i < len ; i++) {
            buffer[end+i] = (byte)s.charAt(i);
        }
        grow(len);
    }

    public String readString () {
        return readString(size());
    }

    public String readString (int len) {
        try {
            String str = new String(buffer, begin, len, "UTF8");
            drop(len);
            return str;
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            throw new UnsupportedOperationException("utf8 not supported");
        }
    }

    public void writeString (String s) {
        try {
            byte[] arr = s.getBytes("UTF8");
            writeI32(arr.length);
            writeBytes(arr, 0, arr.length);
        } catch (UnsupportedEncodingException e) {
            e.printStackTrace();
            throw new UnsupportedOperationException("utf8 not supported");
        }
    }
}
