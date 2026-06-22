/*
 * @WRAPPER_FILE@
 *
 * Copyright (C) 2025, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 *
 * This file is a wrapper for compiling @FILE@ to support @BITS_RANGE@ bits of
 * data precision.  @FILE@ should not be compiled directly.
 */

#define BITS_IN_JSAMPLE  @BITS@

#include "../@FILE@"
