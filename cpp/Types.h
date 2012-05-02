#ifndef TYPES_H
#define TYPES_H

#include <new>
#include <string>
#include <vector>
#include <map>
#include <stdint.h>
#include "Port.h"

using namespace std;

#define BEGIN_STEP() switch (step) { case 0:
#define TRY_READ(type,var,stack) do {\
    if (Reader<type>(var).run(stack) == STOPPED) {\
        return STOPPED;\
    }\
} while (0)
#define TRY_WRITE(type,var,buf) do {\
    if (Writer<type>(var).run(stack) == STOPPED) {\
        return STOPPED;\
    }\
} while (0)
#define NEXT_STEP() step = __LINE__; case __LINE__: 
#define CALL() step = __LINE__; return CONTINUE; case __LINE__: 
#define END_STEP() default:; } return COMPLETE

template<> struct Reader<int8_t>: Frame {
    int8_t &obj;
    Reader (int8_t &o): obj(o) {}

    RESULT run (Stack *stack) {
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->size < 1) return STOPPED;
        obj = buf->read();
        return COMPLETE;
    }
};

template<> struct Writer<int8_t>: Frame {
    int8_t &obj;
    Writer (int8_t &o): obj(o) {}

    RESULT run (Stack *stack) {
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->margin() < 1) return STOPPED;
        buf->write(obj);
        return COMPLETE;
    }
};

template<> struct Reader<int16_t>: Frame {
    int16_t &obj;
    Reader (int16_t &o): obj(o) {}

    RESULT run (Stack *stack) {
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->size < 2) return STOPPED;
        int i = buf->read();
        i = (i<<8) + buf->read();
        obj = i;
        return COMPLETE;
    }
};

template<> struct Writer<int16_t>: Frame {
    int16_t &obj;
    Writer (int16_t &o): obj(o) {}

    RESULT run (Stack *stack) {
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->margin() < 2) return STOPPED;
        int i = obj;
        buf->write(i>>8);
        buf->write(i);
        return COMPLETE;
    }
};

template<> struct Reader<int32_t>: Frame {
    int32_t &obj;
    Reader (int32_t &o): obj(o) {}

    RESULT run (Stack *stack) {
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->size < 4) return STOPPED;
        int i = buf->read();
        i = (i<<8) + buf->read();
        i = (i<<8) + buf->read();
        i = (i<<8) + buf->read();
        obj = i;
        return COMPLETE;
    }
};

template<> struct Writer<int32_t>: Frame {
    int32_t &obj;
    Writer (int32_t &o): obj(o) {}

    RESULT run (Stack *stack) {
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        if (buf->margin() < 4) return STOPPED;
        int i = obj;
        buf->write(i>>24);
        buf->write(i>>16);
        buf->write(i>>8);
        buf->write(i);
        return COMPLETE;
    }
};

template<> struct Reader<string>: Frame {

    string &obj;
    int length;

    Reader (string &o): obj(o) {}

    RESULT run (Stack *stack) {

        // read size
        BEGIN_STEP();
        TRY_READ(int32_t, length, stack);
        obj->clear();
        obj->reserve(length + 1);

        // read each char
        NEXT_STEP();
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        while (length) {
            if (buf->size) {
                return STOPPED;
            }
            obj.push_back(static_cast<char>(buf->read()));
            length--;
        }

        END_STEP();
    }
};

template<> struct Writer<string>: Frame {

    string &obj;
    int i;

    Writer (string &o): obj(o) {}

    RESULT run (Stack *stack) {

        // write size 
        BEGIN_STEP();
        i = obj.size();
        TRY_WRITE(int32_t, i, stack);
        i = 0;

        // write chars
        NEXT_STEP();
        Buffer *buf = reinterpret_cast<Buffer*>(stack->env);
        while (i < obj.length()) {
            if (!buf->margin()) {
                return STOPPED;
            }
            buf->write(obj.at(i++));
        }

        END_STEP();
    }
};

template<typename T> struct Reader<vector<T> >: Frame {

    vector<T> &obj;
    int length;
    int i;

    Reader (vector<T> &o): obj(o) {}

    RESULT run (Stack *stack) {

        // read size
        BEGIN_STEP();
        TRY_READ(int32_t, length, stack);
        obj->clear();
        obj->reserve(length);
        i = 0;

        // read items
        NEXT_STEP();
        if (i < length) {
            new(stack->allocate(sizeof(Reader<T>))) Reader<T>(obj[i++]);
            return CONTINUE;
        }

        END_STEP();
    }
};

template<typename T> struct Writer<vector<T> >: Frame {

    vector<T> &obj;
    int i;

    Writer (vector<T> &o): obj(o) {}

    RESULT run (Stack *stack) {

        // write size
        BEGIN_STEP();
        i = obj->size();
        TRY_WRITE(int32_t, i, stack);
        i = 0;

        // write items
        NEXT_STEP();
        if (i < obj->size()) {
            new(stack->allocate(sizeof(Writer<T>))) Writer<T>(obj[i++]);
            return CONTINUE;
        }

        END_STEP();
    }
};

template<typename T, typename U> struct Reader<map<T,U> >: Frame {
    map<T,U> &obj;
    int n;
    T key;

    Reader (map<T,U> &d): obj(d) {}

    RESULT run (Stack *stack) {

        BEGIN_STEP();
        // read size
        TRY_READ(int32_t, n, stack);
        obj->clear();
        n *= 2;

        NEXT_STEP();
        if (n) {
            if ((n & 1) == 0) {
                // read key
                stack->newFrame(sizeof(Reader<T>)) Reader<T>(key);
                n--;
                return CONTINUE;
            } else {
                // read value
                stack->newFrame(sizeof(Reader<U>)) Reader<U>(obj[key]);
                n--;
                return CONTINUE;
            }
        }

        END_STEP();
    }
};

template<typename T, typename U> struct Writer<map<T,U> >: Frame {
    map<T,U>::iterator iter;
    bool first;

    Writer (map<T,U> &obj): obj(obj) {}

    RESULT run (Stack *stack) {

        // write size
        BEGIN_STEP();
        int size = obj->size();
        TRY_WRITE(int32_t, size, stack);
        iter = obj->begin();
        first = true;

        // write key and value
        NEXT_STEP();
        if (iter != obj->end()) {
            if (first) {
                new(stack->allocate(sizeof(Writer<T>))) Writer<T>(
                        const_cast<T*>(&iter->first));
                first = false;
                return CONTINUE;
            } else {
                new(stack->allocate(sizeof(Writer<U>))) Writer<U>(iter->second);
                iter++;
                first = true;
                return CONTINUE;
            }
        }

        END_STEP();
    }
};

template <typename T>
struct Reader<Ref<T> >: Frame {
    Ref<T> &obj;
    Reader (Ref<T> &o): obj(o) {}
    RESULT run (Stack *stack) {
        int i;
        BEGIN_STEP();
        TRY_READ(int8_t, i, stack);
        if (i) {
            obj = new T();
            stack->pushFrame(new (stack->allocate(sizeof(Reader<T>))) Reader<T>(*obj));
            CALL();
        } else {
            obj = 0;
        }
        END_STEP();
    }

    RESULT run (Stack *stack) {
    }
};

template <typename T>
struct Writer<Ref<T> >: Frame {
    Ref<T> &obj;
    Writer (Ref<T> &o): obj(o) {}

    RESULT run (Stack *stack) {
        BEGIN_STEP();
        if (obj) {
            stack->pushFrame(new (stack->allocate(sizeof(Writer<T>))) Writer<T>(*obj));
            CALL();
        } else {
            int i = 0;
            TRY_WRITE(int8_t, i, stack);
        }

        END_STEP();
    }
};
#endif
