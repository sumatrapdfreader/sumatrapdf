/*
 * HEIF codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#include "security_limits.h"
#include <limits>
#include <map>
#include <mutex>


heif_security_limits global_security_limits{
    .version = 3,

    // --- version 1

    // Artificial limit to avoid allocating too much memory.
    // 32768^2 = 1.5 GB as YUV-4:2:0 or 4 GB as RGB32
    .max_image_size_pixels = 32768 * 32768,
    .max_number_of_tiles = 4096 * 4096,
    .max_bayer_pattern_pixels = 16 * 16,
    .max_items = 1000,

    .max_color_profile_size = 100 * 1024 * 1024, // 100 MB
    .max_memory_block_size = UINT64_C(4) * 1024 * 1024 * 1024,  // 4 GB

    .max_components = 256,
    .max_iloc_extents_per_item = 32,
    .max_size_entity_group = 64,

    .max_children_per_box = 100,

    // --- version 2

    .max_total_memory = UINT64_C(4) * 1024 * 1024 * 1024,  // 4 GB
    .max_sample_description_box_entries = 1024,
    .max_sample_group_description_box_entries = 1024,

    // --- version 3

    .max_sequence_frames = 18'000'000,  // 100 hours at 50 fps
    .max_number_of_file_brands = 1000
};


heif_security_limits disabled_security_limits{
    .version = 3
};


Error check_for_valid_image_size(const heif_security_limits* limits, uint32_t width, uint32_t height)
{
  uint64_t maximum_image_size_limit = limits->max_image_size_pixels;

  // --- check whether the image size is "too large"

  if (maximum_image_size_limit > 0) {
    auto max_width_height = static_cast<uint32_t>(std::numeric_limits<int>::max());
    if ((width > max_width_height || height > max_width_height) ||
        (height != 0 && width > maximum_image_size_limit / height)) {
      std::stringstream sstr;
      sstr << "Image size " << width << "x" << height << " exceeds the maximum image size "
           << maximum_image_size_limit << "\n";

      return {heif_error_Memory_allocation_error,
              heif_suberror_Security_limit_exceeded,
              sstr.str()};
    }
  }

  if (width == 0 || height == 0) {
    return {heif_error_Memory_allocation_error,
            heif_suberror_Invalid_image_size,
            "zero width or height"};
  }

  return Error::Ok;
}


struct memory_stats {
  size_t total_memory_usage = 0;
  size_t max_memory_usage = 0;
};

std::mutex& get_memory_usage_mutex()
{
  static std::mutex sMutex;
  return sMutex;
}

static std::map<const heif_security_limits*, memory_stats> sMemoryUsage;

TotalMemoryTracker::TotalMemoryTracker(const heif_security_limits* limits)
{
  std::lock_guard<std::mutex> lock(get_memory_usage_mutex());

  sMemoryUsage[limits] = {};
  m_limits_context = limits;
}

TotalMemoryTracker::~TotalMemoryTracker()
{
  std::lock_guard<std::mutex> lock(get_memory_usage_mutex());
  sMemoryUsage.erase(m_limits_context);
}


size_t TotalMemoryTracker::get_max_total_memory_used() const
{
  std::lock_guard<std::mutex> lock(get_memory_usage_mutex());

  auto it = sMemoryUsage.find(m_limits_context);
  if (it != sMemoryUsage.end()) {
    return it->second.max_memory_usage;
  }
  else {
    assert(false);
    return 0;
  }
}


Error MemoryHandle::alloc(size_t memory_amount, const heif_security_limits* limits_context,
                          const char* reason_description)
{
  // we allow several allocations on the same handle, but they have to be for the same context
  if (m_limits_context) {
    assert(m_limits_context == limits_context);
  }


  // --- check whether limits are exceeded

  if (!limits_context) {
    return Error::Ok;
  }

  // check against maximum memory block size

  if (limits_context->max_memory_block_size != 0 &&
      memory_amount > limits_context->max_memory_block_size) {
    std::stringstream sstr;

    if (reason_description) {
      sstr << "Allocating " << memory_amount << " bytes for " << reason_description <<" exceeds the security limit of "
           << limits_context->max_memory_block_size << " bytes";
    }
    else {
      sstr << "Allocating " << memory_amount << " bytes exceeds the security limit of "
           << limits_context->max_memory_block_size << " bytes";
    }

    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};
  }

  if (limits_context == &global_security_limits ||
      limits_context == &disabled_security_limits) {
    return Error::Ok;
  }

  std::lock_guard<std::mutex> lock(get_memory_usage_mutex());
  auto it = sMemoryUsage.find(limits_context);
  if (it == sMemoryUsage.end()) {
    assert(false);
    return Error::Ok;
  }

  // check against maximum total memory usage

  if (limits_context->max_total_memory != 0 &&
      it->second.total_memory_usage + memory_amount > limits_context->max_total_memory) {
    std::stringstream sstr;

    if (reason_description) {
      sstr << "Memory usage of " << it->second.total_memory_usage + memory_amount
           << " bytes for " << reason_description << " exceeds the security limit of "
           << limits_context->max_total_memory << " bytes of total memory usage";
    }
    else {
      sstr << "Memory usage of " << it->second.total_memory_usage + memory_amount
           << " bytes exceeds the security limit of "
           << limits_context->max_total_memory << " bytes of total memory usage";
    }

    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};
  }


  // --- register memory usage

  m_limits_context = limits_context;
  m_memory_amount += memory_amount;

  it->second.total_memory_usage += memory_amount;

  // remember maximum memory usage (for informational purpose)
  if (it->second.total_memory_usage > it->second.max_memory_usage) {
    it->second.max_memory_usage = it->second.total_memory_usage;
  }

  return Error::Ok;
}


void MemoryHandle::free()
{
  if (m_limits_context) {
    std::lock_guard<std::mutex> lock(get_memory_usage_mutex());

    auto it = sMemoryUsage.find(m_limits_context);
    if (it != sMemoryUsage.end()) {
      it->second.total_memory_usage -= m_memory_amount;
    }

    m_limits_context = nullptr;
    m_memory_amount = 0;
  }
}


void MemoryHandle::free(size_t memory_amount)
{
  if (m_limits_context) {
    std::lock_guard<std::mutex> lock(get_memory_usage_mutex());

    auto it = sMemoryUsage.find(m_limits_context);
    if (it != sMemoryUsage.end()) {
      it->second.total_memory_usage -= memory_amount;
    }

    m_memory_amount -= memory_amount;
  }
}

