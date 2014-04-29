#ifndef REF_H
#define REF_H

#include <stdint.h>
#include <pthread.h>

namespace acba {

/**
 * Generic smart pointer which points to reference counted object.
 * Reference counted object should have ref()/deref() methods, and
 * reference count starts from zero.
 */
template <typename T>
class Ref {
    T *object;
public:
    Ref (): object(0) {}
    Ref (T *o): object(o) {
        if (o) {
            o->ref();
        }
    }
    Ref (const Ref<T> &r): object(r.object) {
        if (object) {
            object->ref();
        }
    }
    ~Ref () {
        if (object) {
            object->deref();
        }
    }
    T &operator* () {
        return *object;
    }
    T *operator-> () const {
        return object;
    }
    T *get () const {
        return object;
    }
    Ref<T> &operator=(T *o) {
        if (object) {
            object->deref();
        }
        object = o;
        if (object) {
            object->ref();
        }
        return *this;
    }
    Ref<T> &operator=(const Ref<T> &r) {
        if (this == &r) return *this;
        if (object) {
            object->deref();
        }
        object = r.object;
        if (object) object->ref();
        return *this;
    }
    operator T*() const { return object; }
    bool operator==(const Ref<T>& r) {
        return object == r.object;
    }
    bool operator!=(const Ref<T>& r) {
        return object != r.object;
    }
};

template <typename T>
class RefCounter {
    int refCount;
public:
    RefCounter (): refCount(0) {}

    void ref () {
        __sync_add_and_fetch(&refCount, 1);
    }

    void deref () {
        if (__sync_sub_and_fetch(&refCount, 1) == 0) {
            delete static_cast<T*>(this);
        }
    }
};

template <typename T>
struct RefCounted: T, RefCounter<RefCounted<T> > {
    RefCounted (): T() {}
    RefCounted (const T &v): T(v) {}
};

}

#endif
