#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <string>

namespace acba {

template <typename F, typename T>
inline static T bitwise_cast(F f) {
    union {
       F f;
       T t;
    } u;
    u.f = f;
    return u.t;
}

class Buffer {

    // for prepending bytes on payload.
    static const int32_t RESERVED_SIZE = 16; // should be double-aligned.

    static const int32_t ALLOC_UNIT = 512;

    enum {
        STACK_ALLOCATED,
        HEAP_ALLOCATED,
        EXTERNAL
    } type;

    uint8_t internalBuffer[ALLOC_UNIT];

    uint8_t *buffer, *_end;
    uint8_t *rpos, *wpos;

public:

    void *appData; // for passing registry

    // Construct dynamic buffer, initially empty.
    Buffer (): type(STACK_ALLOCATED),
            buffer(internalBuffer), _end(buffer + ALLOC_UNIT), 
            rpos(buffer + RESERVED_SIZE), wpos(buffer + RESERVED_SIZE) {}

    // Construct buffer from pre-defined byte array.
    Buffer (uint8_t *buf, int32_t len): type(EXTERNAL),
            buffer(buf), _end(buf + len),
            rpos(buf), wpos(buf+len) {}

    ~Buffer () {
        if (type == HEAP_ALLOCATED) {
            free(buffer);
        }
    }

    inline int32_t capacity () { return _end - buffer; }
    inline int32_t size () { return wpos - rpos; }
    inline int32_t margin () { return _end - wpos; }

    inline uint8_t read () { return *rpos++; }
    inline void readBytes (uint8_t *buf, int32_t len) {
        while (len-- > 0) {
            *buf++ = *rpos++;
        }
    }
    inline uint8_t peek (int32_t i) { return rpos[i]; }

    inline void write (uint8_t d) {
        *wpos++ = d;
    }
    inline void writeBytes (const uint8_t *buf, int32_t len) {
        ensureMargin(len);
        while (len-- > 0) {
            write(*buf++);
        }
    }
    inline void clear () { rpos = wpos = buffer + RESERVED_SIZE; }
    inline void grow (int32_t s) { wpos += s; }
    inline void drop (int32_t s) { rpos += s; }
    inline uint8_t *begin () { return rpos; }
    inline uint8_t *end () { return wpos; }

    inline void ensureMargin (int32_t s) {
        if (type == EXTERNAL) return;
        if (margin() >= s) return;

        int32_t roff = rpos - buffer;
        int32_t woff = wpos - buffer;
        s = ((woff + s) * 3 / 2 + (ALLOC_UNIT - 1)) / ALLOC_UNIT *
                ALLOC_UNIT;

        if (buffer == internalBuffer) {
            buffer = reinterpret_cast<uint8_t*>(malloc(s));
            memcpy(buffer + roff, rpos, size());
            type = HEAP_ALLOCATED;
        } else {
            buffer = reinterpret_cast<uint8_t*>(realloc(buffer, s));
        }
        _end = buffer + s;
        rpos = buffer + roff;
        wpos = buffer + woff;
    }

    // move payload to front if it is within later half of capacity.
    // heuristic buffer compaction.
    inline void moveToFront () {

        if (size() + margin() >= capacity() / 2) return;

        uint8_t *front = buffer + RESERVED_SIZE;

        // if nowhere to move
        if (rpos <= front) return;

        // check length and move
        int l = wpos - rpos;
        if (l > 0) {
            memmove(front, rpos, l);
        }
        rpos = front;
        wpos = front + l;
    }

    int8_t readI8 () {
        return *rpos++;
    }

    int16_t readI16 () {
        int16_t i = (int16_t(rpos[0]) << 8) | rpos[1];
        rpos += 2;
        return i;
    }

    int32_t readI32 () {
        int32_t i = (int32_t(rpos[0]) << 24) | (int32_t(rpos[1]) << 16) |
                (int32_t(rpos[2]) << 8) | rpos[3];
        rpos += 4;
        return i;
    }

    int64_t readI64 () {
        uint32_t i = (uint32_t(rpos[0]) << 24) | (uint32_t(rpos[1]) << 16) |
                (uint32_t(rpos[2]) << 8) | rpos[3];
        uint32_t j = (uint32_t(rpos[4]) << 24) | (uint32_t(rpos[5]) << 16) |
                (uint32_t(rpos[6]) << 8) | rpos[7];
        rpos += 8;
        return (int64_t(i) << 32) + j;
    }

    float readF32 () {
        return bitwise_cast<int32_t, float>(readI32());
    }

    double readF64 () {
        return bitwise_cast<int64_t, double>(readI64());
    }

    void writeI8 (int8_t i)  {
        ensureMargin(1);
        write(i);
    }

    void writeI16 (int16_t i) {
        ensureMargin(2);
        write(i >> 8);
        write(i);
    }

    void writeI32 (int32_t i) {
        ensureMargin(4);
        write(i >> 24);
        write(i >> 16);
        write(i >> 8);
        write(i);
    }

    void writeI64 (int64_t i) {
        writeI32(i >> 32);
        writeI32(i);
    }

    void writeF32 (float f) {
        writeI32(bitwise_cast<float, int32_t>(f));
    }

    void writeF64 (double f) {
        writeI64(bitwise_cast<double, int64_t>(f));
    }

    void write (const std::string &str) {
        writeBytes(reinterpret_cast<const uint8_t*>(str.data()), str.size());
    }

    void preWriteI8 (int8_t i) {
        rpos--;
        *rpos = i;
    }

    void preWriteI32 (int32_t i) {
        rpos -= 4;
        rpos[0] = i >> 24;
        rpos[1] = i >> 16;
        rpos[2] = i >> 8;
        rpos[3] = i;
    }

    void dump () {
        for (uint8_t *s = rpos; s < wpos; s++) {
            uint8_t c = *s;
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9')) {
                printf("'%c ", c);
            } else {
                printf("%02x ", c);
            }
        }
        printf("\n");
    }
};

}
#endif
