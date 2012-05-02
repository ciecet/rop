#include <sys/select.h>
#include <unistd.h>
#include <sys/ui.h>
#include "Port.h"
#include "UnixTransport.h"

static __thread int rpcThreadId;

static int nextId = 1;

PortRef UnixTransport::getPort ()
{
    if (!rpcThreadId) {
        rpcThreadId = nextId++;
    }
    return getPort(rpcThreadId);
}

static pthraed_t loopThread;

void UnixTransport::loop ()
{
    fd_set infdset;
    fd_set expfdset;
    int nfds = infdset+1;

    loopThread = pthread_self();

    while (true) {
        FD_ZERO(&infdset);
        FD_ZERO(&expfdset);
        FD_SET(inFd, &infdset);
        FD_SET(inFd, &expfdset);
        if (pselect(nfds, infdset, outfdset, expfdset, 0, 0) <= 0) {
            return;
        }

        if (FD_ISSET(inFd, &expfdset)) {
            return;
        }

        if (FD_ISSET(inFd, &infdset)) {
            tryReceive();
        }

        tryHandleProcesses();
    }
}

static void UnixTransport::tryReceive ()
{
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
            return;
        } if (r < 0) {
            // fail (or eblock)
            return;
        }
        inBuffer.size += r;

        // on start of message frame...
        if (inPort == 0) {
            // need port id and payload length
            if (inBuffer.size < 4) {
                return;
            }
            int p = inBuffer.read();
            p = (p << 8) + inBuffer.read();
            p = (p << 8) + inBuffer.read();
            p = (p << 8) + inBuffer.read();
            inPort = getPort(p);
        }

        if (inPort.read(&inBuffer) == COMPLETE) {
            inPort = 0;
        }
    }
}

void UnixTransport::send (PortRef p)
{
    pthread_mutex_lock(&lock);
    while (outPort) {
        pthread_condvar_wait(&writableCondition, &lock);
    }
    outPort = p;
    pthread_mutex_unlock(&lock);

    while (true) {

        // fill in buffer
        if (p->outStack.frame) {
            RESULT r;
            do {
                r = outPort.write(&outBuffer);
            } while (r != STOPPED);
        } else {
            if (outBuffer.size == 0) {
                break;
            }
        }

        int w;
        if (outBuffer.hasWrappedData()) {
            iovec io[2];
            io[0].iov_base = outBuffer.begin();
            io[0].iov_len = BUFFER_SIZE - outBuffer.offset;
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

    pthread_mutex_lock(&lock);
    outPort = 0;
    pthread_cond_signal(&writableCondition);
    pthread_mutex_unlock(&lock);
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

    if (pselect(nfds, infdset, outfdset, expfdset, 0, 0) <= 0) {
        return;
    }

    if (FD_ISSET(inFd, &expfdset) || FD_ISSET(outFd, &expfdset)) {
        return;
    }

    if (FD_ISSET(inFd, &infdset)) {
        tryReceive();
    }
}

void UnixTransport::sendAndWait (PortRef p)
{
    send(p);

    pthread_mutex_lock(&lock);
    while (!p->returns.empty()) {
        pthread_cond_wait(&p->responseCondition);
    }
    pthread_mutex_unlock(&lock);
}

void UnixTransport::notify (PortRef p)
{
    pthread_mutex_lock(&lock);
    pthread_cond_signal(&p->responseCondition);
    pthread_mutex_unlock(&lock);
}
