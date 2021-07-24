/* power.h -- check for POWER CPU features
 * Copyright (C) 2020 Matheus Castanho <msc@linux.ibm.com>, IBM
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef POWER_H_
#define POWER_H_

extern int power_cpu_has_arch_2_07;

void Z_INTERNAL power_check_features(void);

#endif /* POWER_H_ */
