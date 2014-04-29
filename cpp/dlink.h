#ifndef DLINK_H
#define DLINK_H

#include <stddef.h>
#ifndef container_of
#define container_of(ptr, type, member) ({ \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#endif

typedef struct dlink_t dlink_t;
struct dlink_t {
    dlink_t *prev;
    dlink_t *next;
};

inline void dlink_init (dlink_t *node) {
    node->prev = node;
    node->next = node;
}

inline bool dlink_empty (dlink_t *node) {
    return node->next == node;
}

inline void dlink_add_after (dlink_t *node, dlink_t *after)
{
    node->next = after->next;
    node->prev = after;
    after->next->prev = node;
    after->next = node;
}

inline void dlink_add_before (dlink_t *node, dlink_t *before)
{
    node->prev = before->prev;
    node->next = before;
    before->prev->next = node;
    before->prev = node;
}

inline void dlink_remove (dlink_t *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
}

#ifdef __cplusplus

template <int C>
struct DLinkArray {
    dlink_t dlinks[C];
    static const int DLinkArraySize = C;
};

template <class T, int C>
struct DLinkPicker {
    static dlink_t *getLink (T *t) {
        return &t->dlinks[C];
    }

    static T *getContainer (dlink_t *l) {
        return static_cast<T*>(container_of(l, DLinkArray<T::DLinkArraySize>,
                dlinks[C]));
    }
};

template <class T, int C>
struct IntrusiveDoubleList: DLinkPicker<T,C> {
    IntrusiveDoubleList () {
        clear();
    }

    typedef DLinkPicker<T,C> Base;

    inline void clear () {
        dlink_init(&list);
    }

    inline bool isEmpty () {
        return dlink_empty(&list);
    }

    inline void addFront (T *item) {
        dlink_add_after (Base::getLink(item), &list);
    }

    inline void addBack (T *item) {
        dlink_add_before (Base::getLink(item), &list);
    }

    inline static void remove (T *item) {
        dlink_remove(Base::getLink(item));
    }

    inline static void addAfter (T *item, T *after) {
        dlink_add_after (Base::getLink(item), Base::getLink(after));
    }

    inline T *removeFront () {
        dlink_t *lnk = list.next;
        if (lnk == &list) return 0;
        dlink_remove(lnk);
        return Base::getContainer(lnk);
    }

    inline T *removeBack () {
        dlink_t *lnk = list.prev;
        if (lnk == &list) return 0;
        dlink_remove(lnk);
        return Base::getContainer(lnk);
    }

    inline T *first () {
        return toContainer(list.next);
    }

    inline T *last () {
        return toContainer(list.prev);
    }

    inline T *next (T *item) {
        return toContainer(Base::getLink(item)->next);
    }

    inline T *prev (T *item) {
        return toContainer(Base::getLink(item)->prev);
    }

private:

    inline T *toContainer (dlink_t *l) {
        if (l == &list) return 0;
        return Base::getContainer(l);
    }

    dlink_t list;
};

#endif

#endif
