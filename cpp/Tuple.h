#ifndef TUPLE_H
#define TUPLE_H

namespace base {

struct TupleTerminator {};

template <
    template <typename> class ItemType,
    typename ItemBaseType,
    int INDEX,
    typename A = TupleTerminator, typename B = TupleTerminator,
    typename C = TupleTerminator, typename D = TupleTerminator,
    typename E = TupleTerminator, typename F = TupleTerminator,
    typename G = TupleTerminator, typename H = TupleTerminator,
    typename I = TupleTerminator, typename J = TupleTerminator,
    typename K = TupleTerminator, typename L = TupleTerminator,
    typename M = TupleTerminator, typename N = TupleTerminator,
    typename O = TupleTerminator, typename P = TupleTerminator,
    typename Q = TupleTerminator, typename R = TupleTerminator,
    typename S = TupleTerminator, typename T = TupleTerminator,
    typename U = TupleTerminator, typename V = TupleTerminator,
    typename W = TupleTerminator, typename X = TupleTerminator,
    typename Y = TupleTerminator, typename Z = TupleTerminator
>
struct TupleChain: TupleChain<ItemType, ItemBaseType, INDEX+1,
        B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,TupleTerminator> {
    ItemType<A> item;
    TupleChain() {
        TupleChain::items[INDEX] = &item;
    }
};

template <template <typename> class ItemType, typename B, int S>
struct TupleChain<ItemType, B, S, TupleTerminator> {
    static const int SIZE = S;
    B *items[SIZE];
};

template <typename T>
struct TrivialTupleItem { T value; };
template <
    typename A = TupleTerminator, typename B = TupleTerminator,
    typename C = TupleTerminator, typename D = TupleTerminator,
    typename E = TupleTerminator, typename F = TupleTerminator,
    typename G = TupleTerminator, typename H = TupleTerminator,
    typename I = TupleTerminator, typename J = TupleTerminator,
    typename K = TupleTerminator, typename L = TupleTerminator,
    typename M = TupleTerminator, typename N = TupleTerminator,
    typename O = TupleTerminator, typename P = TupleTerminator,
    typename Q = TupleTerminator, typename R = TupleTerminator,
    typename S = TupleTerminator, typename T = TupleTerminator,
    typename U = TupleTerminator, typename V = TupleTerminator,
    typename W = TupleTerminator, typename X = TupleTerminator,
    typename Y = TupleTerminator, typename Z = TupleTerminator
>
struct Tuple: TupleChain<TrivialTupleItem, void, 0, 
        A, B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z> {
    template <typename AS>
    AS &get (int i) {
        return *static_cast<TrivialTupleItem<AS>*>(Tuple::items[i])->value;
    }
};

}
#endif
