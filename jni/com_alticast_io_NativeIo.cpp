#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>

#include <map>
#include <vector>

#include "Log.h"
#include "com_alticast_io_NativeIo.h"
#define ERROR_UNKNOWN com_alticast_io_NativeIo_ERROR_UNKNOWN
#define ERROR_WOULDBLOCK com_alticast_io_NativeIo_ERROR_WOULDBLOCK
#define ERROR_BIND com_alticast_io_NativeIo_ERROR_BIND

using namespace std;
using namespace acba;

static Log log("nativeio ");

JNIEXPORT jint JNICALL Java_com_alticast_io_NativeIo_createServerSocket (
        JNIEnv* env, jclass clz, jint port)
{
    int fd = -1;
    sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return ERROR_UNKNOWN;

    {
        int opt_val = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    }

    if (bind(fd, reinterpret_cast<sockaddr*>(&server_addr),
            sizeof(server_addr))) {
        close(fd);
        return ERROR_BIND;
    }

    listen(fd, 1);
    return fd;
}

JNIEXPORT jint JNICALL Java_com_alticast_io_NativeIo_createSocket (JNIEnv* env,
        jclass clz, jstring hAddr, jint port)
{
    int ret = ERROR_UNKNOWN;
    int sfd = ERROR_UNKNOWN;

    // construct params for addr resolving
    struct addrinfo hints;
    bzero(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    // get addr
    struct addrinfo *srv = NULL;
    const char *node = env->GetStringUTFChars(hAddr, NULL);
    if (!node) goto exit;
    char service[10];
    sprintf(service, "%d", port);
    if (getaddrinfo(node, service, &hints, &srv)) goto exit;

    // connect
    sfd = socket(srv->ai_family, srv->ai_socktype, srv->ai_protocol);
    if (sfd == -1) goto exit;
    if (connect(sfd, srv->ai_addr, srv->ai_addrlen)) goto exit;

    // success
    ret = sfd;

exit:
    if (ret == -1 && sfd != -1) close(sfd);
    if (srv) freeaddrinfo(srv);
    if (node) env->ReleaseStringUTFChars(hAddr, node);
    return ret;
}

JNIEXPORT jint JNICALL Java_com_alticast_io_NativeIo_accept (JNIEnv* env,
    jclass clz, jint listenFd)
{
    int fd = -1;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    fd = accept(listenFd, reinterpret_cast<struct sockaddr*>(&client_addr),
            &client_len);
    if (fd < 0) {
        return ERROR_UNKNOWN;
    }
    return fd;
}

JNIEXPORT void JNICALL Java_com_alticast_io_NativeIo_close (JNIEnv* env,
    jclass clz, jint fd)
{
    log.debug("close fd:%d\n", fd);
    close(fd);
}

static char *get_buffer (int s)
{
    static char *buf = NULL;
    static int size = 0;

    if (s > size) {
        size = s;
        buf = (char*)realloc(buf, size);
    }

    return buf;
}

JNIEXPORT jint JNICALL Java_com_alticast_io_NativeIo_tryRead (JNIEnv* env,
        jclass clz, jint fd, jbyteArray hBuf, jint off, jint size)
{
    char* buf = get_buffer(size);
    if (!buf) return ERROR_UNKNOWN;

    int r = recv(fd, buf, size, MSG_DONTWAIT);
    if (r == 0) return 0;
    if (r < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERROR_WOULDBLOCK;
        } else {
            log.warn("%s\n", strerror(errno));
            return ERROR_UNKNOWN;
        }
    }

    env->SetByteArrayRegion(hBuf, off, r, buf);
    return r;
}

JNIEXPORT jint JNICALL Java_com_alticast_io_NativeIo_tryWrite (JNIEnv* env,
        jclass clz, jint fd, jbyteArray hBuf, jint off, jint size)
{
    char* buf = get_buffer(size);
    if (!buf) return ERROR_UNKNOWN;

    env->GetByteArrayRegion(hBuf, off, size, buf);
    if (env->ExceptionOccurred()) return ERROR_UNKNOWN;

    int w = send(fd, buf, size, (MSG_DONTWAIT | MSG_NOSIGNAL));
    if (w < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERROR_WOULDBLOCK;
        } else {
            return ERROR_UNKNOWN;
        }
    }
    return w;
}

JNIEXPORT jlong JNICALL Java_com_alticast_io_NativeIo_getMonotonicTime (
        JNIEnv* env, jclass clz)
{
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}
