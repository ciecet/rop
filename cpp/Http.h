#ifndef HTTP_MONITOR_H
#define HTTP_MONITOR_H

#include "EventDriver.h"
#include "Buffer.h"
#include "Log.h"

namespace base {

struct HttpReactor: FileReactor {

    typedef enum {
        BEGIN_READ,
        READ_HEADS,
        READ_BODY,
        READ_WEBSOCKET,
        READ_WEBSOCKET_PAYLOAD,
        READ_WEBSOCKET_KEY3, // read addtional head for hixie-76
        READ_WEBSOCKET_76
    } STATE;

    typedef enum {
        OPCODE_CONTINUATION = 0x00,
        OPCODE_TEXT = 0x01,
        OPCODE_BINARY = 0x02,
        OPCODE_CLOSE = 0x08,
        OPCODE_PING = 0x09,
        OPCODE_PONG = 0x0a
    } OPCODE;

    STATE state;
    Buffer inBuffer;
    Buffer outBuffer;
    // balance of request/response in keep-alive connection.
    // Not applicable to websocket.
    string requestLine;
    map<string,string> headers;
    Buffer contentBuffer; // received http body or websocket payload
    OPCODE opcode;
    int webSocketVersion; // 0:not websocket, +:version, -:hixie version

    // temporal data
    uint8_t key3[8];
    int64_t length; // content length or websocket payload length
    uint8_t frameMask[4];

    Log log;

    HttpReactor (): state(BEGIN_READ), webSocketVersion(0), log("http ") {}

    // utility method for easy frame creation.
    // shall be used only in writeFrame().
    void sendFrame (OPCODE op, uint8_t *msg, int64_t plen);

    // utility method which tells event driver to watch writing.
    void beginWrite () {
        changeMask(mask | EVENT_WRITE);
        write(); // try write.
    }

    // callbacks
    virtual void readRequest () {}
    virtual void writeResponse () {}
    virtual void readFrame () {}
    virtual void writeFrame () {}
    virtual void close (const char *msg) {
        ::close(fd);
        eventDriver->remove(this);
        log.info("closed %s\n", msg);
    }

    // don't touch.
    void read ();
    void write ();
};

}
#endif
