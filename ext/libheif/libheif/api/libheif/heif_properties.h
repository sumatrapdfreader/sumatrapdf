/*
 * HEIF codec.
 * Copyright (c) 2017-2023 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_PROPERTIES_H
#define LIBHEIF_HEIF_PROPERTIES_H

#include "heif.h"

#ifdef __cplusplus
extern "C" {
#endif

// ------------------------- item properties -------------------------

enum heif_item_property_type
{
//  heif_item_property_unknown = -1,
  heif_item_property_type_invalid = 0,
  heif_item_property_type_user_description = heif_fourcc('u', 'd', 'e', 's'),
  heif_item_property_type_transform_mirror = heif_fourcc('i', 'm', 'i', 'r'),
  heif_item_property_type_transform_rotation = heif_fourcc('i', 'r', 'o', 't'),
  heif_item_property_type_transform_crop = heif_fourcc('c', 'l', 'a', 'p'),
  heif_item_property_type_image_size = heif_fourcc('i', 's', 'p', 'e'),
  heif_item_property_type_uuid = heif_fourcc('u', 'u', 'i', 'd'),
  heif_item_property_type_tai_clock_info = heif_fourcc('t', 'a', 'i', 'c'),
  heif_item_property_type_tai_timestamp = heif_fourcc('i', 't', 'a', 'i'),
  heif_item_property_type_extended_language = heif_fourcc('e', 'l', 'n', 'g')
};

// Get the heif_property_id for a heif_item_id.
// You may specify which property 'type' you want to receive.
// If you specify 'heif_item_property_type_invalid', all properties associated to that item are returned.
// The number of properties is returned, which are not more than 'count' if (out_list != nullptr).
// By setting out_list==nullptr, you can query the number of properties, 'count' is ignored.
LIBHEIF_API
int heif_item_get_properties_of_type(const heif_context* context,
                                     heif_item_id id,
                                     enum heif_item_property_type type,
                                     heif_property_id* out_list,
                                     int count);

// Returns all transformative properties in the correct order.
// This includes "irot", "imir", "clap".
// The number of properties is returned, which are not more than 'count' if (out_list != nullptr).
// By setting out_list==nullptr, you can query the number of properties, 'count' is ignored.
LIBHEIF_API
int heif_item_get_transformation_properties(const heif_context* context,
                                            heif_item_id id,
                                            heif_property_id* out_list,
                                            int count);

LIBHEIF_API
enum heif_item_property_type heif_item_get_property_type(const heif_context* context,
                                                         heif_item_id id,
                                                         heif_property_id property_id);

// The strings are managed by libheif. They will be deleted in heif_property_user_description_release().
typedef struct heif_property_user_description
{
  int version;

  // version 1

  const char* lang;
  const char* name;
  const char* description;
  const char* tags;
} heif_property_user_description;

// Get the "udes" user description property content.
// Undefined strings are returned as empty strings.
LIBHEIF_API
heif_error heif_item_get_property_user_description(const heif_context* context,
                                                   heif_item_id itemId,
                                                   heif_property_id propertyId,
                                                   heif_property_user_description** out);

// Add a "udes" user description property to the item.
// If any string pointers are NULL, an empty string will be used instead.
LIBHEIF_API
heif_error heif_item_add_property_user_description(const heif_context* context,
                                                   heif_item_id itemId,
                                                   const heif_property_user_description* description,
                                                   heif_property_id* out_propertyId);

// Release all strings and the object itself.
// Only call for objects that you received from heif_item_get_property_user_description().
LIBHEIF_API
void heif_property_user_description_release(heif_property_user_description*);

enum heif_transform_mirror_direction
{
  heif_transform_mirror_direction_invalid = -1,
  heif_transform_mirror_direction_vertical = 0,    // flip image vertically
  heif_transform_mirror_direction_horizontal = 1   // flip image horizontally
};

// Will return 'heif_transform_mirror_direction_invalid' in case of error.
// If 'propertyId==0', it returns the first imir property found.
LIBHEIF_API
enum heif_transform_mirror_direction heif_item_get_property_transform_mirror(const heif_context* context,
                                                                             heif_item_id itemId,
                                                                             heif_property_id propertyId);

// Returns only 0, 90, 180, or 270 angle values.
// Returns -1 in case of error (but it will only return an error in case of wrong usage).
// If 'propertyId==0', it returns the first irot property found.
LIBHEIF_API
int heif_item_get_property_transform_rotation_ccw(const heif_context* context,
                                                  heif_item_id itemId,
                                                  heif_property_id propertyId);

// Returns the number of pixels that should be removed from the four edges.
// Because of the way this data is stored, you have to pass the image size at the moment of the crop operation
// to compute the cropped border sizes.
// If 'propertyId==0', it returns the first clap property found.
LIBHEIF_API
void heif_item_get_property_transform_crop_borders(const heif_context* context,
                                                   heif_item_id itemId,
                                                   heif_property_id propertyId,
                                                   int image_width, int image_height,
                                                   int* left, int* top, int* right, int* bottom);

/**
 * @param context     The heif_context for the file
 * @param itemId      The image item id to which this property belongs.
 * @param fourcc_type The short four-cc type of the property to add.
 * @param uuid_type   If fourcc_type=='uuid', this should point to a 16-byte UUID type. It is ignored otherwise and can be NULL.
 * @param data        Data to insert for this property (including a full-box header, if required for this box).
 * @param size        Length of data in bytes.
 * @param is_essential   Whether this property is essential (boolean).
 * @param out_propertyId Outputs the id of the inserted property. Can be NULL.
*/
LIBHEIF_API
heif_error heif_item_add_raw_property(const heif_context* context,
                                      heif_item_id itemId,
                                      uint32_t fourcc_type,
                                      const uint8_t* uuid_type,
                                      const uint8_t* data, size_t size,
                                      int is_essential,
                                      heif_property_id* out_propertyId);

LIBHEIF_API
heif_error heif_item_get_property_raw_size(const heif_context* context,
                                           heif_item_id itemId,
                                           heif_property_id propertyId,
                                           size_t* out_size);

/**
 * @param out_data User-supplied array to write the property data to. The required size of the output array is given by heif_item_get_property_raw_size().
*/
LIBHEIF_API
heif_error heif_item_get_property_raw_data(const heif_context* context,
                                           heif_item_id itemId,
                                           heif_property_id propertyId,
                                           uint8_t* out_data);

/**
 * Get the extended type for an extended "uuid" box.
 *
 * This provides the UUID for the extended box.
 *
 * This method should only be called on properties of type `heif_item_property_type_uuid`.
 *
 * @param context the heif_context containing the HEIF file
 * @param itemId the image item id to which this property belongs.
 * @param propertyId the property index (1-based) to get the extended type for
 * @param out_extended_type output of the call, must be a pointer to at least 16-bytes.
 * @return heif_error_success or an error indicating the failure
 */
LIBHEIF_API
heif_error heif_item_get_property_uuid_type(const heif_context* context,
                                            heif_item_id itemId,
                                            heif_property_id propertyId,
                                            uint8_t out_extended_type[16]);


// ------------------------- intrinsic and extrinsic matrices -------------------------

typedef struct heif_camera_intrinsic_matrix
{
  double focal_length_x;
  double focal_length_y;
  double principal_point_x;
  double principal_point_y;
  double skew;
} heif_camera_intrinsic_matrix;


LIBHEIF_API
int heif_image_handle_has_camera_intrinsic_matrix(const heif_image_handle* handle);

LIBHEIF_API
heif_error heif_image_handle_get_camera_intrinsic_matrix(const heif_image_handle* handle,
                                                         heif_camera_intrinsic_matrix* out_matrix);


typedef struct heif_camera_extrinsic_matrix heif_camera_extrinsic_matrix;

LIBHEIF_API
int heif_image_handle_has_camera_extrinsic_matrix(const heif_image_handle* handle);

LIBHEIF_API
heif_error heif_image_handle_get_camera_extrinsic_matrix(const heif_image_handle* handle,
                                                         heif_camera_extrinsic_matrix** out_matrix);

LIBHEIF_API
void heif_camera_extrinsic_matrix_release(heif_camera_extrinsic_matrix*);

LIBHEIF_API
heif_error heif_camera_extrinsic_matrix_get_rotation_matrix(const heif_camera_extrinsic_matrix*,
                                                            double* out_matrix_row_major);

#ifdef __cplusplus
}
#endif

#endif
