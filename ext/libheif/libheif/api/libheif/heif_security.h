/*
 * HEIF codec.
 * Copyright (c) 2017-2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_SECURITY_H
#define LIBHEIF_HEIF_SECURITY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <libheif/heif_library.h>


// --- security limits

// If you set a limit to 0, the limit is disabled.
typedef struct heif_security_limits
{
  uint8_t version;

  // --- version 1

  // Limit on the maximum image size to avoid allocating too much memory.
  // For example, setting this to 32768^2 pixels = 1 Gigapixels results
  // in 1.5 GB memory need for YUV-4:2:0 or 4 GB for RGB32.
  uint64_t max_image_size_pixels;
  uint64_t max_number_of_tiles;
  uint32_t max_bayer_pattern_pixels;
  uint32_t max_items;

  uint32_t max_color_profile_size;
  uint64_t max_memory_block_size;

  uint32_t max_components;

  uint32_t max_iloc_extents_per_item;
  uint32_t max_size_entity_group;

  uint32_t max_children_per_box; // for all boxes that are not covered by other limits

  // --- version 2

  uint64_t max_total_memory;
  uint32_t max_sample_description_box_entries;
  uint32_t max_sample_group_description_box_entries;

  // --- version 3

  uint32_t max_sequence_frames;
  uint32_t max_number_of_file_brands;
} heif_security_limits;


// The global security limits are the default for new heif_contexts.
// These global limits cannot be changed, but you can override the limits for a specific heif_context.
LIBHEIF_API
const heif_security_limits* heif_get_global_security_limits(void);

// Returns a set of fully disabled security limits. Use with care and only after user confirmation.
LIBHEIF_API
const heif_security_limits* heif_get_disabled_security_limits(void);

// Returns the security limits for a heif_context.
// By default, the limits are set to the global limits, but you can change them in the returned object.
LIBHEIF_API
heif_security_limits* heif_context_get_security_limits(const heif_context*);

// Overwrites the security limits of a heif_context.
// This is a convenience function to easily copy limits.
LIBHEIF_API
heif_error heif_context_set_security_limits(heif_context*, const heif_security_limits*);


// --- DEPRECATED ---

// Set the maximum image size security limit. This function will set the maximum image area (number of pixels)
// to maximum_width ^ 2. Alternatively to using this function, you can also set the maximum image area
// in the security limits structure returned by heif_context_get_security_limits().
LIBHEIF_API
void heif_context_set_maximum_image_size_limit(heif_context* ctx, int maximum_width);


#ifdef __cplusplus
}
#endif

#endif
