import java.io.*;

public class Pipe {
    private byte[] buffer;
    private int cursor;

    public final InputStream inputStream = new InputStream() {
        public int read () {
            synchronized (Pipe.this) {
                while (buffer == null) {
                    try {
                        Pipe.this.wait();
                    } catch (InterruptedException e) {}
                }
                byte d = buffer[cursor++];
                if (cursor == buffer.length) {
                    buffer = null;
                    Pipe.this.notify();
                }
                return (int)d;
            }
        }
        public int read(byte b[], int off, int len) throws IOException {
            if (len == 0) return 0;
            synchronized (Pipe.this) {
                while (buffer == null) {
                    try {
                        Pipe.this.wait();
                    } catch (InterruptedException e) {}
                }
                int rlen = buffer.length - cursor;
                if (rlen > len) {
                    rlen = len;
                }
                System.arraycopy(buffer, cursor, b, off, rlen);
                cursor += rlen;
                if (cursor == buffer.length) {
                    buffer = null;
                    Pipe.this.notify();
                }
                return rlen;
            }
        }
    };

    public final OutputStream outputStream = new OutputStream() {
        public void write (int d) {
            byte[] buf = new byte[1];
            buf[0] = (byte)d;
            try {
                write(buf, 0, 1);
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
        public void write(byte b[], int off, int len) throws IOException {
            if (len == 0) return;
            synchronized (Pipe.this) {
                while (buffer != null) {
                    try {
                        Pipe.this.wait();
                    } catch (InterruptedException e) {}
                }
                buffer = new byte[len];
                cursor = 0;
                System.arraycopy(b, off, buffer, 0, len);
                Pipe.this.notify();
            }
        }
    };
}
