import java.util.*;
import java.io.*;

public class Buffer {
    private final int RESIZE_UNIT = 1024;

    byte[] buffer = new byte[RESIZE_UNIT];
    int begin;
    int end;

    public int capacity () { return buffer.length; }
    public int read () { return buffer[begin++] & 0xff; }
    public int peek (int i) { return buffer[begin+i]; }
    public void readBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buf[off++] = buffer[begin++];
        }
    }
    public void readTo (OutputStream os) throws IOException {
        System.out.println("send "+size()+" bytes");
        String s = "";
        for (int i = begin; i < end; i++) {
            s += buffer[i]+" ";
        }
        System.out.println(s);
        os.write(buffer, begin, size());
        reset();
    }
    public void write (int d) { buffer[end++] = (byte)d; }
    public void writeBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buffer[end++] = buf[off++];
        }
    }
    public void writeFrom (InputStream is) throws IOException {
        int r = is.read(buffer, end, margin());
        if (r == -1) {
            throw new IOException("EOF");
        }
        grow(r);
    }
    public int size () { return end - begin; }
    public int margin () { return buffer.length - end; }
    public void reset () { begin = end = 0; }
    void moveFront () {
        end -= begin;
        System.arraycopy(buffer, begin, buffer, 0, end);
        begin = 0;
    }
    public void grow (int s) { end += s; }
    public void drop (int s) { begin += s; }

    public void ensureMargin (int s) {
        s = (s + end + (RESIZE_UNIT-1)) / RESIZE_UNIT * RESIZE_UNIT;
        if (s <= buffer.length) return;
        byte[] nbuf = new byte[s];
        end -= begin;
        System.arraycopy(buffer, begin, nbuf, 0, end);
        begin = 0;
        buffer = nbuf;
    }

    public void dump () {
        StringBuffer str = new StringBuffer();
        for (int i = begin; i < end; i++) {
            str.append(buffer[i]);
            str.append(", ");
        }
        System.out.println(str);
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

    public void writeI8 (byte i) {
        write(i);
    }

    public void writeI16 (short i) {
        write(i >> 8);
        write(i);
    }

    public void writeI32 (int i) {
        write(i >> 24);
        write(i >> 16);
        write(i >> 8);
        write(i);
    }

    public void writeI64 (long i) {
        write((int)(i >> 56));
        write((int)(i >> 48));
        write((int)(i >> 40));
        write((int)(i >> 32));
        write((int)(i >> 24));
        write((int)(i >> 16));
        write((int)(i >> 8));
        write((int)(i));
    }
}
