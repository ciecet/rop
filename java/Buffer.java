import java.util.*;
import java.io.*;

public class Buffer implements Reusable {
    private final int HEAD_SIZE = 8;
    private final int RESIZE_UNIT = 1024;

    byte[] buffer = new byte[RESIZE_UNIT];
    int begin = HEAD_SIZE;
    int end = HEAD_SIZE;

    public Object appData;

    public void reset () {
        clear();
        appData = null;
    }

    public int capacity () { return buffer.length; }
    public int read () { return buffer[begin++] & 0xff; }
    public int peek (int i) { return buffer[begin+i]; }
    public void readBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buf[off++] = buffer[begin++];
        }
    }
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
    public void write (int d) { buffer[end++] = (byte)d; }
    public void writeBytes (byte[] buf, int off, int len) {
        while (len-- > 0) {
            buffer[end++] = buf[off++];
        }
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
    public int size () { return end - begin; }
    public int margin () { return buffer.length - end; }
    public void clear () { begin = end = HEAD_SIZE; }
    public void grow (int s) { end += s; }
    public void drop (int s) { begin += s; }

    public void ensureMargin (int s) {
        s = (s + end + (RESIZE_UNIT-1)) / RESIZE_UNIT * RESIZE_UNIT;
        if (s <= buffer.length) return;
        byte[] nbuf = new byte[s];
        System.arraycopy(buffer, begin, nbuf, HEAD_SIZE, end - begin);
        begin = HEAD_SIZE;
        end = end - begin + HEAD_SIZE;
        buffer = nbuf;
    }

    public void dump () {
        StringBuffer str = new StringBuffer();
        for (int i = begin; i < end; i++) {
            str.append(buffer[i]);
            str.append(", ");
        }
        Log.info(str);
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
        write(i);
    }

    public void writeI16 (int i) {
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

    public void preWriteI32 (int i) {
        buffer[--begin] = (byte)(i);
        buffer[--begin] = (byte)(i >> 8);
        buffer[--begin] = (byte)(i >> 16);
        buffer[--begin] = (byte)(i >> 24);
    }
}
