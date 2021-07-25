#ifndef SHIFTLAB_SAFE_TYPEDEF_H__
#define SHIFTLAB_SAFE_TYPEDEF_H__

#include <string>
#include <iostream>
#include <map>

namespace detail {
    template <typename T> class empty_base {};
}

template <class T, class U, class B = ::detail::empty_base<T> >
struct less_than_comparable2 : B
{
     friend bool operator<=(const T& x, const U& y) { return !(x > y); }
     friend bool operator>=(const T& x, const U& y) { return !(x < y); }
     friend bool operator>(const U& x, const T& y)  { return y < x; }
     friend bool operator<(const U& x, const T& y)  { return y > x; }
     friend bool operator<=(const U& x, const T& y) { return !(y < x); }
     friend bool operator>=(const U& x, const T& y) { return !(y > x); }
};

template <class T, class B = ::detail::empty_base<T> >
struct less_than_comparable1 : B
{
     friend bool operator>(const T& x, const T& y)  { return y < x; }
     friend bool operator<=(const T& x, const T& y) { return !(y < x); }
     friend bool operator>=(const T& x, const T& y) { return !(x < y); }
};

template <class T, class U, class B = ::detail::empty_base<T> >
struct equality_comparable2 : B
{
     friend bool operator==(const U& y, const T& x) { return x == y; }
     friend bool operator!=(const U& y, const T& x) { return !(x == y); }
     friend bool operator!=(const T& y, const U& x) { return !(y == x); }
};

template <class T, class B = ::detail::empty_base<T> >
struct equality_comparable1 : B
{
     friend bool operator!=(const T& x, const T& y) { return !(x == y); }
};

template <class T, class U, class B = ::detail::empty_base<T> >
struct totally_ordered2
    : less_than_comparable2<T, U
    , equality_comparable2<T, U, B
      > > {};

template <class T, class B = ::detail::empty_base<T> >
struct totally_ordered1
    : less_than_comparable1<T
    , equality_comparable1<T, B
      > > {};

#define SAFE_TYPEDEF(T, D)                                      \
struct D                                                        \
    : totally_ordered1< D                                       \
    , totally_ordered2< D, T                                    \
    > >                                                         \
{                                                               \
    T t;                                                        \
    explicit D(const T t_) : t(t_) {};                          \
    D() = default;                                              \
    D(const D & t_) = default;                                  \
    D(D&&) = default;                                           \
    D & operator=(const D & rhs) = default;                     \
    D & operator=(D&&) = default;                               \
    operator T & () { return t; }                               \
    bool operator==(const D & rhs) const { return t == rhs.t; } \
    bool operator<(const D & rhs) const { return t < rhs.t; }   \
};

#endif // SHIFTLAB_SAFE_TYPEDEF_H__