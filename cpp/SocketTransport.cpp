#include <sys/select.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "rop.h"
#include "SocketTransport.h"
#include "Log.h"

using namespace std;
using namespace acba;
using namespace rop;

int SocketServer::start(int port, acba::EventDriver* ed)
{
    struct sockaddr_in servAddr;
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(port);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    {
        int optval = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                sizeof(optval));
    }

    if (::bind(fd, (struct sockaddr*)&servAddr, sizeof(servAddr))) {
        log.error("bind failed ret:%d\n", errno);
        ::close(fd);
        return -1;
    }
    ::listen(fd, 1);
    ed->watchFile(fd, ::EVENT_READ, this);
    log.info("listening on port %d (fd:%d)\n", port, fd);
    return 0;
}

void SocketServer::stop()
{
    abort("socket server stopped by request");
}

void SocketServer::read ()
{
    int fd;
    {
        struct sockaddr_in clientAddr;
        socklen_t len = sizeof(clientAddr);
        fd = ::accept(getFd(), (struct sockaddr*)&clientAddr, &len);
    }
    if (!fd) {
        log.warn("Accept failed.\n");
        return;
    }
    log.info("new connection fd:%d\n", fd);

    SocketTransport* t = new SocketTransport(isRopMultiThreaded);
    if (exportables.size() > 0) {
        t->registry->setExportables(exportables);
    }
    getEventDriver()->watchFile(fd, ::EVENT_READ, t);
}

void SocketTransport::read ()
{
    while (true) {
        if (inBuffer.margin() == 0) {
            inBuffer.ensureMargin(inBuffer.capacity() / 2);
        }
        int r = ::recv(getFd(), inBuffer.end(), inBuffer.margin(),
                MSG_DONTWAIT);
        if (r <= 0) {
            if (r == 0) {
                abort("got EOF\n");
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                abort("read error\n");
            }
            return;
        }
        inBuffer.grow(r);

        // check if inBuffer contains a request at least
        if (inBuffer.size() >= 4) {
            int32_t msize = inBuffer.readI32();
            bool hasMessage = (inBuffer.size() >= msize);
            inBuffer.preWriteI32(msize);
            if (hasMessage) break;
        }
    }

    ScopedLock l(registry->monitor);

    while (inBuffer.size() > 4) {
        int32_t msize = inBuffer.readI32();
        if (inBuffer.size() < msize) {
            inBuffer.preWriteI32(msize);
            break;
        }

        Buffer rbuf(inBuffer.begin(), msize);
        rbuf.appData = registry;
        inBuffer.drop(msize);
        int32_t pid = -rbuf.readI32();
        Port *p = registry->getPort(pid);
        log.debug("recv %d bytes from port:%d\n", rbuf.size(), pid);
        if (registry->recv(p, rbuf) == -1) {
            abort("invalid message\n");
            return;
        }
    }

    inBuffer.moveToFront();
}

// unsafe
// returns -1 on error.
int SocketTransport::tryWrite (Buffer &buf)
{
    int fd = getFd();
    if (fd < 0) {
        log.debug("fd already closed.\n");
        return -1;
    }

    while (buf.size()) {
        int w = ::send(getFd(), buf.begin(), buf.size(), (
                MSG_DONTWAIT | MSG_NOSIGNAL));
        log.debug("write(%d, %dbytes) returned %d.\n", getFd(), buf.size(), w);
        if (w < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1;
        }
        buf.drop(w);
    }
    return 0;
}

void SocketTransport::write ()
{
    ScopedLock l(registry->monitor);
    if (tryWrite(outBuffer)) {
        abort("write fail\n");
        return;
    }

    if (!outBuffer.size()) {
        outBuffer.clear();
        setWritable(false);
        return;
    }

    outBuffer.moveToFront();
}

int SocketTransport::send (Port *p, Buffer &buf)
{
    // wait until send buffer get flushed.
    if (outBufferLimit && outBuffer.size() >= outBufferLimit) {
        log.info("flushing out buffer...\n");

        EventDriver *ed = getEventDriver();
        if (ed->isEventThread()) {
            while (registry && outBuffer.size() > 0) { // = outBufferLimit) {
                ScopedUnlock ul(registry->monitor);
                ed->wait();
            }
        } else {
            outBufferMonitor.waitUntilFlushed();
        }
    }

    log.debug("send %d bytes to port:%d\n", buf.size(), p->id);

    buf.preWriteI32(p->id);
    buf.preWriteI32(buf.size());

    // previous message is being sent.
    if (outBuffer.size()) {
        outBuffer.writeBytes(buf.begin(), buf.size());
        buf.clear();
        return 0;
    }

    // try write directly to avoid context-switch.
    // and then return success if buf became empty.
    if (tryWrite(buf)) {
        return -1;
    }
    if (!buf.size()) {
        return 0;
    }

    // schedule event driver to perform send.
    outBuffer.writeBytes(buf.begin(), buf.size());
    buf.clear();
    setWritable(true);
    return 0;
}

int SocketTransport::wait (Port *p)
{
    if (!registry) return -1;
    if (getEventDriver()->isEventThread()) {
        ScopedUnlock ul(registry->monitor);
        getEventDriver()->wait();
    } else {
        registry->wait(p->updated);
    }
    return 0;
}

void SocketTransport::requestRun ()
{
    if (getEventDriver()->isEventThread()) {
        getEventDriver()->setTimeout(0, &requestReactor);
    }
}

void SocketTransport::close ()
{
    acba::Ref<Registry> reg = registry; // copy ref to avoid early destroy
    if (reg) {
        ScopedLock l(reg->monitor);
        log.trace("closing as transport\n");
        Transport::close();
        log.trace("aborting as reactor\n");
        FileReactor::abort(); // this calls close() again in ed thread.
    } else {
        log.trace("closing as reactor\n");
        FileReactor::close();
    }
}
