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

#ifndef LIBHEIF_HEIF_AUX_IMAGES_H
#define LIBHEIF_HEIF_AUX_IMAGES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_library.h>
#include <libheif/heif_image.h>
#include <libheif/heif_error.h>

typedef struct heif_encoder heif_encoder;
typedef struct heif_encoding_options heif_encoding_options;


// ------------------------- depth images -------------------------

LIBHEIF_API
int heif_image_handle_has_depth_image(const heif_image_handle*);

LIBHEIF_API
int heif_image_handle_get_number_of_depth_images(const heif_image_handle* handle);

LIBHEIF_API
int heif_image_handle_get_list_of_depth_image_IDs(const heif_image_handle* handle,
                                                  heif_item_id* ids, int count);

LIBHEIF_API
heif_error heif_image_handle_get_depth_image_handle(const heif_image_handle* handle,
                                                    heif_item_id depth_image_id,
                                                    heif_image_handle** out_depth_handle);


enum heif_depth_representation_type {
  heif_depth_representation_type_uniform_inverse_Z = 0,
  heif_depth_representation_type_uniform_disparity = 1,
  heif_depth_representation_type_uniform_Z = 2,
  heif_depth_representation_type_nonuniform_disparity = 3
};

typedef struct heif_depth_representation_info {
  uint8_t version;

  // version 1 fields

  uint8_t has_z_near;
  uint8_t has_z_far;
  uint8_t has_d_min;
  uint8_t has_d_max;

  double z_near;
  double z_far;
  double d_min;
  double d_max;

  enum heif_depth_representation_type depth_representation_type;
  uint32_t disparity_reference_view;

  uint32_t depth_nonlinear_representation_model_size;
  uint8_t* depth_nonlinear_representation_model;

  // version 2 fields below
} heif_depth_representation_info;


LIBHEIF_API
void heif_depth_representation_info_free(const heif_depth_representation_info* info);

// Returns true when there is depth_representation_info available
// Note 1: depth_image_id is currently unused because we support only one depth channel per image, but
// you should still provide the correct ID for future compatibility.
// Note 2: Because of an API bug before v1.11.0, the function also works when 'handle' is the handle of the depth image.
// However, you should pass the handle of the main image. Please adapt your code if needed.
LIBHEIF_API
int heif_image_handle_get_depth_image_representation_info(const heif_image_handle* handle,
                                                          heif_item_id depth_image_id,
                                                          const heif_depth_representation_info** out);



// ------------------------- thumbnails -------------------------

// List the number of thumbnails assigned to this image handle. Usually 0 or 1.
LIBHEIF_API
int heif_image_handle_get_number_of_thumbnails(const heif_image_handle* handle);

LIBHEIF_API
int heif_image_handle_get_list_of_thumbnail_IDs(const heif_image_handle* handle,
                                                heif_item_id* ids, int count);

// Get the image handle of a thumbnail image.
LIBHEIF_API
heif_error heif_image_handle_get_thumbnail(const heif_image_handle* main_image_handle,
                                           heif_item_id thumbnail_id,
                                           heif_image_handle** out_thumbnail_handle);


// Encode the 'image' as a scaled down thumbnail image.
// The image is scaled down to fit into a square area of width 'bbox_size'.
// If the input image is already so small that it fits into this bounding box, no thumbnail
// image is encoded and NULL is returned in 'out_thumb_image_handle'.
// No error is returned in this case.
// The encoded thumbnail is automatically assigned to the 'master_image_handle'. Hence, you
// do not have to call heif_context_assign_thumbnail().
LIBHEIF_API
heif_error heif_context_encode_thumbnail(heif_context*,
                                         const heif_image* image,
                                         const heif_image_handle* master_image_handle,
                                         heif_encoder* encoder,
                                         const heif_encoding_options* options,
                                         int bbox_size,
                                         heif_image_handle** out_thumb_image_handle);

// Assign 'thumbnail_image' as the thumbnail image of 'master_image'.
LIBHEIF_API
heif_error heif_context_assign_thumbnail(struct heif_context*,
                                         const heif_image_handle* master_image,
                                         const heif_image_handle* thumbnail_image);


// ------------------------- auxiliary images -------------------------

#define LIBHEIF_AUX_IMAGE_FILTER_OMIT_ALPHA (1UL<<1)
#define LIBHEIF_AUX_IMAGE_FILTER_OMIT_DEPTH (2UL<<1)

// List the number of auxiliary images assigned to this image handle.
LIBHEIF_API
int heif_image_handle_get_number_of_auxiliary_images(const heif_image_handle* handle,
                                                     int aux_filter);

LIBHEIF_API
int heif_image_handle_get_list_of_auxiliary_image_IDs(const heif_image_handle* handle,
                                                      int aux_filter,
                                                      heif_item_id* ids, int count);

// You are responsible to deallocate the returned buffer with heif_image_handle_release_auxiliary_type().
LIBHEIF_API
heif_error heif_image_handle_get_auxiliary_type(const heif_image_handle* handle,
                                                const char** out_type);

LIBHEIF_API
void heif_image_handle_release_auxiliary_type(const heif_image_handle* handle,
                                              const char** out_type);

// Get the image handle of an auxiliary image.
LIBHEIF_API
heif_error heif_image_handle_get_auxiliary_image_handle(const heif_image_handle* main_image_handle,
                                                        heif_item_id auxiliary_id,
                                                        heif_image_handle** out_auxiliary_handle);

// ===================== DEPRECATED =====================

// DEPRECATED (because typo in function name). Use heif_image_handle_release_auxiliary_type() instead.
LIBHEIF_API
void heif_image_handle_free_auxiliary_types(const heif_image_handle* handle,
                                            const char** out_type);

#ifdef __cplusplus
}
#endif

#endif
