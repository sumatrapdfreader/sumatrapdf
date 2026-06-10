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

#ifndef LIBHEIF_HEIF_EXPERIMENTAL_H
#define LIBHEIF_HEIF_EXPERIMENTAL_H

#include "libheif/heif.h"

#ifdef __cplusplus
extern "C" {
#endif

#if HEIF_ENABLE_EXPERIMENTAL_FEATURES

/* ===================================================================================
 *   This file contains candidate APIs that did not make it into the public API yet.
 * ===================================================================================
 */


/*
heif_item_property_type_camera_intrinsic_matrix = heif_fourcc('c', 'm', 'i', 'n'),
heif_item_property_type_camera_extrinsic_matrix = heif_fourcc('c', 'm', 'e', 'x')
*/

typedef struct heif_property_camera_intrinsic_matrix heif_property_camera_intrinsic_matrix;
typedef struct heif_property_camera_extrinsic_matrix heif_property_camera_extrinsic_matrix;

//LIBHEIF_API
heif_error heif_item_get_property_camera_intrinsic_matrix(const heif_context* context,
                                                          heif_item_id itemId,
                                                          heif_property_id propertyId,
                                                          heif_property_camera_intrinsic_matrix** out_matrix);

//LIBHEIF_API
void heif_property_camera_intrinsic_matrix_release(heif_property_camera_intrinsic_matrix* matrix);

//LIBHEIF_API
heif_error heif_property_camera_intrinsic_matrix_get_focal_length(const heif_property_camera_intrinsic_matrix* matrix,
                                                                  int image_width, int image_height,
                                                                  double* out_focal_length_x,
                                                                  double* out_focal_length_y);

//LIBHEIF_API
heif_error heif_property_camera_intrinsic_matrix_get_principal_point(const heif_property_camera_intrinsic_matrix* matrix,
                                                                     int image_width, int image_height,
                                                                     double* out_principal_point_x,
                                                                     double* out_principal_point_y);

//LIBHEIF_API
heif_error heif_property_camera_intrinsic_matrix_get_skew(const heif_property_camera_intrinsic_matrix* matrix,
                                                          double* out_skew);

//LIBHEIF_API
heif_property_camera_intrinsic_matrix* heif_property_camera_intrinsic_matrix_alloc(void);

//LIBHEIF_API
void heif_property_camera_intrinsic_matrix_set_simple(heif_property_camera_intrinsic_matrix* matrix,
                                                      int image_width, int image_height,
                                                      double focal_length, double principal_point_x, double principal_point_y);

//LIBHEIF_API
void heif_property_camera_intrinsic_matrix_set_full(heif_property_camera_intrinsic_matrix* matrix,
                                                    int image_width, int image_height,
                                                    double focal_length_x,
                                                    double focal_length_y,
                                                    double principal_point_x, double principal_point_y,
                                                    double skew);

//LIBHEIF_API
heif_error heif_item_add_property_camera_intrinsic_matrix(const heif_context* context,
                                                          heif_item_id itemId,
                                                          const heif_property_camera_intrinsic_matrix* matrix,
                                                          heif_property_id* out_propertyId);


//LIBHEIF_API
heif_error heif_item_get_property_camera_extrinsic_matrix(const heif_context* context,
                                                          heif_item_id itemId,
                                                          heif_property_id propertyId,
                                                          heif_property_camera_extrinsic_matrix** out_matrix);

//LIBHEIF_API
void heif_property_camera_extrinsic_matrix_release(heif_property_camera_extrinsic_matrix* matrix);

// `out_matrix` must point to a 9-element matrix, which will be filled in row-major order.
//LIBHEIF_API
heif_error heif_property_camera_extrinsic_matrix_get_rotation_matrix(const heif_property_camera_extrinsic_matrix* matrix,
                                                                     double* out_matrix);

// `out_vector` must point to a 3-element vector, which will be filled with the (X,Y,Z) coordinates (in micrometers).
//LIBHEIF_API
heif_error heif_property_camera_extrinsic_matrix_get_position_vector(const heif_property_camera_extrinsic_matrix* matrix,
                                                                     int32_t* out_vector);

//LIBHEIF_API
heif_error heif_property_camera_extrinsic_matrix_get_world_coordinate_system_id(const heif_property_camera_extrinsic_matrix* matrix,
                                                                                uint32_t* out_wcs_id);
#endif

// --- Tiled images

typedef struct heif_tiled_image_parameters
{
  int version;

  // --- version 1

  uint32_t image_width;
  uint32_t image_height;

  uint32_t tile_width;
  uint32_t tile_height;

  uint32_t compression_format_fourcc;  // will be set automatically when calling heif_context_add_tiled_image()

  uint8_t offset_field_length;   // one of: 32, 40, 48, 64
  uint8_t size_field_length;     // one of:  0, 24, 32, 64

  uint8_t number_of_extra_dimensions;  // 0 for normal images, 1 for volumetric (3D), ...
  uint32_t extra_dimensions[8];        // size of extra dimensions (first 8 dimensions)

  // boolean flags
  uint8_t tiles_are_sequential;  // TODO: can we derive this automatically
} heif_tiled_image_parameters;

#if HEIF_ENABLE_EXPERIMENTAL_FEATURES
LIBHEIF_API
heif_error heif_context_add_tiled_image(heif_context* ctx,
                                        const heif_tiled_image_parameters* parameters,
                                        const heif_encoding_options* options, // TODO: do we need this?
                                        const heif_encoder* encoder,
                                        heif_image_handle** out_tiled_image_handle);
#endif

// --- 'pymd' entity group (pyramid layers)

typedef struct heif_pyramid_layer_info
{
  heif_item_id layer_image_id;
  uint16_t layer_binning;
  uint32_t tile_rows_in_layer;
  uint32_t tile_columns_in_layer;
} heif_pyramid_layer_info;

#if HEIF_ENABLE_EXPERIMENTAL_FEATURES
// The input images are automatically sorted according to resolution. You can provide them in any order.
LIBHEIF_API
heif_error heif_context_add_pyramid_entity_group(heif_context* ctx,
                                                 const heif_item_id* layer_item_ids,
                                                 size_t num_layers,
                                                 heif_item_id* out_group_id);

LIBHEIF_API
heif_pyramid_layer_info* heif_context_get_pyramid_entity_group_info(heif_context*,
                                                                    heif_entity_group_id id,
                                                                    int* out_num_layers);

LIBHEIF_API
void heif_pyramid_layer_info_release(heif_pyramid_layer_info*);
#endif


#ifdef __cplusplus
}
#endif

#endif
