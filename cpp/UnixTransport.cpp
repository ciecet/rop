#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
//#include <sys/ui.h>
#include "Remote.h"
#include "UnixTransport.h"

using namespace rop;

void UnixTransport::loop ()
{
    fd_set infdset;
    fd_set expfdset;
    int nfds = inFd+1;

    loopThread = pthread_self();

    while (true) {
        FD_ZERO(&infdset);
        FD_ZERO(&expfdset);
        FD_SET(inFd, &infdset);
        FD_SET(inFd, &expfdset);
        if (pselect(nfds, &infdset, 0, &expfdset, 0, 0) <= 0) {
            return;
        }
        if (FD_ISSET(inFd, &expfdset)) {
            return;
        }

        if (FD_ISSET(inFd, &infdset)) {
            tryReceive();
        }

        // execute processors
        for (map<int,Port*>::iterator i = ports.begin();
                i != ports.end(); i++) {
            while (i->second->handleRequest());
        }
    }
}

void UnixTransport::tryReceive ()
{
    pthread_mutex_lock(&monitor);

    while (true) {

        // fill in buffer
        int r;
        if (inBuffer.hasWrappedMargin()) {
            iovec io[2];
            io[0].iov_base = inBuffer.end();
            io[0].iov_len = inBuffer.margin() - inBuffer.offset;
            io[1].iov_base = inBuffer.buffer;
            io[1].iov_len = inBuffer.offset;
            r = readv(inFd, io, 2);
        } else {
            r = read(inFd, inBuffer.end(), inBuffer.margin());
        }
        if (r == 0) {
            // eof
            break;
        } if (r < 0) {
            // fail (or eblock)
            break;
        }
        inBuffer.size += r;

        // on start of message frame...
        if (inPort == 0) {
            // need port id and payload length
            if (inBuffer.size < 4) {
                break;
            }
            int p = inBuffer.read();
            p = (p << 8) + inBuffer.read();
            p = (p << 8) + inBuffer.read();
            p = (p << 8) + inBuffer.read();
            inPort = getPort(-p); // reverse local<->remote port id
        }

        switch (inPort->read(&inBuffer)) {
        case Frame::COMPLETE:
            inPort = 0;
            break;
        case Frame::ABORTED:
            // TODO: handle it
            ;
        }
    }
    pthread_mutex_unlock(&monitor);
}

void UnixTransport::waitWritable ()
{
    fd_set infdset;
    fd_set outfdset;
    fd_set expfdset;
    bool handleRead = false;
    int nfds;

    pthread_t self = pthread_self();
    if (pthread_equal(self, loopThread)) {
        handleRead = true;
    }

    if (handleRead) {
        nfds = ((outFd > inFd) ? outFd : inFd ) + 1;
    } else {
        nfds = outFd + 1;
    }

    FD_ZERO(&infdset);
    FD_ZERO(&outfdset);
    FD_ZERO(&expfdset);
    FD_SET(outFd, &outfdset);
    FD_SET(outFd, &expfdset);
    if (handleRead) {
        FD_SET(inFd, &infdset);
        FD_SET(inFd, &expfdset);
    }

    if (pselect(nfds, &infdset, &outfdset, &expfdset, 0, 0) <= 0) {
        return;
    }

    if (FD_ISSET(inFd, &expfdset) || FD_ISSET(outFd, &expfdset)) {
        return;
    }

    if (FD_ISSET(inFd, &infdset)) {
        tryReceive();
    }
}

void UnixTransport::flushPort (Port *p)
{
    pthread_mutex_lock(&monitor);
    while (isSending) {
        pthread_cond_wait(&writableCondition, &monitor);
    }
    isSending = true;
    pthread_mutex_unlock(&monitor);

    outBuffer.write(p->id >> 24);
    outBuffer.write(p->id >> 16);
    outBuffer.write(p->id >> 8);
    outBuffer.write(p->id);

    while (true) {

        // fill in buffer
        if (p->writer.frame) {
            Frame::STATE r;
            do {
                r = p->write(&outBuffer);
            } while (!(r == Frame::STOPPED || r == Frame::ABORTED));
            if (r == Frame::ABORTED) {
                // TODO: handle abortion.
            }
        } else {
            if (outBuffer.size == 0) {
                // all flushed.
                break;
            }
        }

        int w;
        if (outBuffer.hasWrappedData()) {
            iovec io[2];
            io[0].iov_base = outBuffer.begin();
            io[0].iov_len = Buffer::BUFFER_SIZE - outBuffer.offset;
            io[1].iov_base = outBuffer.buffer;
            io[1].iov_len = outBuffer.end() - outBuffer.buffer;
            w = writev(outFd, io, 2);
        } else {
            w = write(outFd, outBuffer.begin(), outBuffer.size);
        }
        if (w >= 0) {
            outBuffer.drop(w);
            continue;
        }

        if (errno != EWOULDBLOCK) {
            break;
        }

        waitWritable();
    }

    pthread_mutex_lock(&monitor);
    isSending = false;
    pthread_cond_signal(&writableCondition);
    pthread_mutex_unlock(&monitor);
}
