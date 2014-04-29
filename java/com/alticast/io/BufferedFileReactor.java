package com.alticast.io;

import java.io.*;
import java.util.*;
import com.alticast.rop.*;

public class BufferedFileReactor extends FileReactor {

    protected Buffer inBuffer = new Buffer();
    protected Buffer outBuffer = new Buffer();
    protected boolean writingBuffer = false;

    public BufferedFileReactor () {
        super(true);
    }

    public BufferedFileReactor (boolean s) {
        super(s);
    }

    /** Read from inBuffer. Need override. */
    protected void readBuffer () throws IOException {
        throw new IOException("Not implemented.");
    }

    /** Send outBuffer. Called by subclass. */
    protected void sendBuffer () throws IOException {
        if (writingBuffer) return;

        tryWrite();

        // return if outBuffer was emptied.
        if (outBuffer.size() == 0) return;

        // otherwise, schedule writing for remaining.
        writingBuffer = true;
        setWritable(true);
    }

    public synchronized void send (Object data) throws IOException {
        if (data instanceof Buffer) {
            outBuffer.writeBuffer((Buffer)data);
        } else if (data instanceof byte[]) {
            outBuffer.writeBytes((byte[])data);
        } else {
            outBuffer.writeRawString(data.toString());
        }

        sendBuffer();
    }

    public final void read () throws IOException {
        int fd = getFd();
        int r;
        while (true) {
            r = inBuffer.readFrom(fd);
            if (r == 0) {
                if (I) Log.info("got EOF from fd:"+fd);
                break;
            }
            if (r < 0) {
                if (r == NativeIo.ERROR_WOULDBLOCK) break;
                throw new IOException("Read Error");
            }
            if (D) Log.debug("read "+r+" bytes from fd:"+fd);
        }

        readBuffer();

        // close on EOF
        if (r == 0) {
            close();
        }
    }

    public synchronized void write () throws IOException {
        tryWrite();
        if (outBuffer.size() == 0) {
            setWritable(false);
            writingBuffer = false;
        }
    }

    private void tryWrite () throws IOException {
        while (outBuffer.size() > 0) {
            int w = outBuffer.writeTo(getFd());
            if (w < 0) {
                if (w == NativeIo.ERROR_WOULDBLOCK) return;
                throw new IOException("Write Error");
            }
        }
    }

    private boolean closing = false;
    private String closingMessage = null; // used for close request

    public void close (Object msg) {
        if (msg != null) {
            closingMessage = msg.toString();
        }
        close();
    }

    /** Ensure that FileReactor.close() to be called from event thread */
    public void close () {
        EventDriver ed = getEventDriver();
        if (ed == null) return; // if already closed.
        if (!ed.isEventThread()) {
            closing = true;
            ed.setTimeout(0, this);
            return;
        }

        if (I) Log.info("closing file reactor... " + (
                (closingMessage != null) ? closingMessage : ""));
        super.close();
    }

    public void processEvent (int events) {
        if (closing) {
            super.close();
            return;
        }
        super.processEvent(events);
    }
}
