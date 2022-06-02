/* insert_string_acle.c -- insert_string variant using ACLE's CRC instructions
 *
 * Copyright (C) 1995-2013 Jean-loup Gailly and Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 *
 */

#ifdef ARM_ACLE_CRC_HASH
#ifndef _MSC_VER
#  include <arm_acle.h>
#endif
#include "../../zbuild.h"
#include "../../deflate.h"

#define UPDATE_HASH(s, h, val) \
    h = __crc32w(0, val)

#define INSERT_STRING       insert_string_acle
#define QUICK_INSERT_STRING quick_insert_string_acle

#include "../../insert_string_tpl.h"
#endif
