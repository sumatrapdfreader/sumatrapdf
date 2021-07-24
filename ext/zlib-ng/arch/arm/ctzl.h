#ifndef ARM_CTZL_H
#define ARM_CTZL_H

#include <armintr.h>

#if defined(_MSC_VER) && !defined(__clang__)
static __forceinline unsigned long __builtin_ctzl(unsigned long value) {
    return _arm_clz(_arm_rbit(value));
}
#endif

#endif
