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

#include "heif_experimental.h"
#include "context.h"
#include "api_structs.h"
#include "image-items/unc_image.h"
#include "image-items/tiled.h"

#include <array>
#include <cstring>
#include <memory>
#include <vector>
#include <limits>


struct heif_property_camera_intrinsic_matrix
{
  Box_cmin::RelativeIntrinsicMatrix matrix;
};

heif_error heif_item_get_property_camera_intrinsic_matrix(const heif_context* context,
                                                          heif_item_id itemId,
                                                          heif_property_id propertyId,
                                                          heif_property_camera_intrinsic_matrix** out_matrix)
{
  if (!out_matrix || !context) {
    return heif_error_null_pointer_argument;
  }

  auto cmin = context->context->find_property<Box_cmin>(itemId, propertyId);
  if (!cmin) {
    return cmin.error_struct(context->context.get());
  }

  *out_matrix = new heif_property_camera_intrinsic_matrix;
  (*out_matrix)->matrix = (*cmin)->get_intrinsic_matrix();

  return heif_error_success;
}


void heif_property_camera_intrinsic_matrix_release(heif_property_camera_intrinsic_matrix* matrix)
{
  delete matrix;
}

heif_error heif_property_camera_intrinsic_matrix_get_focal_length(const heif_property_camera_intrinsic_matrix* matrix,
                                                                  int image_width, int image_height,
                                                                  double* out_focal_length_x,
                                                                  double* out_focal_length_y)
{
  if (!matrix) {
    return heif_error_null_pointer_argument;
  }

  double fx, fy;
  matrix->matrix.compute_focal_length(image_width, image_height, fx, fy);

  if (out_focal_length_x) *out_focal_length_x = fx;
  if (out_focal_length_y) *out_focal_length_y = fy;

  return heif_error_success;
}


heif_error heif_property_camera_intrinsic_matrix_get_principal_point(const heif_property_camera_intrinsic_matrix* matrix,
                                                                     int image_width, int image_height,
                                                                     double* out_principal_point_x,
                                                                     double* out_principal_point_y)
{
  if (!matrix) {
    return heif_error_null_pointer_argument;
  }

  double px, py;
  matrix->matrix.compute_principal_point(image_width, image_height, px, py);

  if (out_principal_point_x) *out_principal_point_x = px;
  if (out_principal_point_y) *out_principal_point_y = py;

  return heif_error_success;
}


heif_error heif_property_camera_intrinsic_matrix_get_skew(const heif_property_camera_intrinsic_matrix* matrix,
                                                          double* out_skew)
{
  if (!matrix || !out_skew) {
    return heif_error_null_pointer_argument;
  }

  *out_skew = matrix->matrix.skew;

  return heif_error_success;
}


heif_property_camera_intrinsic_matrix* heif_property_camera_intrinsic_matrix_alloc()
{
  return new heif_property_camera_intrinsic_matrix;
}

void heif_property_camera_intrinsic_matrix_set_simple(heif_property_camera_intrinsic_matrix* matrix,
                                                      int image_width, int image_height,
                                                      double focal_length, double principal_point_x, double principal_point_y)
{
  if (!matrix) {
    return;
  }

  matrix->matrix.is_anisotropic = false;
  matrix->matrix.focal_length_x = focal_length / image_width;
  matrix->matrix.principal_point_x = principal_point_x / image_width;
  matrix->matrix.principal_point_y = principal_point_y / image_height;
}

void heif_property_camera_intrinsic_matrix_set_full(heif_property_camera_intrinsic_matrix* matrix,
                                                    int image_width, int image_height,
                                                    double focal_length_x,
                                                    double focal_length_y,
                                                    double principal_point_x, double principal_point_y,
                                                    double skew)
{
  if (!matrix) {
    return;
  }

  if (focal_length_x == focal_length_y && skew == 0) {
    heif_property_camera_intrinsic_matrix_set_simple(matrix, image_width, image_height, focal_length_x, principal_point_x, principal_point_y);
    return;
  }

  matrix->matrix.is_anisotropic = true;
  matrix->matrix.focal_length_x = focal_length_x / image_width;
  matrix->matrix.focal_length_y = focal_length_y / image_width;
  matrix->matrix.principal_point_x = principal_point_x / image_width;
  matrix->matrix.principal_point_y = principal_point_y / image_height;
  matrix->matrix.skew = skew;
}


heif_error heif_item_add_property_camera_intrinsic_matrix(const heif_context* context,
                                                          heif_item_id itemId,
                                                          const heif_property_camera_intrinsic_matrix* matrix,
                                                          heif_property_id* out_propertyId)
{
  if (!context || !matrix) {
    return heif_error_null_pointer_argument;
  }

  auto cmin = std::make_shared<Box_cmin>();
  cmin->set_intrinsic_matrix(matrix->matrix);

  heif_property_id id = context->context->add_property(itemId, cmin, false);

  if (out_propertyId) {
    *out_propertyId = id;
  }

  return heif_error_success;
}


struct heif_property_camera_extrinsic_matrix
{
  Box_cmex::ExtrinsicMatrix matrix;
};


heif_error heif_item_get_property_camera_extrinsic_matrix(const heif_context* context,
                                                          heif_item_id itemId,
                                                          heif_property_id propertyId,
                                                          heif_property_camera_extrinsic_matrix** out_matrix)
{
  if (!out_matrix || !context) {
    return heif_error_null_pointer_argument;
  }

  auto cmex = context->context->find_property<Box_cmex>(itemId, propertyId);
  if (!cmex) {
    return cmex.error_struct(context->context.get());
  }

  *out_matrix = new heif_property_camera_extrinsic_matrix;
  (*out_matrix)->matrix = (*cmex)->get_extrinsic_matrix();

  return heif_error_success;
}


void heif_property_camera_extrinsic_matrix_release(heif_property_camera_extrinsic_matrix* matrix)
{
  delete matrix;
}


heif_error heif_property_camera_extrinsic_matrix_get_rotation_matrix(const heif_property_camera_extrinsic_matrix* matrix,
                                                                     double* out_matrix)
{
  if (!matrix || !out_matrix) {
    return heif_error_null_pointer_argument;
  }

  auto rot_matrix = matrix->matrix.calculate_rotation_matrix();
  for (int i = 0; i < 9; i++) {
    out_matrix[i] = rot_matrix[i];
  }

  return heif_error_success;
}


heif_error heif_property_camera_extrinsic_matrix_get_position_vector(const heif_property_camera_extrinsic_matrix* matrix,
                                                                     int32_t* out_vector)
{
  if (!matrix || !out_vector) {
    return heif_error_null_pointer_argument;
  }

  out_vector[0] = matrix->matrix.pos_x;
  out_vector[1] = matrix->matrix.pos_y;
  out_vector[2] = matrix->matrix.pos_z;

  return heif_error_success;
}


heif_error heif_property_camera_extrinsic_matrix_get_world_coordinate_system_id(const heif_property_camera_extrinsic_matrix* matrix,
                                                                                uint32_t* out_wcs_id)
{
  if (!matrix || !out_wcs_id) {
    return heif_error_null_pointer_argument;
  }

  *out_wcs_id = matrix->matrix.world_coordinate_system_id;

  return heif_error_success;
}


#if HEIF_ENABLE_EXPERIMENTAL_FEATURES

heif_error heif_context_add_pyramid_entity_group(struct heif_context* ctx,
                                                 const heif_item_id* layer_item_ids,
                                                 size_t num_layers,
                                                 /*
                                                 uint16_t tile_width,
                                                 uint16_t tile_height,
                                                 uint32_t num_layers,
                                                 const heif_pyramid_layer_info* in_layers,
                                                  */
                                                 heif_item_id* out_group_id)
{
  if (!layer_item_ids) {
    return heif_error_null_pointer_argument;
  }

  if (num_layers == 0) {
    return {heif_error_Usage_error, heif_suberror_Invalid_parameter_value, "Number of layers cannot be 0."};
  }

  std::vector<heif_item_id> layers(num_layers);
  for (size_t i = 0; i < num_layers; i++) {
    layers[i] = layer_item_ids[i];
  }

  Result<heif_item_id> result = ctx->context->add_pyramid_group(layers);

  if (result) {
    if (out_group_id) {
      *out_group_id = result;
    }
    return heif_error_success;
  }
  else {
    return result.error_struct(ctx->context.get());
  }
}


heif_pyramid_layer_info* heif_context_get_pyramid_entity_group_info(heif_context* ctx, heif_entity_group_id id, int* out_num_layers)
{
  if (!out_num_layers) {
    return nullptr;
  }

  std::shared_ptr<Box_EntityToGroup> groupBox = ctx->context->get_heif_file()->get_entity_group(id);
  if (!groupBox) {
    return nullptr;
  }

  const auto pymdBox = std::dynamic_pointer_cast<Box_pymd>(groupBox);
  if (!pymdBox) {
    return nullptr;
  }

  const std::vector<Box_pymd::LayerInfo> pymd_layers = pymdBox->get_layers();
  if (pymd_layers.empty()) {
    return nullptr;
  }

  auto items = pymdBox->get_item_ids();
  assert(items.size() == pymd_layers.size());

  auto* layerInfo = new heif_pyramid_layer_info[pymd_layers.size()];
  for (size_t i = 0; i < pymd_layers.size(); i++) {
    layerInfo[i].layer_image_id = items[i];
    layerInfo[i].layer_binning = pymd_layers[i].layer_binning;
    layerInfo[i].tile_rows_in_layer = pymd_layers[i].tiles_in_layer_row_minus1 + 1;
    layerInfo[i].tile_columns_in_layer = pymd_layers[i].tiles_in_layer_column_minus1 + 1;
  }

  *out_num_layers = static_cast<int>(pymd_layers.size());

  return layerInfo;
}


void heif_pyramid_layer_info_release(heif_pyramid_layer_info* infos)
{
  delete[] infos;
}


#endif


#if HEIF_ENABLE_EXPERIMENTAL_FEATURES
heif_error heif_context_add_tiled_image(heif_context* ctx,
                                        const heif_tiled_image_parameters* parameters,
                                        const heif_encoding_options* options,
                                        const heif_encoder* encoder,
                                        heif_image_handle** out_grid_image_handle)
{
  if (out_grid_image_handle) {
    *out_grid_image_handle = nullptr;
  }

  Result<std::shared_ptr<ImageItem_Tiled> > gridImageResult;
  gridImageResult = ImageItem_Tiled::add_new_tiled_item(ctx->context.get(), parameters, encoder, options);

  if (!gridImageResult) {
    return gridImageResult.error_struct(ctx->context.get());
  }

  if (out_grid_image_handle) {
    *out_grid_image_handle = new heif_image_handle;
    (*out_grid_image_handle)->image = *gridImageResult;
    (*out_grid_image_handle)->context = ctx->context;
  }

  return heif_error_success;
}
#endif
