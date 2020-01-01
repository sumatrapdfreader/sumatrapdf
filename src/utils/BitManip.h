/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

/* Simple functions to make it easier to set/clear/test for bits in integers */

namespace bit {

template <typename T>
void Set(T& v, int bitNo) {
    T mask = 1 << bitNo;
    v |= mask;
}

template <typename T>
void Set(T& v, int bit1, int bit2) {
    T mask = 1 << bit1;
    v |= mask;
    mask = 1 << bit2;
    v |= mask;
}

template <typename T>
T FromBit(int bitNo) {
    T v = (T)(1 << bitNo);
    return v;
}

template <typename T>
void Clear(T& v, int bitNo) {
    T mask = 1 << bitNo;
    v &= ~mask;
}

template <typename T>
bool IsSet(T v, int bitNo) {
    T mask = (T)1 << bitNo;
    return (v & mask) != 0;
}

template <typename T>
bool IsMaskSet(T v, T mask) {
    return (v & mask) != 0;
}

} // namespace bit

namespace bitmask {

template <typename T>
bool IsSet(T v, decltype(v) mask) {
    return (v & mask) != 0;
}

template <typename T>
bool IsClear(T v, decltype(v) mask) {
    return (v & ~mask) == 0;
}

} // namespace bitmask
