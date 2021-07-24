/* POWER feature check
 * Copyright (C) 2020 Matheus Castanho <msc@linux.ibm.com>, IBM
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#include <sys/auxv.h>
#include "../../zutil.h"

Z_INTERNAL int power_cpu_has_arch_2_07;

void Z_INTERNAL power_check_features(void) {
    unsigned long hwcap2;
    hwcap2 = getauxval(AT_HWCAP2);

#ifdef POWER8
    if (hwcap2 & PPC_FEATURE2_ARCH_2_07)
      power_cpu_has_arch_2_07 = 1;
#endif
}
