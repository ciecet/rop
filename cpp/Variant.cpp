#include <assert.h>
#include <new>
#include <string>
#include <sstream>
#include <ostream>
#include <stdio.h>
#include "Variant.h"

using namespace std;
using namespace acba;

struct NullValueHandler: ValueHandler {
    VARIANT_TYPE getType () const { return VARIANT_TYPE_NULL; }
    int compare (const Value *a, const Value *b) const { return 0; }

    virtual void write (ostream &os, const Value *v) const {
        os << "null";
    }
} nullValueHandler;

struct BoolValueHandler: ValueHandler {
    VARIANT_TYPE getType () const { return VARIANT_TYPE_BOOL; }
    void setValue (Value *v, const void *r) {
        v->b = *static_cast<const bool*>(r);
    }
    const void *getValue (const Value *v) const { return &v->b; }
    bool asBool (const Value *v) const { return v->b; }
    int64_t asInteger (const Value *v) const { return v->b ? 1 : 0; }
    int compare (const Value *a, const Value *b) const {
        if (a->b) {
            return b->b ? 0 : 1;
        } else {
            return b->b ? -1 : 0;
        }
    }
    void write (ostream &os, const Value *v) const {
        os << (v->b ? "true" : "false");
    }
} boolValueHandler;

struct IntValueHandler: ValueHandler {
    VARIANT_TYPE getType () const { return VARIANT_TYPE_INT; }
    void setValue (Value *v, const void *r) {
        v->i = *static_cast<const int64_t*>(r);
    }
    const void *getValue (const Value *v) const { return &v->i; }
    bool asBool (const Value *v) const { return (v->i != 0); }
    int64_t asInteger (const Value *v) const { return v->i; }
    double asFloat (const Value *v) const { return static_cast<double>(v->i); }
    int compare (const Value *a, const Value *b) const {
        return (int)(a->i - b->i);
    }
    void write (ostream &os, const Value *v) const {
        os << asInteger(v);
    }
} intValueHandler;

struct FloatValueHandler: ValueHandler {
    VARIANT_TYPE getType () const { return VARIANT_TYPE_FLOAT; }
    void setValue (Value *v, const void *r) {
        v->d = *static_cast<const double*>(r);
    }
    const void *getValue (const Value *v) const { return &v->d; }
    int64_t asInteger (const Value *v) const {
        return static_cast<int64_t>(v->d);
    }
    double asFloat (const Value *v) const { return v->d; }
    int compare (const Value *a, const Value *b) const {
        double d = a->d - b->d;
        if (d < 0.0) return -1;
        if (d > 0.0) return 1;
        return 0;
    }
    void write (ostream &os, const Value *v) const {
        os << asFloat(v);
    }
} floatValueHandler;

struct StringValueHandler: ValueHandler {
    VARIANT_TYPE getType () const { return VARIANT_TYPE_STRING; }
    void setValue (Value *v, const void *r) {
        new (v->s) string(*static_cast<const string*>(r));
    }
    const void *getValue (const Value *v) const { return v->s; }
    void dispose (Value *v) { reinterpret_cast<string*>(v->s)->~string(); }
    const string &asString (const Value *v) const {
        return *reinterpret_cast<const string*>(v->s);
    }
    int compare (const Value *a, const Value *b) const {
        return asString(a).compare(asString(b));
    }
    void write (ostream &os, const Value *v) const {
        const string &s = asString(v);
        os << '"';
        for (string::const_iterator i = s.begin(); i != s.end(); i++) {
            char c = *i;
            if (c == '"') {
                os << "\\\"";
            } else {
                os << c;
            }
        }
        os << '"';
    }
} stringValueHandler;

struct ListValueHandler: ValueHandler {
    typedef Ref<Variant::List> ListRef;
    VARIANT_TYPE getType () const { return VARIANT_TYPE_LIST; }
    void setValue (Value *v, const void *r) {
        // need to clear const qualifier to use ref count.
        new (v->s) ListRef(static_cast<Variant::List*>(const_cast<void*>(r)));
    }
    const void *getValue (const Value *v) const {
        return reinterpret_cast<const ListRef*>(v->s)->get();
    }
    void dispose (Value *v) { reinterpret_cast<ListRef*>(v->s)->~ListRef(); }
    Variant::List *asList (const Value *v) const {
        return reinterpret_cast<const ListRef*>(v->s)->get();
    }
    int compare (const Value *a, const Value *b) const {
        assert(!"Cannot compare variant list");
        return 0;
    }
    void write (ostream &os, const Value *v) const {
        const Variant::List *l = asList(v);
        os << "[";
        if (l->size() > 0) {
            Variant::List::const_iterator i = l->begin();
            while (true) {
                os << *i;
                i++;
                if (i == l->end()) break;
                os << ", ";
            }
        }
        os << "]";
    }
} listValueHandler;

struct MapValueHandler: ValueHandler {
    typedef Ref<Variant::Map> MapRef;
    VARIANT_TYPE getType () const { return VARIANT_TYPE_MAP; }
    void setValue (Value *v, const void *r) {
        // need to clear const qualifier to use ref count.
        new (v->s) MapRef(static_cast<Variant::Map*>(const_cast<void*>(r)));
    }
    const void *getValue (const Value *v) const {
        return reinterpret_cast<const MapRef*>(v->s)->get();
    }
    void dispose (Value *v) { reinterpret_cast<MapRef*>(v->s)->~MapRef(); }
    Variant::Map *asMap (const Value *v) const {
        return reinterpret_cast<const MapRef*>(v->s)->get();
    }
    int compare (const Value *a, const Value *b) const {
        assert(!"Cannot compare variant map");
        return 0;
    }
    void write (ostream &os, const Value *v) const {
        const Variant::Map *m = asMap(v);
        os << "{";
        if (m->size() > 0) {
            Variant::Map::const_iterator i = m->begin();
            while (true) {
                os << i->first;
                os << ":";
                os << i->second;
                i++;
                if (i == m->end()) break;
                os << ", ";
            }
        }
        os << "}";
    }
} mapValueHandler;

ValueHandler *handlers[] = {
    &nullValueHandler,
    &boolValueHandler,
    &intValueHandler,
    &floatValueHandler,
    &stringValueHandler,
    &listValueHandler,
    &mapValueHandler
};

Variant::Variant (const Variant &v)
{
    setValue(v.getType(), v.handler->getValue(&v.value));
}

Variant::~Variant ()
{
    handler->dispose(&value);
}

VARIANT_TYPE Variant::getType () const
{
    return handler->getType();
}

void Variant::setNull () {
    handler->dispose(&value);
    setValue(VARIANT_TYPE_NULL, 0);
}

Variant &Variant::operator= (const Variant &v) {
    handler->dispose(&value);
    setValue(v.getType(), v.handler->getValue(&v.value));
    return *this;
}

Variant &Variant::operator= (bool b) {
    handler->dispose(&value);
    setValue(VARIANT_TYPE_BOOL, &b);
    return *this;
}

Variant &Variant::operator= (int32_t i) {
    handler->dispose(&value);
    int64_t l = i;
    setValue(VARIANT_TYPE_INT, &l);
    return *this;
}

Variant &Variant::operator= (int64_t i) {
    handler->dispose(&value);
    setValue(VARIANT_TYPE_INT, &i);
    return *this;
}

Variant &Variant::operator= (double d) {
    handler->dispose(&value);
    setValue(VARIANT_TYPE_FLOAT, &d);
    return *this;
}

Variant &Variant::operator= (const std::string &s)
{
    handler->dispose(&value);
    setValue(VARIANT_TYPE_STRING, &s);
    return *this;
}

Variant &Variant::operator= (const char *s)
{
    handler->dispose(&value);
    string str(s);
    setValue(VARIANT_TYPE_STRING, &str);
    return *this;
}

Variant &Variant::operator= (const Variant::List *v)
{
    handler->dispose(&value);
    setValue(VARIANT_TYPE_LIST, v);
    return *this;
}

Variant &Variant::operator= (const Variant::Map *m)
{
    handler->dispose(&value);
    setValue(VARIANT_TYPE_MAP, m);
    return *this;
}

bool Variant::asBool () const
{
    return handler->asBool(&value);
}

int32_t Variant::asI32 () const
{
    return (int32_t)handler->asInteger(&value);
}

int64_t Variant::asI64 () const
{
    return handler->asInteger(&value);
}

float Variant::asF32 () const
{
    return (float)handler->asFloat(&value);
}

double Variant::asF64 () const
{
    return handler->asFloat(&value);
}

const std::string &Variant::asString () const
{
    return handler->asString(&value);
}

Variant::List *Variant::asList () const
{
    return handler->asList(&value);
}

Variant::Map *Variant::asMap () const
{
    return handler->asMap(&value);
}

bool Variant::operator< (const acba::Variant &o) const
{
    if (handler != o.handler) {
        return (handler->getType() < o.handler->getType());
    }
    return handler->compare(&value, &o.value) < 0;
}

const string Variant::toString () const
{
    stringstream ss;
    ss << *this;
    return ss.str();
}

void Variant::setValue (VARIANT_TYPE t, const void *r)
{
    handler = handlers[t];
    handler->setValue(&value, r);
}

const string ValueHandler::emptyString;

ostream &operator<< (ostream &os, const acba::Variant &v)
{
    v.handler->write(os, &v.value);
    return os;
}
