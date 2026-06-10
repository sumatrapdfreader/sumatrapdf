/*
 * HEIF codec.
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_COMPONENTS_H
#define LIBHEIF_HEIF_COMPONENTS_H

#include "libheif/heif_library.h"
#include "libheif/heif_image.h"
#include "libheif/heif_image_handle.h"

#ifdef __cplusplus
extern "C" {
#endif

/* @file heif_components.h
 * @brief Multi-component image API.
 *
 * The component model (id-addressed pixel planes with per-component type,
 * datatype and bit-depth) is codec-agnostic. It was introduced for ISO 23001-17
 * (uncompressed) images but is also expected to be reused by other codecs that
 * carry multiple typed components (e.g. JPEG-2000).
 */


// --- pixel datatype support
//
// The numeric values are aligned with the ISO/IEC 23001-17 Table 2
// component_format byte (used by the uncC box of the uncompressed codec).
// This is an internal convenience and should not be relied upon.

typedef enum heif_component_datatype
{
  heif_component_datatype_unsigned_integer = 0,
  heif_component_datatype_floating_point   = 1,
  heif_component_datatype_complex_number   = 2,
  heif_component_datatype_signed_integer   = 3,
  heif_component_datatype_undefined        = 0xFF
} heif_component_datatype;

typedef struct heif_complex32
{
  float real, imaginary;
} heif_complex32;

typedef struct heif_complex64
{
  double real, imaginary;
} heif_complex64;


// --- component types (ISO/IEC 23001-17 Table 1, used in the cmpd box)

typedef enum heif_cmpd_component_type
{
  heif_cmpd_component_type_monochrome = 0,
  heif_cmpd_component_type_Y = 1,
  heif_cmpd_component_type_Cb = 2,
  heif_cmpd_component_type_Cr = 3,
  heif_cmpd_component_type_red = 4,
  heif_cmpd_component_type_green = 5,
  heif_cmpd_component_type_blue = 6,
  heif_cmpd_component_type_alpha = 7,
  heif_cmpd_component_type_depth = 8,
  heif_cmpd_component_type_disparity = 9,
  heif_cmpd_component_type_palette = 10,
  heif_cmpd_component_type_filter_array = 11,
  heif_cmpd_component_type_padded = 12,
  heif_cmpd_component_type_cyan = 13,
  heif_cmpd_component_type_magenta = 14,
  heif_cmpd_component_type_yellow = 15,
  heif_cmpd_component_type_key_black = 16
} heif_cmpd_component_type;


// --- ID-based component access (operates on a decoded heif_image)

// Returns the number of components that have pixel data (planes) in this image.
LIBHEIF_API
uint32_t heif_image_get_number_of_used_components(const heif_image*);

// Fills `out_component_ids` with the valid component IDs.
// The caller must allocate an array of at least heif_image_get_number_of_used_components() elements.
LIBHEIF_API
void heif_image_get_used_component_ids(const heif_image*, uint32_t* out_component_ids);

LIBHEIF_API
heif_channel heif_image_get_component_channel(const heif_image*, uint32_t component_id);

LIBHEIF_API
uint32_t heif_image_get_component_width(const heif_image*, uint32_t component_id);

LIBHEIF_API
uint32_t heif_image_get_component_height(const heif_image*, uint32_t component_id);

LIBHEIF_API
int heif_image_get_component_bits_per_pixel(const heif_image*, uint32_t component_id);

LIBHEIF_API
uint16_t heif_image_get_component_type(const heif_image*, uint32_t component_id);

// Returns the datatype (unsigned/signed integer, floating point, or complex
// number) of the given component.
LIBHEIF_API
heif_component_datatype heif_image_get_component_datatype(const heif_image*, uint32_t component_id);


// --- ID-based component access via heif_image_handle (before decoding)
//
// These let the caller introspect a multi-component image's components without
// decoding any tile. They return 0 / -1 / heif_component_datatype_undefined for
// images that do not expose a component model or for unknown component IDs.
//
// The component IDs returned here match the IDs that
// heif_image_get_used_component_ids() will report after the same image is
// decoded, so the same numerical id can be used to address a component on
// either side of the API.

LIBHEIF_API
uint32_t heif_image_handle_get_number_of_components(const heif_image_handle*);

// Fills `out_component_ids` with the valid component IDs.
// The caller must allocate an array of at least
// heif_image_handle_get_number_of_components() elements.
LIBHEIF_API
void heif_image_handle_get_used_component_ids(const heif_image_handle*, uint32_t* out_component_ids);

LIBHEIF_API
uint16_t heif_image_handle_get_component_type(const heif_image_handle*, uint32_t component_id);

LIBHEIF_API
int heif_image_handle_get_component_bits_per_pixel(const heif_image_handle*, uint32_t component_id);

LIBHEIF_API
heif_component_datatype heif_image_handle_get_component_datatype(const heif_image_handle*, uint32_t component_id);


// --- adding components to a heif_image (encoder path)

LIBHEIF_API
heif_error heif_image_add_component(heif_image* image,
                                    int width, int height,
                                    uint16_t component_type,
                                    heif_component_datatype datatype,
                                    int bit_depth,
                                    uint32_t* out_component_id);


// --- untyped uint8 component data getters: stride is in BYTES per row.

LIBHEIF_API
const uint8_t* heif_image_get_component_readonly(const heif_image*, uint32_t component_id, size_t* out_stride);

LIBHEIF_API
uint8_t* heif_image_get_component(heif_image*, uint32_t component_id, size_t* out_stride);


// --- typed component data getters: `out_row_elements` is the number of T
// elements per row, not bytes. Index with `data[y * row_elements + x]` (no
// casts, no sizeof). libheif allocates rows with element-aligned padding, so
// this count is always exact for the named type T.

LIBHEIF_API
const uint16_t* heif_image_get_component_uint16_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
uint16_t* heif_image_get_component_uint16(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const uint32_t* heif_image_get_component_uint32_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
uint32_t* heif_image_get_component_uint32(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const uint64_t* heif_image_get_component_uint64_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
uint64_t* heif_image_get_component_uint64(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const int8_t* heif_image_get_component_int8_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
int8_t* heif_image_get_component_int8(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const int16_t* heif_image_get_component_int16_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
int16_t* heif_image_get_component_int16(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const int32_t* heif_image_get_component_int32_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
int32_t* heif_image_get_component_int32(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const int64_t* heif_image_get_component_int64_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
int64_t* heif_image_get_component_int64(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const float* heif_image_get_component_float32_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
float* heif_image_get_component_float32(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const double* heif_image_get_component_float64_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
double* heif_image_get_component_float64(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const heif_complex32* heif_image_get_component_complex32_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
heif_complex32* heif_image_get_component_complex32(heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
const heif_complex64* heif_image_get_component_complex64_readonly(const heif_image*, uint32_t component_id, size_t* out_row_elements);

LIBHEIF_API
heif_complex64* heif_image_get_component_complex64(heif_image*, uint32_t component_id, size_t* out_row_elements);


// --- GIMI component content IDs (set before encoding)

// Set a GIMI component content ID for the component with the given
// component_id (as minted by heif_image_add_component / returned via the
// component access API). Pass an empty string to clear a previously set id.
// Returns an error if no component with this id exists on the image.
// The collected ids are written into an ItemComponentContentIDProperty box
// during encoding.
LIBHEIF_API
heif_error heif_image_set_gimi_component_content_id(heif_image*,
                                                    uint32_t component_id,
                                                    const char* content_id);


#ifdef __cplusplus
}
#endif

#endif
