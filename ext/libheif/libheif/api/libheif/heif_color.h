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

#ifndef LIBHEIF_HEIF_COLOR_H
#define LIBHEIF_HEIF_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_library.h>

// forward declaration from other header files
typedef struct heif_image heif_image;


enum heif_chroma_downsampling_algorithm
{
  heif_chroma_downsampling_nearest_neighbor = 1,
  heif_chroma_downsampling_average = 2,

  // Combine with 'heif_chroma_upsampling_bilinear' for best quality.
  // Makes edges look sharper when using YUV 420 with bilinear chroma upsampling.
  heif_chroma_downsampling_sharp_yuv = 3
};

enum heif_chroma_upsampling_algorithm
{
  heif_chroma_upsampling_nearest_neighbor = 1,
  heif_chroma_upsampling_bilinear = 2
};


typedef struct heif_color_conversion_options
{
  // 'version' must be 1.
  uint8_t version;

  // --- version 1 options

  enum heif_chroma_downsampling_algorithm preferred_chroma_downsampling_algorithm;
  enum heif_chroma_upsampling_algorithm preferred_chroma_upsampling_algorithm;

  // When set to 'false' libheif may also use a different algorithm if the preferred one is not available
  // or using a different algorithm is computationally less complex. Note that currently (v1.17.0) this
  // means that for RGB input it will usually choose nearest-neighbor sampling because this is computationally
  // the simplest.
  // Set this field to 'true' if you want to make sure that the specified algorithm is used even
  // at the cost of slightly higher computation times.
  uint8_t only_use_preferred_chroma_algorithm;

  // --- Note that we cannot extend this struct because it is embedded in
  //     other structs (heif_decoding_options and heif_encoding_options).
} heif_color_conversion_options;


enum heif_alpha_composition_mode
{
  heif_alpha_composition_mode_none,
  heif_alpha_composition_mode_solid_color,
  heif_alpha_composition_mode_checkerboard,
};


typedef struct heif_color_conversion_options_ext
{
  uint8_t version;

  // --- version 1 options

  enum heif_alpha_composition_mode alpha_composition_mode;

  // color values should be specified in the range [0, 65535]
  uint16_t background_red, background_green, background_blue;
  uint16_t secondary_background_red, secondary_background_green, secondary_background_blue;
  uint16_t checkerboard_square_size;
} heif_color_conversion_options_ext;


// Assumes that it is a version=1 struct.
LIBHEIF_API
void heif_color_conversion_options_set_defaults(heif_color_conversion_options*);

LIBHEIF_API
heif_color_conversion_options_ext* heif_color_conversion_options_ext_alloc(void);

LIBHEIF_API
void heif_color_conversion_options_ext_copy(heif_color_conversion_options_ext* dst,
                                            const heif_color_conversion_options_ext* src);

LIBHEIF_API
void heif_color_conversion_options_ext_free(heif_color_conversion_options_ext*);


// ------------------------- color profiles -------------------------

enum heif_color_profile_type
{
  heif_color_profile_type_not_present = 0,
  heif_color_profile_type_nclx = heif_fourcc('n', 'c', 'l', 'x'),
  heif_color_profile_type_rICC = heif_fourcc('r', 'I', 'C', 'C'),
  heif_color_profile_type_prof = heif_fourcc('p', 'r', 'o', 'f')
};


// Returns 'heif_color_profile_type_not_present' if there is no color profile.
// If there is an ICC profile and an NCLX profile, the ICC profile is returned.
// TODO: we need a new API for this function as images can contain both NCLX and ICC at the same time.
//       However, you can still use heif_image_handle_get_raw_color_profile() and
//       heif_image_handle_get_nclx_color_profile() to access both profiles.
LIBHEIF_API
enum heif_color_profile_type heif_image_handle_get_color_profile_type(const heif_image_handle* handle);

LIBHEIF_API
size_t heif_image_handle_get_raw_color_profile_size(const heif_image_handle* handle);

// Returns 'heif_error_Color_profile_does_not_exist' when there is no ICC profile.
LIBHEIF_API
struct heif_error heif_image_handle_get_raw_color_profile(const heif_image_handle* handle,
                                                          void* out_data);


enum heif_color_primaries
{
  heif_color_primaries_ITU_R_BT_709_5 = 1, // g=0.3;0.6, b=0.15;0.06, r=0.64;0.33, w=0.3127,0.3290
  heif_color_primaries_unspecified = 2,
  heif_color_primaries_ITU_R_BT_470_6_System_M = 4,
  heif_color_primaries_ITU_R_BT_470_6_System_B_G = 5,
  heif_color_primaries_ITU_R_BT_601_6 = 6,
  heif_color_primaries_SMPTE_240M = 7,
  heif_color_primaries_generic_film = 8,
  heif_color_primaries_ITU_R_BT_2020_2_and_2100_0 = 9,
  heif_color_primaries_SMPTE_ST_428_1 = 10,
  heif_color_primaries_SMPTE_RP_431_2 = 11,
  heif_color_primaries_SMPTE_EG_432_1 = 12,
  heif_color_primaries_EBU_Tech_3213_E = 22
};

enum heif_transfer_characteristics
{
  heif_transfer_characteristic_ITU_R_BT_709_5 = 1,
  heif_transfer_characteristic_unspecified = 2,
  heif_transfer_characteristic_ITU_R_BT_470_6_System_M = 4,
  heif_transfer_characteristic_ITU_R_BT_470_6_System_B_G = 5,
  heif_transfer_characteristic_ITU_R_BT_601_6 = 6,
  heif_transfer_characteristic_SMPTE_240M = 7,
  heif_transfer_characteristic_linear = 8,
  heif_transfer_characteristic_logarithmic_100 = 9,
  heif_transfer_characteristic_logarithmic_100_sqrt10 = 10,
  heif_transfer_characteristic_IEC_61966_2_4 = 11,
  heif_transfer_characteristic_ITU_R_BT_1361 = 12,
  heif_transfer_characteristic_IEC_61966_2_1 = 13,
  heif_transfer_characteristic_ITU_R_BT_2020_2_10bit = 14,
  heif_transfer_characteristic_ITU_R_BT_2020_2_12bit = 15,
  heif_transfer_characteristic_ITU_R_BT_2100_0_PQ = 16,
  heif_transfer_characteristic_SMPTE_ST_428_1 = 17,
  heif_transfer_characteristic_ITU_R_BT_2100_0_HLG = 18
};

enum heif_matrix_coefficients
{
  heif_matrix_coefficients_RGB_GBR = 0,
  heif_matrix_coefficients_ITU_R_BT_709_5 = 1,  // TODO: or 709-6 according to h.273
  heif_matrix_coefficients_unspecified = 2,
  heif_matrix_coefficients_US_FCC_T47 = 4,
  heif_matrix_coefficients_ITU_R_BT_470_6_System_B_G = 5,
  heif_matrix_coefficients_ITU_R_BT_601_6 = 6,  // TODO: or 601-7 according to h.273
  heif_matrix_coefficients_SMPTE_240M = 7,
  heif_matrix_coefficients_YCgCo = 8,
  heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance = 9,
  heif_matrix_coefficients_ITU_R_BT_2020_2_constant_luminance = 10,
  heif_matrix_coefficients_SMPTE_ST_2085 = 11,
  heif_matrix_coefficients_chromaticity_derived_non_constant_luminance = 12,
  heif_matrix_coefficients_chromaticity_derived_constant_luminance = 13,
  heif_matrix_coefficients_ICtCp = 14
};

typedef struct heif_color_profile_nclx
{
  // === version 1 fields

  uint8_t version;

  enum heif_color_primaries color_primaries;
  enum heif_transfer_characteristics transfer_characteristics;
  enum heif_matrix_coefficients matrix_coefficients;
  uint8_t full_range_flag;

  // --- decoded values (not used when saving nclx)

  float color_primary_red_x, color_primary_red_y;
  float color_primary_green_x, color_primary_green_y;
  float color_primary_blue_x, color_primary_blue_y;
  float color_primary_white_x, color_primary_white_y;
} heif_color_profile_nclx;

LIBHEIF_API
heif_error heif_nclx_color_profile_set_color_primaries(heif_color_profile_nclx* nclx, uint16_t cp);

LIBHEIF_API
heif_error heif_nclx_color_profile_set_transfer_characteristics(heif_color_profile_nclx* nclx, uint16_t transfer_characteristics);

LIBHEIF_API
heif_error heif_nclx_color_profile_set_matrix_coefficients(heif_color_profile_nclx* nclx, uint16_t matrix_coefficients);

// Returns 'heif_error_Color_profile_does_not_exist' when there is no NCLX profile.
// TODO: This function does currently not return an NCLX profile if it is stored in the image bitstream.
//       Only NCLX profiles stored as colr boxes are returned. This may change in the future.
LIBHEIF_API
heif_error heif_image_handle_get_nclx_color_profile(const heif_image_handle* handle,
                                                    heif_color_profile_nclx** out_data);

// Returned color profile has 'version' field set to the maximum allowed.
// Do not fill values for higher versions as these might be outside the allocated structure size.
// May return NULL.
LIBHEIF_API
heif_color_profile_nclx* heif_nclx_color_profile_alloc(void);

LIBHEIF_API
void heif_nclx_color_profile_free(heif_color_profile_nclx* nclx_profile);

// Note: in early versions of HEIF, there could only be one color profile per image. However, this has been changed.
// This function will now return ICC if one is present and NCLX only if there is no ICC.
// You may better avoid this function and simply query for NCLX and ICC directly.
LIBHEIF_API
enum heif_color_profile_type heif_image_get_color_profile_type(const heif_image* image);

// Returns the size of the ICC profile if one is assigned to the image. Otherwise, it returns 0.
LIBHEIF_API
size_t heif_image_get_raw_color_profile_size(const heif_image* image);

// Returns the ICC profile if one is assigned to the image. Otherwise, it returns an error.
LIBHEIF_API
heif_error heif_image_get_raw_color_profile(const heif_image* image,
                                            void* out_data);

LIBHEIF_API
heif_error heif_image_get_nclx_color_profile(const heif_image* image,
                                             heif_color_profile_nclx** out_data);


// The color profile is not attached to the image handle because we might need it
// for color space transform and encoding.
LIBHEIF_API
heif_error heif_image_set_raw_color_profile(heif_image* image,
                                            const char* profile_type_fourcc_string,
                                            const void* profile_data,
                                            const size_t profile_size);

LIBHEIF_API
heif_error heif_image_set_nclx_color_profile(heif_image* image,
                                             const heif_color_profile_nclx* color_profile);


// TODO: this function does not make any sense yet, since we currently cannot modify existing HEIF files.
//LIBHEIF_API
//void heif_image_remove_color_profile(struct heif_image* image);


// --- content light level ---

// Note: a value of 0 for any of these values indicates that the value is undefined.
// The unit of these values is Candelas per square meter.
typedef struct heif_content_light_level
{
  uint16_t max_content_light_level;
  uint16_t max_pic_average_light_level;
} heif_content_light_level;

LIBHEIF_API
int heif_image_has_content_light_level(const heif_image*);

LIBHEIF_API
void heif_image_get_content_light_level(const heif_image*, heif_content_light_level* out);

// Returns whether the image has 'content light level' information. If 0 is returned, the output is not filled.
LIBHEIF_API
int heif_image_handle_get_content_light_level(const heif_image_handle*, heif_content_light_level* out);

LIBHEIF_API
void heif_image_set_content_light_level(const heif_image*, const heif_content_light_level* in);

LIBHEIF_API
void heif_image_handle_set_content_light_level(const heif_image_handle*, const heif_content_light_level* in);


// --- mastering display colour volume ---

// Note: color coordinates are defined according to the CIE 1931 definition of x as specified in ISO 11664-1 (see also ISO 11664-3 and CIE 15).
typedef struct heif_mastering_display_colour_volume
{
  uint16_t display_primaries_x[3];
  uint16_t display_primaries_y[3];
  uint16_t white_point_x;
  uint16_t white_point_y;
  uint32_t max_display_mastering_luminance;
  uint32_t min_display_mastering_luminance;
} heif_mastering_display_colour_volume;

// The units for max_display_mastering_luminance and min_display_mastering_luminance is Candelas per square meter.
typedef struct heif_decoded_mastering_display_colour_volume
{
  float display_primaries_x[3];
  float display_primaries_y[3];
  float white_point_x;
  float white_point_y;
  double max_display_mastering_luminance;
  double min_display_mastering_luminance;
} heif_decoded_mastering_display_colour_volume;

typedef struct heif_ambient_viewing_environment
{
  uint32_t ambient_illumination;
  uint16_t ambient_light_x;
  uint16_t ambient_light_y;
} heif_ambient_viewing_environment;

LIBHEIF_API
int heif_image_has_mastering_display_colour_volume(const heif_image*);

LIBHEIF_API
void heif_image_get_mastering_display_colour_volume(const heif_image*, heif_mastering_display_colour_volume* out);

// Returns whether the image has 'mastering display colour volume' information. If 0 is returned, the output is not filled.
LIBHEIF_API
int heif_image_handle_get_mastering_display_colour_volume(const heif_image_handle*, heif_mastering_display_colour_volume* out);

LIBHEIF_API
void heif_image_set_mastering_display_colour_volume(const heif_image*, const heif_mastering_display_colour_volume* in);

LIBHEIF_API
void heif_image_handle_set_mastering_display_colour_volume(const heif_image_handle*, const heif_mastering_display_colour_volume* in);


// Converts the internal numeric representation of heif_mastering_display_colour_volume to the
// normalized values, collected in heif_decoded_mastering_display_colour_volume.
// Values that are out-of-range are decoded to 0, indicating an undefined value (as specified in ISO/IEC 23008-2).
LIBHEIF_API
struct heif_error heif_mastering_display_colour_volume_decode(const heif_mastering_display_colour_volume* in,
                                                              heif_decoded_mastering_display_colour_volume* out);

#ifdef __cplusplus
}
#endif

#endif
