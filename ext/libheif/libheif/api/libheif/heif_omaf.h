/*
 * HEIF codec.
 * Copyright (c) 2026 Brad Hards <bradh@frogmouth.net>
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

#ifndef LIBHEIF_HEIF_OMAF_H
#define LIBHEIF_HEIF_OMAF_H

#include <libheif/heif_library.h>
#include <libheif/heif_error.h>
#include <libheif/heif_image.h>
#include <libheif/heif_image_handle.h>

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------- OMAF projection information -------------------------
//
// ISO/IEC 23090-2 "Omnidirectional media format" describes formats for
// representing immersive / 360-degree imagery. The 'prfr' (projection format)
// item property records the kind of projection used by the encoded image.

/**
 * OMAF Image projection.
 *
 * The image projection for most images is flat - it is projected as intended to be shown on
 * a flat screen or print. For immersive or omnidirectional media (e.g. VR headsets, or
 * equivalent), there are alternatives such as an equirectangular projection or cubemap projection.
 *
 * See ISO/IEC 23090-2 "Omnidirectional media format" for more information.
 */
typedef enum heif_omaf_image_projection
{
  /**
   * Equirectangular projection.
   */
  heif_omaf_image_projection_equirectangular = 0x00,

  /**
   * Cube map.
   */
  heif_omaf_image_projection_cube_map = 0x01,

  /*
   * Values 2 through 31 are reserved in ISO/IEC 23090-2:2023 Table 10.
   * Files may carry any of them; libheif passes the raw projection_type
   * value through unchanged, so callers can log or round-trip it.
   * Handle anything outside the named constants in a `default:` arm.
   */

  /**
   * Flat projection. Also returned by the get-projection accessors when no
   * projection information is present on the image, so callers can use
   * `result == heif_omaf_image_projection_flat` to test for "no prfr box".
   * 0xFF lies outside the prfr value range reserved by ISO 23090-2:2023
   * Table 10, so it cannot collide with a value read from a file.
   */
  heif_omaf_image_projection_flat = 0xFF,
} heif_omaf_image_projection;


// To test whether projection information is present, compare the getter's
// result against heif_omaf_image_projection_flat.

LIBHEIF_API
heif_omaf_image_projection heif_image_handle_get_omaf_image_projection(const heif_image_handle* handle);

LIBHEIF_API
void heif_image_handle_set_omaf_image_projection(heif_image_handle* handle,
                                                 heif_omaf_image_projection image_projection);

// Variants operating on a heif_image (a decoded or about-to-be-encoded pixel
// image). Setting the projection on a heif_image before encoding causes the
// resulting image item to carry the corresponding prfr property.

LIBHEIF_API
heif_omaf_image_projection heif_image_get_omaf_image_projection(const heif_image* image);

LIBHEIF_API
void heif_image_set_omaf_image_projection(heif_image* image,
                                          heif_omaf_image_projection image_projection);

#ifdef __cplusplus
}
#endif

#endif
