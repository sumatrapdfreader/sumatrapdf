/*
 * wrgif-12.c
 *
 * Copyright (C) 2025, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file is a wrapper for compiling wrgif.c to support 12 bits of
 * data precision.  wrgif.c should not be compiled directly.
 */

#define BITS_IN_JSAMPLE  12

#include "../wrgif.c"
