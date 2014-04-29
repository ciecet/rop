#ifndef VARIANT_H
#define VARIANT_H

#include <stdint.h>
#include <vector>
#include <map>
#include <string>
#include <ostream>

#include "Ref.h"

/*
 111xxxxx ~ 0xxxxxxx     integer in -32 ~ 127
 110xxxxx                String[0 ~ 30] 31:32bit (32-bit length followed)
 101xxxxx                List[0 ~ 30] 31:32bit
 1001xxxx                Map[0 ~ 14] 15:32bit

 10000000 null
 10000001
 10000010 false
 10000011 true
 10000100 i8
 10000101 i16
 10000110 i32
 10000111 i64
 10001000 f32
 10001001 f64
*/

namespace acba {
struct ValueHandler;
struct Variant;
}

std::ostream &operator<< (std::ostream &os, const acba::Variant &v);

namespace acba {

typedef enum {
    VARIANT_TYPE_NULL,
    VARIANT_TYPE_BOOL,
    VARIANT_TYPE_INT,
    VARIANT_TYPE_FLOAT,
    VARIANT_TYPE_STRING,
    VARIANT_TYPE_LIST,
    VARIANT_TYPE_MAP
} VARIANT_TYPE;

union Value {
    bool b;
    int64_t i;
    double d;
    void *p;
    char s[sizeof(std::string)];
};

struct Variant {

    typedef RefCounted<std::vector<Variant> > List;
    typedef RefCounted<std::map<Variant, Variant> > Map;

    Variant (const Variant &v);
    Variant () { setValue(VARIANT_TYPE_NULL, 0); }
    Variant (bool b) { setValue(VARIANT_TYPE_BOOL, &b); }
    Variant (int32_t i) { int64_t l = i; setValue(VARIANT_TYPE_INT, &l); }
    Variant (int64_t i) { setValue(VARIANT_TYPE_INT, &i); }
    Variant (double d) { setValue(VARIANT_TYPE_FLOAT, &d); }
    Variant (const std::string &s) { setValue(VARIANT_TYPE_STRING, &s); }
    Variant (const char *s) {
        std::string str(s);
        setValue(VARIANT_TYPE_STRING, &str);
    }
    Variant (const List *v) { setValue(VARIANT_TYPE_LIST, v); }
    Variant (const Map *m) { setValue(VARIANT_TYPE_MAP, m); }
    ~Variant ();

    VARIANT_TYPE getType () const;

    void setNull ();
    Variant &operator= (const Variant &v);
    Variant &operator= (bool b);
    Variant &operator= (int32_t i);
    Variant &operator= (int64_t i);
    Variant &operator= (double d);
    Variant &operator= (const std::string &s);
    Variant &operator= (const char *s);
    Variant &operator= (const List *v);
    Variant &operator= (const Map *m);

    bool isNull () const {
        return getType() == VARIANT_TYPE_NULL;
    }
    bool asBool () const;
    int32_t asI32 () const;
    int64_t asI64 () const;
    float asF32 () const;
    double asF64 () const;
    const std::string &asString () const;
    List *asList () const;
    Map *asMap () const;

    bool operator< (const acba::Variant &o) const;
    const std::string toString () const;

private:
    ValueHandler *handler;
    Value value;

    void setValue (VARIANT_TYPE t, const void *p);

    friend struct ValueHandler;
    friend std::ostream &::operator<< (std::ostream &os, const acba::Variant &v);
};

struct ValueHandler {
    static const std::string emptyString;

    virtual VARIANT_TYPE getType () const = 0;

    virtual void setValue (Value *v, const void *r) { }
    virtual const void *getValue (const Value *v) const { return 0; }
    virtual int compare (const Value *a, const Value *b) const = 0;
    virtual void dispose (Value *v) {} 

    virtual bool asBool (const Value *v) const { return false; }
    virtual int64_t asInteger (const Value *v) const { return 0; }
    virtual double asFloat (const Value *v) const { return 0.0; }
    virtual const std::string &asString (const Value *v) const {
        return emptyString;
    }
    virtual Variant::List *asList (const Value *v) const { return 0; }
    virtual Variant::Map *asMap (const Value *v) const { return 0; }

    virtual void write (std::ostream &os, const Value *v) const = 0;
};

}

#endif
