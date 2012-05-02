#ifndef REF_COUNT_H
#define REF_COUNT_H

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
};

template <typename T>
class RefCounted {
    int refCount;
public:
    RefCounted (): refCount(0) {}

    void ref() {
        refCount++;
    }

    void deref() {
        refCount--;
        if (refCount == 0) {
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
};

#endif
