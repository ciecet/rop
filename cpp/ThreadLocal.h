#ifndef THREAD_LOCAL_H
#define THREAD_LOCAL_H

#ifndef ROP_THREAD_LOCAL_IMPL
#define ROP_THREAD_LOCAL_IMPL 1
#endif

#if ROP_THREAD_LOCAL_IMPL == 0

#include <stdlib.h>
#include <pthread.h>

namespace acba {

template <typename T>
class ThreadLocal {

    pthread_key_t key;

public:

    ThreadLocal () {
        pthread_key_create(&key, free);
    }

    ~ThreadLocal () {
        pthread_key_delete(key);
    }

    T *get () {
        void *p = pthread_getspecific(key);
        if (!p) {
            p = calloc(1, sizeof(T));
            pthread_setspecific(key, p);
        }
        return reinterpret_cast<T*>(p);
    }

    T &operator* () {
        return *get();
    }
    T *operator-> () const {
        return get();
    }
};

}

#define THREAD_LOCAL(T,K) static ThreadLocal<T> K
#define THREAD_GET(K) (*(K))

#elif ROP_THREAD_LOCAL_IMPL == 1

#define THREAD_LOCAL(T,K) static __thread T K
#define THREAD_GET(K) (K)

#endif

#endif
