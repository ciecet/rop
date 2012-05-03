#ifndef REF_COUNT_H
#define REF_COUNT_H

namespace base {

/**
 * Generic smart pointer which points to reference counted object.
 * Reference counted object should have ref()/deref() methods, and
 * reference counter starts from zero.
 */
template <typename T>
class Ref {
    T *object;
public:
    Ref (): object(0) {}
    Ref (T *o): object(o) { object->ref(); }
    Ref (const Ref<T> &r): object(r.object) { object->ref(); }
    ~Ref () {
        if (object) {
            object->deref();
        }
    }
    T &operator* () {
        return *object;
    }
    T *operator-> () {
        return object;
    }
    T *get () {
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
    }
    Ref<T> &operator=(Ref<T> &r) {
        if (object) {
            object->deref();
        }
        object = r.object;
        object->ref();
    }
    operator void*() { return object; }
    bool operator==(const Ref<T>& r) {
        return object == r.object;
    }
    bool operator!=(const Ref<T>& r) {
        return object != r.object;
    }
};

template <typename T>
class RefCounted {
    int refCount;
public:
    RefCounted (): refCount(0) {}

    void ref() {
        __sync_add_and_fetch(&refCount, 1);
    }

    void deref() {
        if (__sync_sub_and_fetch(&refCount, 1) == 0) {
            delete static_cast<T*>(this);
        }
    }
};

template <typename T>
struct Container: RefCounted<Container<T> > {
    T value;
};

template <typename T>
class ContainerRef {
    Container<T> *object;
public:
    ContainerRef (): object(0) {}
    ContainerRef (Container<T> *o): object(o) { object->ref(); }
    ContainerRef (const ContainerRef<T> &r): object(r.object) { object->ref(); }
    ~ContainerRef () {
        if (object) {
            object->deref();
        }
    }
    T &operator* () {
        return object->value;
    }
    T *operator-> () {
        return &object->value;
    }
    T *get () {
        if (object) {
            return &object->value;
        } else {
            return 0;
        }
    }
    Ref<T> &operator=(T *o) {
        if (object) {
            object->deref();
        }
        object = o;
        if (object) {
            object->ref();
        }
    }
    Ref<T> &operator=(Ref<T> &r) {
        if (object) {
            object->deref();
        }
        object = r.object;
        object->ref();
    }
    operator void*() { return object; }
    bool operator==(const Ref<T>& r) {
        return object == r.object;
    }
    bool operator!=(const Ref<T>& r) {
        return object != r.object;
    }
};

}

#endif
