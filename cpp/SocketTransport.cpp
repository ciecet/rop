#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include "Remote.h"
#include "SocketTransport.h"
#include "Log.h"

using namespace std;
using namespace base;
using namespace rop;

void SocketTransport::loop ()
{
    Log l("loop ");
    fd_set infdset;
    fd_set expfdset;
    int nfds = inFd+1;

    loopThread = pthread_self();
    isLooping = true;

    while (true) {
        FD_ZERO(&infdset);
        FD_ZERO(&expfdset);
        FD_SET(inFd, &infdset);
        FD_SET(inFd, &expfdset);
        l.info("blocked for reading.\n");
        if (pselect(nfds, &infdset, 0, &expfdset, 0, 0) <= 0) {
            return;
        }
        if (FD_ISSET(inFd, &expfdset)) {
            return;
        }

        pthread_mutex_lock(&monitor);
        if (FD_ISSET(inFd, &infdset)) {
            l.info("got message.\n");
            unsafeRead();
        }

        // execute processors
        for (map<int,Port*>::iterator i = ports.begin();
                i != ports.end(); i++) {
            if (!i->second->requests.empty()) {
                l.info("executing request...\n");
            }
            while (i->second->processRequest());
        }
        pthread_mutex_unlock(&monitor);
    }
}

void SocketTransport::unsafeRead ()
{
    Log l("read ");

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
            l.debug("getting port:%d\n", -p);
        }

        switch (inPort->read(&inBuffer)) {
        case Frame::COMPLETE:
            inPort = 0;
            break;
        case Frame::STOPPED:
            break;
        default:
        case Frame::ABORTED:
            // TODO: handle it
            l.error("Unexpected IO exception.\n");
        }
    }
}

void SocketTransport::waitWritable ()
{
    fd_set infdset;
    fd_set outfdset;
    fd_set expfdset;
    bool isLoopThread = false;
    int nfds;

    pthread_t self = pthread_self();
    if (pthread_equal(self, loopThread)) {
        isLoopThread = true;
    }

    if (isLoopThread) {
        nfds = ((outFd > inFd) ? outFd : inFd ) + 1;
    } else {
        nfds = outFd + 1;
    }

    FD_ZERO(&infdset);
    FD_ZERO(&outfdset);
    FD_ZERO(&expfdset);
    FD_SET(outFd, &outfdset);
    FD_SET(outFd, &expfdset);
    if (isLoopThread) {
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
        pthread_mutex_lock(&monitor);
        unsafeRead();
        pthread_mutex_unlock(&monitor);
    }
}

void SocketTransport::flushPort (Port *p)
{
    Log l("flush ");
    pthread_mutex_lock(&monitor);
    while (isSending) {
        l.trace("waiting for lock...\n");
        pthread_cond_wait(&writableCondition, &monitor);
    }
    isSending = true;
    pthread_mutex_unlock(&monitor);

    l.debug("sending port:%d...\n", p->id);
    outBuffer.write(p->id >> 24);
    outBuffer.write(p->id >> 16);
    outBuffer.write(p->id >> 8);
    outBuffer.write(p->id);

    while (true) {

        // fill in buffer
        if (p->writer.frame) {
            l.info("... write to buffer\n");
            if (p->write(&outBuffer) == Frame::ABORTED) {
                // TODO: handle abortion.
                l.error("ABORTED..?\n");
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
            l.debug("sending message (wrapped)...\n");
            w = writev(outFd, io, 2);
        } else {
            l.debug("sending message...\n");
            w = write(outFd, outBuffer.begin(), outBuffer.size);
        }
        if (w >= 0) {
            l.debug("sent %d bytes.\n", w);
            outBuffer.drop(w);
            continue;
        }

        if (errno != EWOULDBLOCK) {
            break;
        }

        l.debug("buffer full...\n");
        waitWritable();
    }

    pthread_mutex_lock(&monitor);
    isSending = false;
    pthread_cond_signal(&writableCondition);
    pthread_mutex_unlock(&monitor);
}

void SocketTransport::waitReadable ()
{
    fd_set infdset;
    fd_set expfdset;

    FD_ZERO(&infdset);
    FD_ZERO(&expfdset);
    FD_SET(inFd, &infdset);
    FD_SET(inFd, &expfdset);

    if (pselect(inFd+1, &infdset, 0, &expfdset, 0, 0) <= 0) {
        return;
    }
}

void SocketTransport::waitPort (Port *p)
{
    Log l("waitport ");
    if (isLooping) {
        pthread_t self = pthread_self();
        if (!pthread_equal(self, loopThread)) {
            l.debug("waiting on condvar for port:%d\n", p->id);
            pthread_cond_wait(&p->wakeCondition, &monitor);
            return;
        }
    }

    pthread_mutex_unlock(&monitor);
    waitReadable();
    pthread_mutex_lock(&monitor);
    unsafeRead();
}
