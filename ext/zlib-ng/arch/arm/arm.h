/* arm.h -- check for ARM features.
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef ARM_H_
#define ARM_H_

extern int arm_cpu_has_neon;
extern int arm_cpu_has_crc32;

void Z_INTERNAL arm_check_features(void);

#endif /* ARM_H_ */
