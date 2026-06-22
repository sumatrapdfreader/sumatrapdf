/*
 * jquant1-8.c
 *
 * Copyright (C) 2025, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file is a wrapper for compiling jquant1.c to support 8 bits of
 * data precision.  jquant1.c should not be compiled directly.
 */

#define BITS_IN_JSAMPLE  8

#include "../jquant1.c"
