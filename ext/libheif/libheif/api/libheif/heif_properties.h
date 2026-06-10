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

typedef enum heif_item_property_type
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
} heif_item_property_type;

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

typedef enum heif_transform_mirror_direction
{
  heif_transform_mirror_direction_invalid = -1,
  heif_transform_mirror_direction_vertical = 0,    // flip image vertically
  heif_transform_mirror_direction_horizontal = 1   // flip image horizontally
} heif_transform_mirror_direction;

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


// ------------------------- ISO/IEC 23001-17 properties -------------------------
//
// Imaging-metadata properties defined by ISO/IEC 23001-17 (uncompressed-image
// format) and currently surfaced on uncompressed images. They operate on a
// heif_image (the in-memory pixel image) rather than on (context, item_id) like
// the box-level property API above.


// --- Bayer / filter array pattern (cpat box)

typedef struct heif_bayer_pattern_pixel
{
  uint32_t component_id;
  float component_gain;
} heif_bayer_pattern_pixel;


// Set a Bayer / filter array pattern on an image.
// The pattern is a 2D array of component indices with dimensions pattern_width x pattern_height.
// The number of entries in patternPixels must be pattern_width * pattern_height.
// The component_index values are indices into the cmpd component definition table.
// On the encoder path, these indices are generated by heif_image_add_component() and the
// encoder adds reference components to cmpd for pattern entries that don't have image planes.
// On the decoder path, they come directly from the cpat box.
LIBHEIF_API
heif_error heif_image_set_bayer_pattern(heif_image*,
                                        uint32_t bayer_component_id,
                                        uint16_t pattern_width,
                                        uint16_t pattern_height,
                                        const heif_bayer_pattern_pixel* patternPixels);

// Add a reference-only component to the image's component description table for use as
// a Bayer pattern entry. The component is registered (its component_type appears in the
// cmpd box) but carries no pixel plane of its own — the actual pixel data lives in the
// single combined Bayer component (added with heif_image_add_component()).
//
// Use this for the per-cell colors that the Bayer pattern references (e.g. red, green,
// blue) when those colors have no standalone plane. Pass the returned component_id in
// heif_bayer_pattern_pixel::component_id of the patternPixels array given to
// heif_image_set_bayer_pattern().
//
// component_type: one of heif_cmpd_component_type_* (e.g. _red, _green, _blue).
// out_component_id: receives the minted component id. Must not be NULL.
LIBHEIF_API
heif_error heif_image_add_bayer_component(heif_image*,
                                          uint16_t component_type,
                                          uint32_t* out_component_id);

// Returns whether the image has a Bayer / filter array pattern.
// If the image has a pattern, out_pattern_width and out_pattern_height are set.
// Either output pointer may be NULL if the caller does not need that value.
LIBHEIF_API
int heif_image_get_bayer_pattern_size(const heif_image*,
                                      uint32_t bayer_component_id,
                                      uint16_t* out_pattern_width,
                                      uint16_t* out_pattern_height);

// Get the Bayer / filter array pattern pixels.
// The caller must provide an array large enough for pattern_width * pattern_height entries
// (use heif_image_get_bayer_pattern_size() to query the dimensions first).
// Returns heif_error_Ok on success, or an error if no pattern is set.
LIBHEIF_API
heif_error heif_image_get_bayer_pattern(const heif_image*,
                                        uint32_t bayer_component_id,
                                        heif_bayer_pattern_pixel* out_patternPixels);


// --- Polarization pattern (ISO 23001-17, Section 6.1.5)

// Special float value indicating "no polarization filter" at a pattern position.
// On the wire this is the IEEE 754 bit pattern 0xFFFFFFFF (a signaling NaN).
// Test with heif_polarization_angle_is_no_filter() below, or with isnan()/std::isnan().

// Returns a float with the 0xFFFFFFFF bit pattern (NaN) representing "no polarization filter".
LIBHEIF_API
float heif_polarization_angle_no_filter(void);

// Returns non-zero if the given angle has the "no filter" bit pattern (0xFFFFFFFF).
LIBHEIF_API
int heif_polarization_angle_is_no_filter(float angle);

// Add a polarization pattern to an image.
// component_indices: array of component indices this pattern applies to (may be NULL if num_component_indices == 0,
//                    meaning the pattern applies to all components).
// polarization_angles: array of pattern_width * pattern_height float values.
//                      Each is an angle in degrees [0.0, 360.0), or heif_polarization_angle_no_filter() for "no filter".
// Multiple patterns can be added (one per distinct component group).
LIBHEIF_API
heif_error heif_image_add_polarization_pattern(heif_image*,
                                               uint32_t num_component_indices,
                                               const uint32_t* component_indices,
                                               uint16_t pattern_width,
                                               uint16_t pattern_height,
                                               const float* polarization_angles);

// Returns the number of polarization patterns on this image (0 if none).
LIBHEIF_API
int heif_image_get_number_of_polarization_patterns(const heif_image*);

// Get the sizes/dimensions of a polarization pattern (to allocate arrays for the data query).
LIBHEIF_API
heif_error heif_image_get_polarization_pattern_info(const heif_image*,
                                                    int pattern_index,
                                                    uint32_t* out_num_component_indices,
                                                    uint16_t* out_pattern_width,
                                                    uint16_t* out_pattern_height);

// Get the actual data of a polarization pattern.
// Caller must provide pre-allocated arrays:
//   out_component_indices: num_component_indices entries (may be NULL if num_component_indices == 0)
//   out_polarization_angles: pattern_width * pattern_height entries
LIBHEIF_API
heif_error heif_image_get_polarization_pattern_data(const heif_image*,
                                                    int pattern_index,
                                                    uint32_t* out_component_indices,
                                                    float* out_polarization_angles);

// Find the polarization pattern index that applies to a given component index.
// Returns the pattern index (>= 0), or -1 if no pattern matches.
// A pattern with an empty component list (component_count == 0) matches all components.
LIBHEIF_API
int heif_image_get_polarization_pattern_index_for_component(const heif_image*,
                                                            uint32_t component_index);


// --- Sensor bad pixels map (ISO 23001-17, Section 6.1.7)

typedef struct heif_bad_pixel
{
  uint32_t row; uint32_t column;
} heif_bad_pixel;

// Add a sensor bad pixels map to an image.
// component_indices: array of component indices this map applies to (may be NULL if num_component_indices == 0,
//                    meaning the map applies to all components).
// Multiple maps can be added (one per distinct component group with different defects).
LIBHEIF_API
heif_error heif_image_add_sensor_bad_pixels_map(heif_image*,
                                                 uint32_t num_component_indices,
                                                 const uint32_t* component_indices,
                                                 int correction_applied,
                                                 uint32_t num_bad_rows,
                                                 const uint32_t* bad_rows,
                                                 uint32_t num_bad_columns,
                                                 const uint32_t* bad_columns,
                                                 uint32_t num_bad_pixels,
                                                 const heif_bad_pixel* bad_pixels);

// Returns the number of sensor bad pixels maps on this image (0 if none).
LIBHEIF_API
int heif_image_get_number_of_sensor_bad_pixels_maps(const heif_image*);

// Get the sizes of a sensor bad pixels map (to allocate arrays for the data query).
LIBHEIF_API
heif_error heif_image_get_sensor_bad_pixels_map_info(const heif_image*,
                                                      int map_index,
                                                      uint32_t* out_num_component_indices,
                                                      int* out_correction_applied,
                                                      uint32_t* out_num_bad_rows,
                                                      uint32_t* out_num_bad_columns,
                                                      uint32_t* out_num_bad_pixels);

// Get the actual data of a sensor bad pixels map.
// Caller must provide pre-allocated arrays:
//   out_component_indices: num_component_indices entries (may be NULL if num_component_indices == 0)
//   out_bad_rows: num_bad_rows entries (may be NULL if num_bad_rows == 0)
//   out_bad_columns: num_bad_columns entries (may be NULL if num_bad_columns == 0)
//   out_bad_pixels: num_bad_pixels entries (may be NULL if num_bad_pixels == 0)
LIBHEIF_API
heif_error heif_image_get_sensor_bad_pixels_map_data(const heif_image*,
                                                      int map_index,
                                                      uint32_t* out_component_indices,
                                                      uint32_t* out_bad_rows,
                                                      uint32_t* out_bad_columns,
                                                      struct heif_bad_pixel* out_bad_pixels);


// --- Sensor non-uniformity correction (ISO 23001-17, Section 6.1.6)

// Add a sensor non-uniformity correction table to an image.
// component_indices: array of component indices this NUC applies to (may be NULL if num_component_indices == 0,
//                    meaning it applies to all components).
// nuc_gains and nuc_offsets: arrays of image_width * image_height float values.
// Correction equation: y = nuc_gain * x + nuc_offset.
// Multiple NUC tables can be added (one per distinct component group).
LIBHEIF_API
heif_error heif_image_add_sensor_nuc(heif_image*,
                                      uint32_t num_component_indices,
                                      const uint32_t* component_indices,
                                      int nuc_is_applied,
                                      uint32_t image_width,
                                      uint32_t image_height,
                                      const float* nuc_gains,
                                      const float* nuc_offsets);

// Returns the number of sensor NUC tables on this image (0 if none).
LIBHEIF_API
int heif_image_get_number_of_sensor_nucs(const heif_image*);

// Get the sizes of a sensor NUC table (to allocate arrays for the data query).
LIBHEIF_API
heif_error heif_image_get_sensor_nuc_info(const heif_image*,
                                           int nuc_index,
                                           uint32_t* out_num_component_indices,
                                           int* out_nuc_is_applied,
                                           uint32_t* out_image_width,
                                           uint32_t* out_image_height);

// Get the actual data of a sensor NUC table.
// Caller must provide pre-allocated arrays:
//   out_component_indices: num_component_indices entries (may be NULL if num_component_indices == 0)
//   out_nuc_gains: image_width * image_height entries
//   out_nuc_offsets: image_width * image_height entries
LIBHEIF_API
heif_error heif_image_get_sensor_nuc_data(const heif_image*,
                                           int nuc_index,
                                           uint32_t* out_component_indices,
                                           float* out_nuc_gains,
                                           float* out_nuc_offsets);


// --- Chroma sample location (ISO 23091-2 / ITU-T H.273 + ISO 23001-17, Section 6.1.4) [cloc box]

typedef enum heif_chroma420_sample_location {
  // values 0-5 according to ISO 23091-2 / ITU-T H.273
  heif_chroma420_sample_location_00_05 = 0,
  heif_chroma420_sample_location_05_05 = 1,
  heif_chroma420_sample_location_00_00 = 2,
  heif_chroma420_sample_location_05_00 = 3,
  heif_chroma420_sample_location_00_10 = 4,
  heif_chroma420_sample_location_05_10 = 5,

  // value 6 according to ISO 23001-17
  heif_chroma420_sample_location_00_00_01_00 = 6
} heif_chroma420_sample_location;

// Set the chroma sample location on an image.
// chroma_location must be in the range 0-6 (see heif_chroma420_sample_location).
LIBHEIF_API
heif_error heif_image_set_chroma_location(heif_image*, uint8_t chroma_location);

// Returns non-zero if the image has a chroma sample location set.
LIBHEIF_API
int heif_image_has_chroma_location(const heif_image*);

// Returns the chroma sample location (0-6), or 0 if none is set.
LIBHEIF_API
uint8_t heif_image_get_chroma_location(const heif_image*);


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
