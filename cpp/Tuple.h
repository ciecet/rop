#ifndef TUPLE_H
#define TUPLE_H

namespace acba {

template <
    typename A = void, typename B = void, typename C = void,
    typename D = void, typename E = void, typename F = void,
    typename G = void, typename H = void, typename I = void,
    typename J = void, typename K = void, typename L = void,
    typename M = void, typename N = void, typename O = void,
    typename P = void, typename Q = void, typename R = void,
    typename S = void, typename T = void, typename U = void,
    typename V = void, typename W = void, typename X = void,
    typename Y = void, typename Z = void
>
struct Tuple {
    A head;
    typedef Tuple<B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> tail_t;
    tail_t tail;

    inline A *at0 () { return &head; }
    inline B *at1 () { return tail.at0(); }
    inline C *at2 () { return tail.at1(); }
    inline D *at3 () { return tail.at2(); }
    inline E *at4 () { return tail.at3(); }
    inline F *at5 () { return tail.at4(); }
    inline G *at6 () { return tail.at5(); }
    inline H *at7 () { return tail.at6(); }
    inline I *at8 () { return tail.at7(); }
    inline J *at9 () { return tail.at8(); }
    inline K *at10 () { return tail.at9(); }
    inline L *at11 () { return tail.at10(); }
    inline M *at12 () { return tail.at11(); }
    inline N *at13 () { return tail.at12(); }
    inline O *at14 () { return tail.at13(); }
    inline P *at15 () { return tail.at14(); }
    inline Q *at16 () { return tail.at15(); }
    inline R *at17 () { return tail.at16(); }
    inline S *at18 () { return tail.at17(); }
    inline T *at19 () { return tail.at18(); }
    inline U *at20 () { return tail.at19(); }
    inline V *at21 () { return tail.at20(); }
    inline W *at22 () { return tail.at21(); }
    inline X *at23 () { return tail.at22(); }
    inline Y *at24 () { return tail.at23(); }
    inline Z *at25 () { return tail.at24(); }
};

template <typename A>
struct Tuple<A,void> {
    A head;

    inline A *at0 () { return &head; }
    inline void *at1 () { return 0; }
    inline void *at2 () { return 0; }
    inline void *at3 () { return 0; }
    inline void *at4 () { return 0; }
    inline void *at5 () { return 0; }
    inline void *at6 () { return 0; }
    inline void *at7 () { return 0; }
    inline void *at8 () { return 0; }
    inline void *at9 () { return 0; }
    inline void *at10 () { return 0; }
    inline void *at11 () { return 0; }
    inline void *at12 () { return 0; }
    inline void *at13 () { return 0; }
    inline void *at14 () { return 0; }
    inline void *at15 () { return 0; }
    inline void *at16 () { return 0; }
    inline void *at17 () { return 0; }
    inline void *at18 () { return 0; }
    inline void *at19 () { return 0; }
    inline void *at20 () { return 0; }
    inline void *at21 () { return 0; }
    inline void *at22 () { return 0; }
    inline void *at23 () { return 0; }
    inline void *at24 () { return 0; }
    inline void *at25 () { return 0; }
};

}

#endif
