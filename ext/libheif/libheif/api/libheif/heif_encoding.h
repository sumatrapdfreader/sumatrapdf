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

#ifndef LIBHEIF_HEIF_ENCODING_H
#define LIBHEIF_HEIF_ENCODING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <libheif/heif_library.h>
#include <libheif/heif_image.h>
#include <libheif/heif_context.h>
#include <libheif/heif_brands.h>
#include <libheif/heif_color.h>


// ----- encoder -----

/**
 * Opaque object that represents the encoder used to code the images.
 */
typedef struct heif_encoder heif_encoder;

// A description of the encoder's capabilities and name.
typedef struct heif_encoder_descriptor heif_encoder_descriptor;

// A configuration parameter of the encoder. Each encoder implementation may have a different
// set of parameters. For the most common settings (e.q. quality), special functions to set
// the parameters are provided.
typedef struct heif_encoder_parameter heif_encoder_parameter;


// Quick check whether there is an enoder available for the given format.
// Note that the encoder may be limited to a certain subset of features (e.g. only 8 bit, only lossy).
// You will have to query the specific capabilities further.
LIBHEIF_API
int heif_have_encoder_for_format(enum heif_compression_format format);

// Get a list of available encoders. You can filter the encoders by compression format and name.
// Use format_filter==heif_compression_undefined and name_filter==NULL as wildcards.
// The returned list of encoders is sorted by their priority (which is a plugin property).
// The number of encoders is returned, which are not more than 'count' if (out_encoders != nullptr).
// By setting out_encoders==nullptr, you can query the number of encoders, 'count' is ignored.
// Note: to get the actual encoder from the descriptors returned here, use heif_context_get_encoder().
LIBHEIF_API
int heif_get_encoder_descriptors(enum heif_compression_format format_filter,
                                 const char* name_filter,
                                 const heif_encoder_descriptor** out_encoders,
                                 int count);

// Return a long, descriptive name of the encoder (including version information).
LIBHEIF_API
const char* heif_encoder_descriptor_get_name(const heif_encoder_descriptor*);

// Return a short, symbolic name for identifying the encoder.
// This name should stay constant over different encoder versions.
LIBHEIF_API
const char* heif_encoder_descriptor_get_id_name(const heif_encoder_descriptor*);

LIBHEIF_API
enum heif_compression_format
heif_encoder_descriptor_get_compression_format(const heif_encoder_descriptor*);

LIBHEIF_API
int heif_encoder_descriptor_supports_lossy_compression(const heif_encoder_descriptor*);

LIBHEIF_API
int heif_encoder_descriptor_supports_lossless_compression(const heif_encoder_descriptor*);


// Get an encoder instance that can be used to actually encode images from a descriptor.
LIBHEIF_API
heif_error heif_context_get_encoder(heif_context* context,
                                    const heif_encoder_descriptor*,
                                    heif_encoder** out_encoder);

// Get an encoder for the given compression format. If there are several encoder plugins
// for this format, the encoder with the highest plugin priority will be returned.
LIBHEIF_API
heif_error heif_context_get_encoder_for_format(heif_context* context,
                                               enum heif_compression_format format,
                                               heif_encoder**);

/**
 * Release the encoder object after use.
 */
LIBHEIF_API
void heif_encoder_release(heif_encoder*);

// Get the encoder name from the encoder itself.
LIBHEIF_API
const char* heif_encoder_get_name(const heif_encoder*);


// --- Encoder Parameters ---

// Libheif supports settings parameters through specialized functions and through
// generic functions by parameter name. Sometimes, the same parameter can be set
// in both ways.
// We consider it best practice to use the generic parameter functions only in
// dynamically generated user interfaces, as no guarantees are made that some specific
// parameter names are supported by all plugins.


// Set a 'quality' factor (0-100). How this is mapped to actual encoding parameters is
// encoder dependent.
LIBHEIF_API
heif_error heif_encoder_set_lossy_quality(heif_encoder*, int quality);

LIBHEIF_API
heif_error heif_encoder_set_lossless(heif_encoder*, int enable);

// level should be between 0 (= none) to 4 (= full)
LIBHEIF_API
heif_error heif_encoder_set_logging_level(heif_encoder*, int level);

// Get a generic list of encoder parameters.
// Each encoder may define its own, additional set of parameters.
// You do not have to free the returned list.
LIBHEIF_API
const heif_encoder_parameter* const* heif_encoder_list_parameters(heif_encoder*);

// Return the parameter name.
LIBHEIF_API
const char* heif_encoder_parameter_get_name(const heif_encoder_parameter*);


enum heif_encoder_parameter_type
{
  heif_encoder_parameter_type_integer = 1,
  heif_encoder_parameter_type_boolean = 2,
  heif_encoder_parameter_type_string = 3
};

// Return the parameter type.
LIBHEIF_API
enum heif_encoder_parameter_type heif_encoder_parameter_get_type(const heif_encoder_parameter*);

// DEPRECATED. Use heif_encoder_parameter_get_valid_integer_values() instead.
LIBHEIF_API
heif_error heif_encoder_parameter_get_valid_integer_range(const heif_encoder_parameter*,
                                                          int* have_minimum_maximum,
                                                          int* minimum, int* maximum);

// If integer is limited by a range, have_minimum and/or have_maximum will be != 0 and *minimum, *maximum is set.
// If integer is limited by a fixed set of values, *num_valid_values will be >0 and *out_integer_array is set.
LIBHEIF_API
heif_error heif_encoder_parameter_get_valid_integer_values(const heif_encoder_parameter*,
                                                           int* have_minimum, int* have_maximum,
                                                           int* minimum, int* maximum,
                                                           int* num_valid_values,
                                                           const int** out_integer_array);

LIBHEIF_API
heif_error heif_encoder_parameter_get_valid_string_values(const heif_encoder_parameter*,
                                                          const char* const** out_stringarray);


LIBHEIF_API
heif_error heif_encoder_set_parameter_integer(heif_encoder*,
                                              const char* parameter_name,
                                              int value);

LIBHEIF_API
heif_error heif_encoder_get_parameter_integer(heif_encoder*,
                                              const char* parameter_name,
                                              int* value);

// TODO: name should be changed to heif_encoder_get_valid_integer_parameter_range
LIBHEIF_API // DEPRECATED.
heif_error heif_encoder_parameter_integer_valid_range(heif_encoder*,
                                                      const char* parameter_name,
                                                      int* have_minimum_maximum,
                                                      int* minimum, int* maximum);

LIBHEIF_API
heif_error heif_encoder_set_parameter_boolean(heif_encoder*,
                                              const char* parameter_name,
                                              int value);

LIBHEIF_API
heif_error heif_encoder_get_parameter_boolean(heif_encoder*,
                                              const char* parameter_name,
                                              int* value);

LIBHEIF_API
heif_error heif_encoder_set_parameter_string(heif_encoder*,
                                             const char* parameter_name,
                                             const char* value);

LIBHEIF_API
heif_error heif_encoder_get_parameter_string(heif_encoder*,
                                             const char* parameter_name,
                                             char* value, int value_size);

// returns a NULL-terminated list of valid strings or NULL if all values are allowed
LIBHEIF_API
heif_error heif_encoder_parameter_string_valid_values(heif_encoder*,
                                                      const char* parameter_name,
                                                      const char* const** out_stringarray);

LIBHEIF_API
heif_error heif_encoder_parameter_integer_valid_values(heif_encoder*,
                                                       const char* parameter_name,
                                                       int* have_minimum, int* have_maximum,
                                                       int* minimum, int* maximum,
                                                       int* num_valid_values,
                                                       const int** out_integer_array);

// Set a parameter of any type to the string value.
// Integer values are parsed from the string.
// Boolean values can be "true"/"false"/"1"/"0"
//
// x265 encoder specific note:
// When using the x265 encoder, you may pass any of its parameters by
// prefixing the parameter name with 'x265:'. Hence, to set the 'ctu' parameter,
// you will have to set 'x265:ctu' in libheif.
// Note that there is no checking for valid parameters when using the prefix.
LIBHEIF_API
heif_error heif_encoder_set_parameter(heif_encoder*,
                                      const char* parameter_name,
                                      const char* value);

// Get the current value of a parameter of any type as a human readable string.
// The returned string is compatible with heif_encoder_set_parameter().
LIBHEIF_API
heif_error heif_encoder_get_parameter(heif_encoder*,
                                      const char* parameter_name,
                                      char* value_ptr, int value_size);

// Query whether a specific parameter has a default value.
LIBHEIF_API
int heif_encoder_has_default(heif_encoder*,
                             const char* parameter_name);


// The orientation values are defined equal to the EXIF Orientation tag.
enum heif_orientation
{
  heif_orientation_normal = 1,
  heif_orientation_flip_horizontally = 2,
  heif_orientation_rotate_180 = 3,
  heif_orientation_flip_vertically = 4,
  heif_orientation_rotate_90_cw_then_flip_horizontally = 5,
  heif_orientation_rotate_90_cw = 6,
  heif_orientation_rotate_90_cw_then_flip_vertically = 7,
  heif_orientation_rotate_270_cw = 8
};


typedef struct heif_encoding_options
{
  uint8_t version;

  // version 1 options

  uint8_t save_alpha_channel; // default: true

  // version 2 options

  // DEPRECATED. This option is not required anymore. Its value will be ignored.
  uint8_t macOS_compatibility_workaround;

  // version 3 options

  uint8_t save_two_colr_boxes_when_ICC_and_nclx_available; // default: false

  // version 4 options

  // Set this to the NCLX parameters to be used in the output image or set to NULL
  // when the same parameters as in the input image should be used.
  heif_color_profile_nclx* output_nclx_profile;

  uint8_t macOS_compatibility_workaround_no_nclx_profile;

  // version 5 options

  // libheif will generate irot/imir boxes to match these orientations
  enum heif_orientation image_orientation;

  // version 6 options

  heif_color_conversion_options color_conversion_options;

  // version 7 options

  // Set this to true to use compressed form of uncC where possible.
  uint8_t prefer_uncC_short_form;

  // TODO: we should add a flag to force MIAF compatible outputs. E.g. this will put restrictions on grid tile sizes and
  //       might add a clap box when the grid output size does not match the color subsampling factors.
  //       Since some of these constraints have to be known before actually encoding the image, "forcing MIAF compatibility"
  //       could also be a flag in the heif_context.
} heif_encoding_options;

LIBHEIF_API
heif_encoding_options* heif_encoding_options_alloc(void);

LIBHEIF_API
void heif_encoding_options_copy(heif_encoding_options* dst, const heif_encoding_options* src);

LIBHEIF_API
void heif_encoding_options_free(heif_encoding_options*);


// Compress the input image.
// Returns a handle to the coded image in 'out_image_handle' unless out_image_handle = NULL.
// 'options' should be NULL for now.
// The first image added to the context is also automatically set the primary image, but
// you can change the primary image later with heif_context_set_primary_image().
LIBHEIF_API
heif_error heif_context_encode_image(heif_context*,
                                     const heif_image* image,
                                     heif_encoder* encoder,
                                     const heif_encoding_options* options,
                                     heif_image_handle** out_image_handle);

// offsets[] should either be NULL (all offsets==0) or an array of size 2*nImages with x;y offset pairs.
// If background_rgba is NULL, the background is transparent.
LIBHEIF_API
heif_error heif_context_add_overlay_image(heif_context* ctx,
                                          uint32_t image_width,
                                          uint32_t image_height,
                                          uint16_t nImages,
                                          const heif_item_id* image_ids,
                                          int32_t* offsets,
                                          const uint16_t background_rgba[4],
                                          heif_image_handle** out_iovl_image_handle);

LIBHEIF_API
heif_error heif_context_set_primary_image(heif_context*,
                                          heif_image_handle* image_handle);

// Set the major brand of the file.
// If this function is not called, the major brand is determined automatically from
// the image or sequence content.
LIBHEIF_API
void heif_context_set_major_brand(heif_context* ctx,
                                  heif_brand2 major_brand);

// Add a compatible brand that is now added automatically by libheif when encoding images (e.g. some application brands like 'geo1').
LIBHEIF_API
void heif_context_add_compatible_brand(heif_context* ctx,
                                       heif_brand2 compatible_brand);

// --- deprecated functions ---

// DEPRECATED, typo in function name
LIBHEIF_API
int heif_encoder_descriptor_supportes_lossy_compression(const heif_encoder_descriptor*);

// DEPRECATED, typo in function name
LIBHEIF_API
int heif_encoder_descriptor_supportes_lossless_compression(const heif_encoder_descriptor*);

// DEPRECATED: use heif_get_encoder_descriptors() instead.
// Get a list of available encoders. You can filter the encoders by compression format and name.
// Use format_filter==heif_compression_undefined and name_filter==NULL as wildcards.
// The returned list of encoders is sorted by their priority (which is a plugin property).
// The number of encoders is returned, which are not more than 'count' if (out_encoders != nullptr).
// By setting out_encoders==nullptr, you can query the number of encoders, 'count' is ignored.
// Note: to get the actual encoder from the descriptors returned here, use heif_context_get_encoder().
LIBHEIF_API
int heif_context_get_encoder_descriptors(heif_context*, // TODO: why do we need this parameter?
                                         enum heif_compression_format format_filter,
                                         const char* name_filter,
                                         const heif_encoder_descriptor** out_encoders,
                                         int count);

#ifdef __cplusplus
}
#endif

#endif
