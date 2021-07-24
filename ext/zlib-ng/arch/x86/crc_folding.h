/* crc_folding.h
 *
 * Compute the CRC32 using a parallelized folding approach with the PCLMULQDQ
 * instruction.
 *
 * Copyright (C) 2013 Intel Corporation Jim Kukunas
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

#ifndef CRC_FOLDING_H_
#define CRC_FOLDING_H_

#include "../../zutil.h"

Z_INTERNAL uint32_t crc_fold_init(unsigned int crc0[4 * 5]);
Z_INTERNAL uint32_t crc_fold_512to32(unsigned int crc0[4 * 5]);
Z_INTERNAL void crc_fold_copy(unsigned int crc0[4 * 5], unsigned char *, const unsigned char *, long);

#endif
