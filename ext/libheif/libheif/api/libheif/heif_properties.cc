/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#include "libheif/heif_properties.h"
#include "context.h"
#include "api_structs.h"
#include "file.h"
#include "image/pixelimage.h"

#include <array>
#include <cstring>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>

int heif_item_get_properties_of_type(const heif_context* context,
                                     heif_item_id id,
                                     heif_item_property_type type,
                                     heif_property_id* out_list,
                                     int count)
{
  auto file = context->context->get_heif_file();

  std::vector<std::shared_ptr<Box>> properties;
  Error err = file->get_properties(id, properties);
  if (err) {
    // We do not pass the error, because a missing ipco should have been detected already when reading the file.
    return 0;
  }

  int out_idx = 0;
  int property_id = 1;

  for (const auto& property : properties) {
    bool match;
    if (type == heif_item_property_type_invalid) {
      match = true;
    }
    else {
      match = (property->get_short_type() == type);
    }

    if (match) {
      if (out_list && out_idx < count) {
        out_list[out_idx] = property_id;
        out_idx++;
      }
      else if (!out_list) {
        out_idx++;
      }
    }

    property_id++;
  }

  return out_idx;
}


int heif_item_get_transformation_properties(const heif_context* context,
                                            heif_item_id id,
                                            heif_property_id* out_list,
                                            int count)
{
  auto file = context->context->get_heif_file();

  std::vector<std::shared_ptr<Box>> properties;
  Error err = file->get_properties(id, properties);
  if (err) {
    // We do not pass the error, because a missing ipco should have been detected already when reading the file.
    return 0;
  }

  int out_idx = 0;
  int property_id = 1;

  for (const auto& property : properties) {
    bool match = (property->get_short_type() == fourcc("imir") ||
                  property->get_short_type() == fourcc("irot") ||
                  property->get_short_type() == fourcc("clap"));

    if (match) {
      if (out_list && out_idx < count) {
        out_list[out_idx] = property_id;
        out_idx++;
      }
      else if (!out_list) {
        out_idx++;
      }
    }

    property_id++;
  }

  return out_idx;
}

heif_item_property_type heif_item_get_property_type(const heif_context* context,
                                                    heif_item_id id,
                                                    heif_property_id propertyId)
{
  auto file = context->context->get_heif_file();

  std::vector<std::shared_ptr<Box>> properties;
  Error err = file->get_properties(id, properties);
  if (err) {
    // We do not pass the error, because a missing ipco should have been detected already when reading the file.
    return heif_item_property_type_invalid;
  }

  if (propertyId < 1 || propertyId - 1 >= properties.size()) {
    return heif_item_property_type_invalid;
  }

  auto property = properties[propertyId - 1];
  return (heif_item_property_type) property->get_short_type();
}


static char* create_c_string_copy(const std::string s)
{
  char* copy = new char[s.length() + 1];
  strcpy(copy, s.data());
  return copy;
}


heif_error heif_item_get_property_user_description(const heif_context* context,
                                                   heif_item_id itemId,
                                                   heif_property_id propertyId,
                                                   heif_property_user_description** out)
{
  if (!out || !context) {
    return heif_error_null_pointer_argument;
  }
  auto udes = context->context->find_property<Box_udes>(itemId, propertyId);
  if (!udes) {
    return udes.error_struct(context->context.get());
  }

  auto* udes_c = new heif_property_user_description();
  udes_c->version = 1;
  udes_c->lang = create_c_string_copy((*udes)->get_lang());
  udes_c->name = create_c_string_copy((*udes)->get_name());
  udes_c->description = create_c_string_copy((*udes)->get_description());
  udes_c->tags = create_c_string_copy((*udes)->get_tags());

  *out = udes_c;

  return heif_error_success;
}


heif_error heif_item_add_property_user_description(const heif_context* context,
                                                   heif_item_id itemId,
                                                   const heif_property_user_description* description,
                                                   heif_property_id* out_propertyId)
{
  if (!context || !description) {
    return heif_error_null_pointer_argument;
  }

  auto udes = std::make_shared<Box_udes>();
  udes->set_lang(description->lang ? description->lang : "");
  udes->set_name(description->name ? description->name : "");
  udes->set_description(description->description ? description->description : "");
  udes->set_tags(description->tags ? description->tags : "");

  heif_property_id id = context->context->add_property(itemId, udes, false);

  if (out_propertyId) {
    *out_propertyId = id;
  }

  return heif_error_success;
}


void heif_property_user_description_release(heif_property_user_description* udes)
{
  if (udes == nullptr) {
    return;
  }

  delete[] udes->lang;
  delete[] udes->name;
  delete[] udes->description;
  delete[] udes->tags;

  delete udes;
}


heif_transform_mirror_direction heif_item_get_property_transform_mirror(const heif_context* context,
                                                                        heif_item_id itemId,
                                                                        heif_property_id propertyId)
{
  auto imir = context->context->find_property<Box_imir>(itemId, propertyId);
  if (!imir) {
    return heif_transform_mirror_direction_invalid;
  }

  return (*imir)->get_mirror_direction();
}


int heif_item_get_property_transform_rotation_ccw(const heif_context* context,
                                                  heif_item_id itemId,
                                                  heif_property_id propertyId)
{
  auto irot = context->context->find_property<Box_irot>(itemId, propertyId);
  if (!irot) {
    return -1;
  }

  return (*irot)->get_rotation_ccw();
}

void heif_item_get_property_transform_crop_borders(const heif_context* context,
                                                   heif_item_id itemId,
                                                   heif_property_id propertyId,
                                                   int image_width, int image_height,
                                                   int* left, int* top, int* right, int* bottom)
{
  auto clap = context->context->find_property<Box_clap>(itemId, propertyId);
  if (!clap) {
    return;
  }

  if (left) *left = (*clap)->left_rounded(image_width);
  if (right) *right = image_width - 1 - (*clap)->right_rounded(image_width);
  if (top) *top = (*clap)->top_rounded(image_height);
  if (bottom) *bottom = image_height - 1 - (*clap)->bottom_rounded(image_height);
}


heif_error heif_item_add_raw_property(const heif_context* context,
                                      heif_item_id itemId,
                                      uint32_t short_type,
                                      const uint8_t* uuid_type,
                                      const uint8_t* data, size_t size,
                                      int is_essential,
                                      heif_property_id* out_propertyId)
{
  if (!context || !data || (short_type == fourcc("uuid") && uuid_type==nullptr)) {
    return heif_error_null_pointer_argument;
  }

  auto raw_box = std::make_shared<Box_other>(short_type);

  if (short_type == fourcc("uuid")) {
    std::vector<uint8_t> uuid_type_vector(uuid_type, uuid_type + 16);
    raw_box->set_uuid_type(uuid_type_vector);
  }

  std::vector<uint8_t> data_vector(data, data + size);
  raw_box->set_raw_data(data_vector);

  heif_property_id id = context->context->add_property(itemId, raw_box, is_essential != 0);

  if (out_propertyId) {
    *out_propertyId = id;
  }

  return heif_error_success;
}


heif_error heif_item_get_property_raw_size(const heif_context* context,
                                           heif_item_id itemId,
                                           heif_property_id propertyId,
                                           size_t* size_out)
{
  if (!context || !size_out) {
    return heif_error_null_pointer_argument;
  }
  auto box_other = context->context->find_property<Box_other>(itemId, propertyId);
  if (!box_other) {
    return box_other.error_struct(context->context.get());
  }

  // TODO: every Box (not just Box_other) should have a get_raw_data() method.
  if (*box_other == nullptr) {
    return {heif_error_Usage_error, heif_suberror_Invalid_property, "this property is not read as a raw box"};
  }

  const auto& data = (*box_other)->get_raw_data();

  *size_out = data.size();

  return heif_error_success;
}


heif_error heif_item_get_property_raw_data(const heif_context* context,
                                           heif_item_id itemId,
                                           heif_property_id propertyId,
                                           uint8_t* data_out)
{
  if (!context || !data_out) {
    return heif_error_null_pointer_argument;
  }

  auto box_other = context->context->find_property<Box_other>(itemId, propertyId);
  if (!box_other) {
    return box_other.error_struct(context->context.get());
  }

  // TODO: every Box (not just Box_other) should have a get_raw_data() method.
  if (*box_other == nullptr) {
    return {heif_error_Usage_error, heif_suberror_Invalid_property, "this property is not read as a raw box"};
  }

  auto data = (*box_other)->get_raw_data();

  std::copy(data.begin(), data.end(), data_out);

  return heif_error_success;
}


heif_error heif_item_get_property_uuid_type(const heif_context* context,
                                            heif_item_id itemId,
                                            heif_property_id propertyId,
                                            uint8_t extended_type[16])
{
  if (!context || !extended_type) {
    return heif_error_null_pointer_argument;
  }

  auto box_other = context->context->find_property<Box_other>(itemId, propertyId);
  if (!box_other) {
    return box_other.error_struct(context->context.get());
  }

  if (*box_other == nullptr) {
    return {heif_error_Usage_error, heif_suberror_Invalid_property, "this property is not read as a raw box"};
  }

  auto uuid = (*box_other)->get_uuid_type();

  std::copy(uuid.begin(), uuid.end(), extended_type);

  return heif_error_success;
}



// ------------------------- intrinsic and extrinsic matrices -------------------------


int heif_image_handle_has_camera_intrinsic_matrix(const heif_image_handle* handle)
{
  if (!handle) {
    return false;
  }

  return handle->image->has_intrinsic_matrix();
}


heif_error heif_image_handle_get_camera_intrinsic_matrix(const heif_image_handle* handle,
                                                         heif_camera_intrinsic_matrix* out_matrix)
{
  if (handle == nullptr || out_matrix == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (!handle->image->has_intrinsic_matrix()) {
    Error err(heif_error_Usage_error,
              heif_suberror_Camera_intrinsic_matrix_undefined);
    return err.error_struct(handle->image.get());
  }

  const auto& m = handle->image->get_intrinsic_matrix();
  out_matrix->focal_length_x = m.focal_length_x;
  out_matrix->focal_length_y = m.focal_length_y;
  out_matrix->principal_point_x = m.principal_point_x;
  out_matrix->principal_point_y = m.principal_point_y;
  out_matrix->skew = m.skew;

  return heif_error_success;
}


int heif_image_handle_has_camera_extrinsic_matrix(const heif_image_handle* handle)
{
  if (!handle) {
    return false;
  }

  return handle->image->has_extrinsic_matrix();
}


struct heif_camera_extrinsic_matrix
{
  Box_cmex::ExtrinsicMatrix matrix;
};


heif_error heif_image_handle_get_camera_extrinsic_matrix(const heif_image_handle* handle,
                                                         heif_camera_extrinsic_matrix** out_matrix)
{
  if (handle == nullptr || out_matrix == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (!handle->image->has_extrinsic_matrix()) {
    Error err(heif_error_Usage_error,
              heif_suberror_Camera_extrinsic_matrix_undefined);
    return err.error_struct(handle->image.get());
  }

  *out_matrix = new heif_camera_extrinsic_matrix;
  (*out_matrix)->matrix = handle->image->get_extrinsic_matrix();

  return heif_error_success;
}


void heif_camera_extrinsic_matrix_release(heif_camera_extrinsic_matrix* matrix)
{
  delete matrix;
}


heif_error heif_camera_extrinsic_matrix_get_rotation_matrix(const heif_camera_extrinsic_matrix* matrix,
                                                            double* out_matrix_row_major)
{
  if (matrix == nullptr || out_matrix_row_major == nullptr) {
    return heif_error_null_pointer_argument;
  }

  auto m3x3 = matrix->matrix.calculate_rotation_matrix();

  for (int i=0;i<9;i++) {
    out_matrix_row_major[i] = m3x3[i];
  }

  return heif_error_success;
}


// ====================== ISO/IEC 23001-17 properties ======================


heif_error heif_image_set_bayer_pattern(heif_image* image,
                                        uint32_t bayer_component_id,
                                        uint16_t pattern_width,
                                        uint16_t pattern_height,
                                        const struct heif_bayer_pattern_pixel* patternPixels)
{
  if (image == nullptr || patternPixels == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (pattern_width == 0 || pattern_height == 0) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Bayer pattern dimensions must be non-zero."};
  }

  BayerPattern pattern;
  pattern.pattern_width = pattern_width;
  pattern.pattern_height = pattern_height;

  size_t num_pixels = size_t{pattern_width} * pattern_height;
  pattern.pixels.assign(patternPixels, patternPixels + num_pixels);

  image->image->set_bayer_pattern(pattern);

  return heif_error_success;
}


heif_error heif_image_add_bayer_component(heif_image* image,
                                          uint16_t component_type,
                                          uint32_t* out_component_id)
{
  if (image == nullptr || out_component_id == nullptr) {
    return heif_error_null_pointer_argument;
  }

  *out_component_id = image->image->add_component_without_data(component_type);

  return heif_error_success;
}


int heif_image_get_bayer_pattern_size(const heif_image* image,
                                      uint32_t bayer_component_id,
                                      uint16_t* out_pattern_width,
                                      uint16_t* out_pattern_height)
{
  if (image == nullptr || !image->image->has_bayer_pattern(bayer_component_id)) {
    if (out_pattern_width) {
      *out_pattern_width = 0;
    }
    if (out_pattern_height) {
      *out_pattern_height = 0;
    }
    return 0;
  }

  const BayerPattern& pattern = image->image->get_bayer_pattern(bayer_component_id);

  if (out_pattern_width) {
    *out_pattern_width = pattern.pattern_width;
  }
  if (out_pattern_height) {
    *out_pattern_height = pattern.pattern_height;
  }

  return 1;
}


heif_error heif_image_get_bayer_pattern(const heif_image* image,
                                        uint32_t bayer_component_id,
                                        struct heif_bayer_pattern_pixel* out_patternPixels)
{
  if (image == nullptr || out_patternPixels == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (!image->image->has_bayer_pattern(bayer_component_id)) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Image does not have a Bayer pattern."};
  }

  const BayerPattern& pattern = image->image->get_bayer_pattern(bayer_component_id);
  size_t num_pixels = size_t{pattern.pattern_width} * pattern.pattern_height;
  std::copy(pattern.pixels.begin(), pattern.pixels.begin() + num_pixels, out_patternPixels);

  return heif_error_success;
}


float heif_polarization_angle_no_filter()
{
  uint32_t bits = 0xFFFFFFFF;
  float f;
  memcpy(&f, &bits, sizeof(f));
  return f;
}


int heif_polarization_angle_is_no_filter(float angle)
{
  uint32_t bits;
  memcpy(&bits, &angle, sizeof(bits));
  return bits == 0xFFFFFFFF;
}


heif_error heif_image_add_polarization_pattern(heif_image* image,
                                               uint32_t num_component_ids,
                                               const uint32_t* component_ids,
                                               uint16_t pattern_width,
                                               uint16_t pattern_height,
                                               const float* polarization_angles)
{
  if (image == nullptr || polarization_angles == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (num_component_ids > 0 && component_ids == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (pattern_width == 0 || pattern_height == 0) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Polarization pattern dimensions must be non-zero."};
  }

  PolarizationPattern pattern;
  pattern.component_ids.assign(component_ids, component_ids + num_component_ids);
  pattern.pattern_width = pattern_width;
  pattern.pattern_height = pattern_height;

  size_t num_pixels = size_t{pattern_width} * pattern_height;
  pattern.polarization_angles.assign(polarization_angles, polarization_angles + num_pixels);

  image->image->add_polarization_pattern(pattern);

  return heif_error_success;
}


int heif_image_get_number_of_polarization_patterns(const heif_image* image)
{
  if (image == nullptr) {
    return 0;
  }

  return static_cast<int>(image->image->get_polarization_patterns().size());
}


heif_error heif_image_get_polarization_pattern_info(const heif_image* image,
                                                    int pattern_index,
                                                    uint32_t* out_num_component_ids,
                                                    uint16_t* out_pattern_width,
                                                    uint16_t* out_pattern_height)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  const auto& patterns = image->image->get_polarization_patterns();
  if (pattern_index < 0 || static_cast<size_t>(pattern_index) >= patterns.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Polarization pattern index out of range."};
  }

  const auto& p = patterns[pattern_index];
  if (out_num_component_ids) {
    *out_num_component_ids = static_cast<uint32_t>(p.component_ids.size());
  }
  if (out_pattern_width) {
    *out_pattern_width = p.pattern_width;
  }
  if (out_pattern_height) {
    *out_pattern_height = p.pattern_height;
  }

  return heif_error_success;
}


heif_error heif_image_get_polarization_pattern_data(const heif_image* image,
                                                    int pattern_index,
                                                    uint32_t* out_component_ids,
                                                    float* out_polarization_angles)
{
  if (image == nullptr || out_polarization_angles == nullptr) {
    return heif_error_null_pointer_argument;
  }

  const auto& patterns = image->image->get_polarization_patterns();
  if (pattern_index < 0 || static_cast<size_t>(pattern_index) >= patterns.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Polarization pattern index out of range."};
  }

  const auto& p = patterns[pattern_index];

  if (out_component_ids && !p.component_ids.empty()) {
    std::copy(p.component_ids.begin(), p.component_ids.end(), out_component_ids);
  }

  size_t num_pixels = size_t{p.pattern_width} * p.pattern_height;
  std::copy(p.polarization_angles.begin(), p.polarization_angles.begin() + num_pixels, out_polarization_angles);

  return heif_error_success;
}


int heif_image_get_polarization_pattern_index_for_component(const heif_image* image,
                                                            uint32_t component_id)
{
  if (image == nullptr) {
    return -1;
  }

  const auto& patterns = image->image->get_polarization_patterns();
  for (size_t i = 0; i < patterns.size(); i++) {
    const auto& p = patterns[i];
    if (p.component_ids.empty()) {
      // Empty component list means pattern applies to all components.
      return static_cast<int>(i);
    }
    for (uint32_t idx : p.component_ids) {
      if (idx == component_id) {
        return static_cast<int>(i);
      }
    }
  }

  return -1;
}


heif_error heif_image_add_sensor_bad_pixels_map(heif_image* image,
                                                 uint32_t num_component_ids,
                                                 const uint32_t* component_ids,
                                                 int correction_applied,
                                                 uint32_t num_bad_rows,
                                                 const uint32_t* bad_rows,
                                                 uint32_t num_bad_columns,
                                                 const uint32_t* bad_columns,
                                                 uint32_t num_bad_pixels,
                                                 const struct heif_bad_pixel* bad_pixels)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (num_component_ids > 0 && component_ids == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (num_bad_rows > 0 && bad_rows == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (num_bad_columns > 0 && bad_columns == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (num_bad_pixels > 0 && bad_pixels == nullptr) {
    return heif_error_null_pointer_argument;
  }

  SensorBadPixelsMap map;
  map.component_ids.assign(component_ids, component_ids + num_component_ids);
  map.correction_applied = (correction_applied != 0);

  map.bad_rows.assign(bad_rows, bad_rows + num_bad_rows);
  map.bad_columns.assign(bad_columns, bad_columns + num_bad_columns);

  map.bad_pixels.resize(num_bad_pixels);
  for (uint32_t i = 0; i < num_bad_pixels; i++) {
    map.bad_pixels[i].row = bad_pixels[i].row;
    map.bad_pixels[i].column = bad_pixels[i].column;
  }

  image->image->add_sensor_bad_pixels_map(map);

  return heif_error_success;
}


int heif_image_get_number_of_sensor_bad_pixels_maps(const heif_image* image)
{
  if (image == nullptr) {
    return 0;
  }

  return static_cast<int>(image->image->get_sensor_bad_pixels_maps().size());
}


heif_error heif_image_get_sensor_bad_pixels_map_info(const heif_image* image,
                                                      int map_index,
                                                      uint32_t* out_num_component_ids,
                                                      int* out_correction_applied,
                                                      uint32_t* out_num_bad_rows,
                                                      uint32_t* out_num_bad_columns,
                                                      uint32_t* out_num_bad_pixels)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  const auto& maps = image->image->get_sensor_bad_pixels_maps();
  if (map_index < 0 || static_cast<size_t>(map_index) >= maps.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Sensor bad pixels map index out of range."};
  }

  const auto& m = maps[map_index];
  if (out_num_component_ids) {
    *out_num_component_ids = static_cast<uint32_t>(m.component_ids.size());
  }
  if (out_correction_applied) {
    *out_correction_applied = m.correction_applied ? 1 : 0;
  }
  if (out_num_bad_rows) {
    *out_num_bad_rows = static_cast<uint32_t>(m.bad_rows.size());
  }
  if (out_num_bad_columns) {
    *out_num_bad_columns = static_cast<uint32_t>(m.bad_columns.size());
  }
  if (out_num_bad_pixels) {
    *out_num_bad_pixels = static_cast<uint32_t>(m.bad_pixels.size());
  }

  return heif_error_success;
}


heif_error heif_image_get_sensor_bad_pixels_map_data(const heif_image* image,
                                                      int map_index,
                                                      uint32_t* out_component_ids,
                                                      uint32_t* out_bad_rows,
                                                      uint32_t* out_bad_columns,
                                                      struct heif_bad_pixel* out_bad_pixels)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  const auto& maps = image->image->get_sensor_bad_pixels_maps();
  if (map_index < 0 || static_cast<size_t>(map_index) >= maps.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Sensor bad pixels map index out of range."};
  }

  const auto& m = maps[map_index];

  if (out_component_ids && !m.component_ids.empty()) {
    std::copy(m.component_ids.begin(), m.component_ids.end(), out_component_ids);
  }

  if (out_bad_rows && !m.bad_rows.empty()) {
    std::copy(m.bad_rows.begin(), m.bad_rows.end(), out_bad_rows);
  }

  if (out_bad_columns && !m.bad_columns.empty()) {
    std::copy(m.bad_columns.begin(), m.bad_columns.end(), out_bad_columns);
  }

  if (out_bad_pixels && !m.bad_pixels.empty()) {
    for (size_t i = 0; i < m.bad_pixels.size(); i++) {
      out_bad_pixels[i].row = m.bad_pixels[i].row;
      out_bad_pixels[i].column = m.bad_pixels[i].column;
    }
  }

  return heif_error_success;
}


heif_error heif_image_add_sensor_nuc(heif_image* image,
                                      uint32_t num_component_ids,
                                      const uint32_t* component_ids,
                                      int nuc_is_applied,
                                      uint32_t image_width,
                                      uint32_t image_height,
                                      const float* nuc_gains,
                                      const float* nuc_offsets)
{
  if (image == nullptr || nuc_gains == nullptr || nuc_offsets == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (num_component_ids > 0 && component_ids == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (image_width == 0 || image_height == 0) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "NUC image dimensions must be non-zero."};
  }

  SensorNonUniformityCorrection nuc;
  nuc.component_ids.assign(component_ids, component_ids + num_component_ids);
  nuc.nuc_is_applied = (nuc_is_applied != 0);
  nuc.image_width = image_width;
  nuc.image_height = image_height;

  size_t num_pixels = size_t{image_width} * image_height;
  nuc.nuc_gains.assign(nuc_gains, nuc_gains + num_pixels);
  nuc.nuc_offsets.assign(nuc_offsets, nuc_offsets + num_pixels);

  image->image->add_sensor_nuc(nuc);

  return heif_error_success;
}


int heif_image_get_number_of_sensor_nucs(const heif_image* image)
{
  if (image == nullptr) {
    return 0;
  }

  return static_cast<int>(image->image->get_sensor_nuc().size());
}


heif_error heif_image_get_sensor_nuc_info(const heif_image* image,
                                           int nuc_index,
                                           uint32_t* out_num_component_ids,
                                           int* out_nuc_is_applied,
                                           uint32_t* out_image_width,
                                           uint32_t* out_image_height)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  const auto& nucs = image->image->get_sensor_nuc();
  if (nuc_index < 0 || static_cast<size_t>(nuc_index) >= nucs.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Sensor NUC index out of range."};
  }

  const auto& n = nucs[nuc_index];
  if (out_num_component_ids) {
    *out_num_component_ids = static_cast<uint32_t>(n.component_ids.size());
  }
  if (out_nuc_is_applied) {
    *out_nuc_is_applied = n.nuc_is_applied ? 1 : 0;
  }
  if (out_image_width) {
    *out_image_width = n.image_width;
  }
  if (out_image_height) {
    *out_image_height = n.image_height;
  }

  return heif_error_success;
}


heif_error heif_image_get_sensor_nuc_data(const heif_image* image,
                                           int nuc_index,
                                           uint32_t* out_component_ids,
                                           float* out_nuc_gains,
                                           float* out_nuc_offsets)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  const auto& nucs = image->image->get_sensor_nuc();
  if (nuc_index < 0 || static_cast<size_t>(nuc_index) >= nucs.size()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Sensor NUC index out of range."};
  }

  const auto& n = nucs[nuc_index];

  if (out_component_ids && !n.component_ids.empty()) {
    std::copy(n.component_ids.begin(), n.component_ids.end(), out_component_ids);
  }

  size_t num_pixels = size_t{n.image_width} * n.image_height;

  if (out_nuc_gains && !n.nuc_gains.empty()) {
    std::copy(n.nuc_gains.begin(), n.nuc_gains.begin() + num_pixels, out_nuc_gains);
  }

  if (out_nuc_offsets && !n.nuc_offsets.empty()) {
    std::copy(n.nuc_offsets.begin(), n.nuc_offsets.begin() + num_pixels, out_nuc_offsets);
  }

  return heif_error_success;
}


heif_error heif_image_set_chroma_location(heif_image* image, uint8_t chroma_location)
{
  if (image == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (chroma_location > 6) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Chroma location must be in the range 0-6."};
  }

  image->image->set_chroma_location(chroma_location);

  return heif_error_success;
}


int heif_image_has_chroma_location(const heif_image* image)
{
  if (image == nullptr) {
    return 0;
  }

  return image->image->has_chroma_location() ? 1 : 0;
}


uint8_t heif_image_get_chroma_location(const heif_image* image)
{
  if (image == nullptr) {
    return 0;
  }

  return image->image->get_chroma_location();
}
