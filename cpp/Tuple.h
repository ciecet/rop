#ifndef TUPLE_H
#define TUPLE_H

namespace base {

struct TupleEnd {};

template <
    template <typename> class ItemType,
    typename ItemBaseType,
    int INDEX,
    typename A = TupleEnd, typename B = TupleEnd, typename C = TupleEnd,
    typename D = TupleEnd, typename E = TupleEnd, typename F = TupleEnd,
    typename G = TupleEnd, typename H = TupleEnd, typename I = TupleEnd,
    typename J = TupleEnd, typename K = TupleEnd, typename L = TupleEnd,
    typename M = TupleEnd, typename N = TupleEnd, typename O = TupleEnd,
    typename P = TupleEnd, typename Q = TupleEnd, typename R = TupleEnd,
    typename S = TupleEnd, typename T = TupleEnd, typename U = TupleEnd,
    typename V = TupleEnd, typename W = TupleEnd, typename X = TupleEnd,
    typename Y = TupleEnd, typename Z = TupleEnd
>
struct TupleChain: TupleChain<ItemType, ItemBaseType, INDEX+1,
        B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X,Y,Z,TupleEnd> {
    ItemType<A> item;
    TupleChain() {
        TupleChain::items[INDEX] = &item;
    }
};

template <template <typename> class ItemType, typename B, int S>
struct TupleChain<ItemType, B, S, TupleEnd> {
    static const int SIZE = S;
    B *items[SIZE];
};

template <typename T>
struct TrivialTupleItem { T value; };
template <
    typename A = TupleEnd, typename B = TupleEnd, typename C = TupleEnd,
    typename D = TupleEnd, typename E = TupleEnd, typename F = TupleEnd,
    typename G = TupleEnd, typename H = TupleEnd, typename I = TupleEnd,
    typename J = TupleEnd, typename K = TupleEnd, typename L = TupleEnd,
    typename M = TupleEnd, typename N = TupleEnd, typename O = TupleEnd,
    typename P = TupleEnd, typename Q = TupleEnd, typename R = TupleEnd,
    typename S = TupleEnd, typename T = TupleEnd, typename U = TupleEnd,
    typename V = TupleEnd, typename W = TupleEnd, typename X = TupleEnd,
    typename Y = TupleEnd, typename Z = TupleEnd
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
