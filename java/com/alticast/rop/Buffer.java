package com.alticast.rop;
import java.util.*;
import java.io.*;

public class Buffer implements Reusable {

    private static final int RESERVED_SIZE = 8; // for adding prefix such as message length
    private static final int RESIZE_UNIT = 1024;

    private byte[] buffer = new byte[RESIZE_UNIT];
    private int begin = RESERVED_SIZE;
    private int end = RESERVED_SIZE;

    public Object appData;

    public void reuse () {
        clear();
        appData = null;
    }

    // unsafe methods (doesn't check capacity)

    protected int read () { return buffer[begin++] & 0xff; }
    protected void readBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buf[off++] = buffer[begin++];
        }
    }
    protected void write (int d) { buffer[end++] = (byte)d; }
    protected void writeBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buffer[end++] = buf[off++];
        }
    }
    protected void grow (int s) { end += s; }
    protected void drop (int s) { begin += s; }

    public int capacity () { return buffer.length; }
    public int size () { return end - begin; }
    public int margin () { return buffer.length - end; }
    public void clear () { begin = end = RESERVED_SIZE; }

    public void ensureMargin (int s) {
        if (margin() >= s) return;
        s = (s + end + (RESIZE_UNIT-1)) / RESIZE_UNIT * RESIZE_UNIT;
        byte[] nbuf = new byte[s];
        System.arraycopy(buffer, begin, nbuf, RESERVED_SIZE, end - begin);
        begin = RESERVED_SIZE;
        end = end - begin + RESERVED_SIZE;
        buffer = nbuf;
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

    public void preWriteI32 (int i) {
        buffer[--begin] = (byte)(i);
        buffer[--begin] = (byte)(i >> 8);
        buffer[--begin] = (byte)(i >> 16);
        buffer[--begin] = (byte)(i >> 24);
    }

    public int peek (int i) { return buffer[begin+i]; }
    public void writeTo (OutputStream os) throws IOException {
        Log.info("send "+size()+" bytes");
        String s = "";
        for (int i = begin; i < end; i++) {
            s += (buffer[i] & 0xff)+" ";
        }
        Log.info(s);
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

    public void dump () {
        StringBuffer str = new StringBuffer();
        for (int i = begin; i < end; i++) {
            str.append(buffer[i]);
            str.append(", ");
        }
        Log.info(str);
    }

}
