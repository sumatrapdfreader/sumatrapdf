/*
 * HEIF codec.
 * Copyright (c) 2018 Dirk Farin <dirk.farin@gmail.com>
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
#ifndef LIBHEIF_SECURITY_LIMITS_H
#define LIBHEIF_SECURITY_LIMITS_H

#include "libheif/heif.h"
#include <cinttypes>
#include <cstddef>
#include "error.h"


extern heif_security_limits global_security_limits;
extern heif_security_limits disabled_security_limits;

// Maximum nesting level of boxes in input files.
// We put a limit on this to avoid unlimited stack usage by malicious input files.
static const int MAX_BOX_NESTING_LEVEL = 20;

static const int MAX_BOX_SIZE = 0x7FFFFFFF; // 2 GB
static const int64_t MAX_LARGE_BOX_SIZE = 0x0FFFFFFFFFFFFFFF;
static const int64_t MAX_FILE_POS = 0x007FFFFFFFFFFFFFLL; // maximum file position
static const int MAX_FRACTION_VALUE = 0x10000;


Error check_for_valid_image_size(const heif_security_limits* limits, uint32_t width, uint32_t height);

// Maximum coding-unit size (in pixels) that the given codec may pad a coded
// frame up to. Used as the margin for tighten_image_size_limit_for_ispe.
// Returns 0 for codecs without coding-unit padding (e.g. uncompressed).
uint32_t max_coding_unit_size_for_codec(heif_compression_format format);

// Return a copy of `base` with max_image_size_pixels lowered to a value
// just above the declared image size. This is used to bound how much memory
// a codec plugin may allocate for an image whose internal (codec-declared)
// dimensions exceed the file-declared (ispe) dimensions — without us having
// to parse the codec bitstream ourselves.
//
// `coding_unit_size` is the maximum coding-unit size of the target codec
// (e.g. 128 for AV1/VVC, 64 for HEVC, 16 for AVC). The allowed coded
// dimensions are (ispe + coding_unit_size) in each axis, since a codec may
// pad the coded frame up to a coding-unit boundary.
heif_security_limits tighten_image_size_limit_for_ispe(const heif_security_limits* base,
                                                       uint32_t ispe_width,
                                                       uint32_t ispe_height,
                                                       uint32_t coding_unit_size);


class TotalMemoryTracker
{
public:
  explicit TotalMemoryTracker(const heif_security_limits* limits_context);
  ~TotalMemoryTracker();

  size_t get_max_total_memory_used() const;

  void operator=(const TotalMemoryTracker&) = delete;
  TotalMemoryTracker(const TotalMemoryTracker&) = delete;

private:
  const heif_security_limits* m_limits_context = nullptr;
};


class MemoryHandle
{
public:
  MemoryHandle() = default;
  ~MemoryHandle() { free(); }

  Error alloc(size_t memory_amount, const heif_security_limits* limits_context, const char* reason_description);

  // calloc-style overload: checks `count * element_size` for size_t overflow before allocating.
  // Use this when allocating an array whose total size is count*element_size, to avoid silent
  // truncation on 32-bit builds when count is near UINT32_MAX.
  Error alloc(size_t count, size_t element_size,
              const heif_security_limits* limits_context, const char* reason_description);

  void free();

  void free(size_t memory_amount);

  const heif_security_limits* get_security_limits() const { return m_limits_context; }

  MemoryHandle(const MemoryHandle&) = delete;
  MemoryHandle& operator=(const MemoryHandle&) = delete;

  MemoryHandle(MemoryHandle&& other) noexcept
      : m_limits_context(other.m_limits_context), m_memory_amount(other.m_memory_amount)
  {
    other.m_limits_context = nullptr;
    other.m_memory_amount = 0;
  }

  MemoryHandle& operator=(MemoryHandle&& other) noexcept
  {
    if (this != &other) {
      free();
      m_limits_context = other.m_limits_context;
      m_memory_amount = other.m_memory_amount;
      other.m_limits_context = nullptr;
      other.m_memory_amount = 0;
    }
    return *this;
  }

private:
  const heif_security_limits* m_limits_context = nullptr;
  size_t m_memory_amount = 0;
};


#endif  // LIBHEIF_SECURITY_LIMITS_H
