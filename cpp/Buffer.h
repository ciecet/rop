#ifndef BUFFER_H
#define BUFFER_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

namespace base {

class Buffer {

    static const int RESIZE_UNIT = 1024;

    uint8_t *buffer;
    int _capacity;
    uint8_t *_begin, *_end;
    bool allocated;

public:

    // Construct buffer from pre-defined byte array.
    Buffer (uint8_t *buf, int len, int cap):
            buffer(buf), _capacity(cap), _begin(buf), _end(buf+len),
            allocated(false) {}

    // Construct dynamic buffer, initially empty.
    Buffer (): _capacity(RESIZE_UNIT), allocated(true) {
        buffer = reinterpret_cast<uint8_t*>(malloc(_capacity));
        _begin = _end = buffer;
    }

    ~Buffer () {
        if (allocated) {
            free(buffer);
        }
    }

    inline int capacity () { return _capacity; }
    inline uint8_t read () { return *_begin++; }
    inline void readBytes (uint8_t *buf, int len) {
        while (len-- > 0) {
            *buf++ = *_begin++;
        }
    }
    inline uint8_t peek (int i) { return _begin[i]; }
    inline void write (uint8_t d) { *_end++ = d; }
    inline void writeBytes (const uint8_t *buf, int len) {
        while (len-- > 0) {
            *_end++ = *buf++;
        }
    }
    inline int size () { return _end - _begin; }
    inline int margin () { return buffer + _capacity - _end; }
    inline void reset () { _begin = _end = buffer; }
    void moveFront () {
        if (_begin == buffer) {
            return;
        }
        int s = size();
        if (!s) {
            reset();
            return;
        }
        memmove(buffer, _begin, s);
        _begin = buffer;
        _end = buffer + s;
    }
    inline void grow (int s) { _end += s; }
    inline void drop (int s) { _begin += s; }
    inline uint8_t *begin () { return _begin; }
    inline uint8_t *end () { return _end; }

    inline bool ensureMargin (int s) {
        if (!allocated) return false;

        int b = _begin - buffer;
        int e = _end - buffer;
        s = (s + e + (RESIZE_UNIT - 1)) / RESIZE_UNIT * RESIZE_UNIT;
        if (s <= _capacity) {
            return true;
        }
        buffer = reinterpret_cast<uint8_t*>(realloc(buffer, s));
        _capacity = s;
        _begin = buffer + b;
        _end = buffer + e;
        return true;
    }
};

}
#endif
