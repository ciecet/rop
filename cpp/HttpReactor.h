#ifndef HTTP_MONITOR_H
#define HTTP_MONITOR_H

#include "EventDriver.h"
#include "Buffer.h"
#include "Log.h"
#include <map>
#include <string>

#ifdef ED_MANGLE
#define HttpReactor ED_MANGLE(HttpReactor)
#endif

namespace acba {

struct HttpReactor: FileReactor {

    Buffer inBuffer;
    Buffer outBuffer;

    std::string requestLine;
    std::string requestMethod; // GET, POST, ...
    std::string requestPath; // /xxx/index.html
    std::string requestProtocol; // HTTP/1.1
    std::map<std::string,std::string> headers;
    Buffer contentBuffer; // received http body or websocket payload
    int32_t contentLength;

    // 0:not websocket, +:version, -:hixie version
    int websocketVersion;

    HttpReactor (): FileReactor(true), websocketVersion(0),
            state(BEGIN_READ), log("http ") {
        pthread_mutex_init(&mutex, 0);
    }

    ~HttpReactor () {
        pthread_mutex_destroy(&mutex);
    }

    // callbacks
    virtual void readFrame () { abort("Not Supported"); }
    virtual void readRequest () { abort("Not Supported"); }

    // mt-safe
    void send (const uint8_t *buf, int32_t len);
    void send (const std::string &msg);
    void sendFrame (const uint8_t *msg, int32_t len, bool isText = false);
    void sendFrameAsText (const uint8_t *msg, int32_t len);
    void sendResponse (int32_t status, const std::string &mimetype,
            const std::string &content);

    // FileReactor implementations
    void read ();
    void write ();

protected:

    enum STATE {
        BEGIN_READ,
        READ_HEADS,
        TRY_HANDSHAKE,
        READ_BODY,
        READ_WEBSOCKET,
        READ_WEBSOCKET_PAYLOAD,
        READ_WEBSOCKET_76,
        READ_WEBSOCKET_PAYLOAD_76
    } state;

    enum OPCODE {
        OPCODE_CONTINUATION = 0x00,
        OPCODE_TEXT = 0x01,
        OPCODE_BINARY = 0x02,
        OPCODE_CLOSE = 0x08,
        OPCODE_PING = 0x09,
        OPCODE_PONG = 0x0a
    } opcode;

    Log log;

private:

    uint8_t frameMask[4]; // temporal data
    pthread_mutex_t mutex;

    bool parseHeader ();
    bool handshake ();
    bool readWebSocketHeader ();
    bool tryWrite ();
};

}
#endif
