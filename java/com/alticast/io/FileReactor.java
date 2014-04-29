package com.alticast.io;

import java.io.*;
import com.alticast.rop.*;

/**
 * A Reactor which provides file related apis.
 * Its super class can act as file reactor as well, however it lacks
 * high level apis for file io.
 */
public abstract class FileReactor extends Reactor {

    public FileReactor () {
        super(true);
    }

    public FileReactor (boolean s) {
        super(s);
    }

    /** Read from file. called by event driver. */
    public void read () throws IOException {}

    /** Write to file. called by event driver. */
    public void write () throws IOException {}

    /** Close file if open. Must be called from event thread. */
    public void close () {
        int fd = getFd();
        if (fd == -1) return;

        getEventDriver().unwatchFile(this);

        NativeIo.close(fd);
        postClose();
    }

    public void postClose () { }

    public void setReadable (boolean r) {
        int fd = getFd();
        if (fd == -1) return;
        int fmask = getMask();
        if (r) {
            fmask |= EventDriver.EVENT_READ;
        } else {
            fmask &= ~EventDriver.EVENT_READ;
        }
        getEventDriver().watchFile(fd, fmask, this);
    }

    public void setWritable (boolean w) {
        int fd = getFd();
        if (fd == -1) return;
        int fmask = getMask();
        if (w) {
            fmask |= EventDriver.EVENT_WRITE;
        } else {
            fmask &= ~EventDriver.EVENT_WRITE;
        }
        getEventDriver().watchFile(fd, fmask, this);
    }

    public void processEvent (int events) {
        try {
            if ((events & EventDriver.EVENT_WRITE) != 0 &&
                    (getMask() & EventDriver.EVENT_FILE) != 0) {
                write();
            }
            if ((events & EventDriver.EVENT_READ) != 0 &&
                    (getMask() & EventDriver.EVENT_FILE) != 0) {
                read();
            }
        } catch (IOException e) {
            e.printStackTrace();
            if (W) Log.warn("Got exception:"+e);
            close();
        } catch (Throwable t) {
            t.printStackTrace();
            if (E) Log.error("Uncaught:"+t);
            close();
        }
    }
}
