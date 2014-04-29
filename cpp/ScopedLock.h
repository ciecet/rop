#ifndef SCOPED_LOCK_H
#define SCOPED_LOCK_H

#include <pthread.h>

namespace acba {

struct ScopedLock {
    pthread_mutex_t *mutex;

    ScopedLock (pthread_mutex_t &m): mutex(&m) {
        pthread_mutex_lock(mutex);
    }

    ~ScopedLock () {
        pthread_mutex_unlock(mutex);
    }
};

struct ScopedUnlock {
    pthread_mutex_t *mutex;

    ScopedUnlock (pthread_mutex_t &m): mutex(&m) {
        pthread_mutex_unlock(mutex);
    }

    ~ScopedUnlock () {
        pthread_mutex_lock(mutex);
    }
};

}

#endif
