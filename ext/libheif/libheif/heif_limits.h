/*
 * HEIF codec.
 * Copyright (c) 2018 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef LIBHEIF_HEIF_LIMITS_H
#define LIBHEIF_HEIF_LIMITS_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <cinttypes>
#include <cstddef>

static const size_t MAX_CHILDREN_PER_BOX = 20000;
static const int MAX_ILOC_ITEMS = 20000;
static const int MAX_ILOC_EXTENTS_PER_ITEM = 32;
static const int MAX_MEMORY_BLOCK_SIZE = 512 * 1024 * 1024; // 512 MB

// Artificial limit to avoid allocating too much memory.
// 32768^2 = 1.5 GB as YUV-4:2:0 or 4 GB as RGB32
static const int MAX_IMAGE_WIDTH = 32768;
static const int MAX_IMAGE_HEIGHT = 32768;

// Maximum nesting level of boxes in input files.
// We put a limit on this to avoid unlimited stack usage by malicious input files.
static const int MAX_BOX_NESTING_LEVEL = 20;

static const int MAX_BOX_SIZE = 0x7FFFFFFF; // 2 GB
static const int64_t MAX_LARGE_BOX_SIZE = 0x0FFFFFFFFFFFFFFF;
static const int64_t MAX_FILE_POS = 0x007FFFFFFFFFFFFFLL; // maximum file position
static const int MAX_FRACTION_VALUE = 0x10000;

#endif  // LIBHEIF_HEIF_LIMITS_H
