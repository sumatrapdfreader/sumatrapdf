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

#include <libheif/heif_color.h>

#include "common_utils.h"
#include <cstdint>
#include "heif.h"
#include "pixelimage.h"
#include "api_structs.h"
#include "error.h"
#include <set>
#include <limits>

#include <algorithm>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cstring>
#include <array>


void heif_color_conversion_options_set_defaults(heif_color_conversion_options* options)
{
  options->version = 1;
#if HAVE_LIBSHARPYUV
  options->preferred_chroma_downsampling_algorithm = heif_chroma_downsampling_sharp_yuv;
#else
  options->preferred_chroma_downsampling_algorithm = heif_chroma_downsampling_average;
#endif

  options->preferred_chroma_upsampling_algorithm = heif_chroma_upsampling_bilinear;
  options->only_use_preferred_chroma_algorithm = true;
}


static void fill_default_color_conversion_options_ext(heif_color_conversion_options_ext& options)
{
  options.version = 1;
  options.alpha_composition_mode = heif_alpha_composition_mode_none;
  options.background_red = options.background_green = options.background_blue = 0xFFFF;
  options.secondary_background_red = options.secondary_background_green = options.secondary_background_blue = 0xCCCC;
  options.checkerboard_square_size = 16;
}


heif_color_conversion_options_ext* heif_color_conversion_options_ext_alloc()
{
  auto options = new heif_color_conversion_options_ext;

  fill_default_color_conversion_options_ext(*options);

  return options;
}


void heif_color_conversion_options_ext_copy(heif_color_conversion_options_ext* dst,
                                            const heif_color_conversion_options_ext* src)
{
  if (src == nullptr) {
    return;
  }

  int min_version = std::min(dst->version, src->version);

  switch (min_version) {
    case 1:
      dst->alpha_composition_mode = src->alpha_composition_mode;
      dst->background_red = src->background_red;
      dst->background_green = src->background_green;
      dst->background_blue = src->background_blue;
      dst->secondary_background_red = src->secondary_background_red;
      dst->secondary_background_green = src->secondary_background_green;
      dst->secondary_background_blue = src->secondary_background_blue;
      dst->checkerboard_square_size = src->checkerboard_square_size;
  }
}


void heif_color_conversion_options_ext_free(heif_color_conversion_options_ext* options)
{
  delete options;
}


heif_color_profile_type heif_image_handle_get_color_profile_type(const heif_image_handle* handle)
{
  auto profile_icc = handle->image->get_color_profile_icc();
  if (profile_icc) {
    return (heif_color_profile_type) profile_icc->get_type();
  }

  if (handle->image->has_nclx_color_profile()) {
    return heif_color_profile_type_nclx;
  }

  return heif_color_profile_type_not_present;
}


size_t heif_image_handle_get_raw_color_profile_size(const heif_image_handle* handle)
{
  auto profile_icc = handle->image->get_color_profile_icc();
  if (profile_icc) {
    return profile_icc->get_data().size();
  }
  else {
    return 0;
  }
}


heif_error heif_image_handle_get_raw_color_profile(const heif_image_handle* handle,
                                                   void* out_data)
{
  if (out_data == nullptr) {
    return heif_error_null_pointer_argument;
  }

  auto raw_profile = handle->image->get_color_profile_icc();
  if (raw_profile) {
    memcpy(out_data,
           raw_profile->get_data().data(),
           raw_profile->get_data().size());
  }
  else {
    Error err(heif_error_Color_profile_does_not_exist,
              heif_suberror_Unspecified);
    return err.error_struct(handle->image.get());
  }

  return Error::Ok.error_struct(handle->image.get());
}


static const std::set<std::underlying_type<heif_color_primaries>::type> known_color_primaries{
  heif_color_primaries_ITU_R_BT_709_5,
  heif_color_primaries_unspecified,
  heif_color_primaries_ITU_R_BT_470_6_System_M,
  heif_color_primaries_ITU_R_BT_470_6_System_B_G,
  heif_color_primaries_ITU_R_BT_601_6,
  heif_color_primaries_SMPTE_240M,
  heif_color_primaries_generic_film,
  heif_color_primaries_ITU_R_BT_2020_2_and_2100_0,
  heif_color_primaries_SMPTE_ST_428_1,
  heif_color_primaries_SMPTE_RP_431_2,
  heif_color_primaries_SMPTE_EG_432_1,
  heif_color_primaries_EBU_Tech_3213_E,
};


heif_error heif_nclx_color_profile_set_color_primaries(heif_color_profile_nclx* nclx, uint16_t cp)
{
  if (static_cast<std::underlying_type<heif_color_primaries>::type>(cp) > std::numeric_limits<std::underlying_type<heif_color_primaries>::type>::max()) {
    return Error(heif_error_Invalid_input, heif_suberror_Unknown_NCLX_color_primaries).error_struct(nullptr);
  }

  auto n = static_cast<std::underlying_type<heif_color_primaries>::type>(cp);
  if (known_color_primaries.find(n) != known_color_primaries.end()) {
    nclx->color_primaries = static_cast<heif_color_primaries>(n);
  }
  else {
    nclx->color_primaries = heif_color_primaries_unspecified;
    return Error(heif_error_Invalid_input, heif_suberror_Unknown_NCLX_color_primaries).error_struct(nullptr);
  }

  return Error::Ok.error_struct(nullptr);
}


static const std::set<std::underlying_type<heif_transfer_characteristics>::type> known_transfer_characteristics{
  heif_transfer_characteristic_ITU_R_BT_709_5,
  heif_transfer_characteristic_unspecified,
  heif_transfer_characteristic_ITU_R_BT_470_6_System_M,
  heif_transfer_characteristic_ITU_R_BT_470_6_System_B_G,
  heif_transfer_characteristic_ITU_R_BT_601_6,
  heif_transfer_characteristic_SMPTE_240M,
  heif_transfer_characteristic_linear,
  heif_transfer_characteristic_logarithmic_100,
  heif_transfer_characteristic_logarithmic_100_sqrt10,
  heif_transfer_characteristic_IEC_61966_2_4,
  heif_transfer_characteristic_ITU_R_BT_1361,
  heif_transfer_characteristic_IEC_61966_2_1,
  heif_transfer_characteristic_ITU_R_BT_2020_2_10bit,
  heif_transfer_characteristic_ITU_R_BT_2020_2_12bit,
  heif_transfer_characteristic_ITU_R_BT_2100_0_PQ,
  heif_transfer_characteristic_SMPTE_ST_428_1,
  heif_transfer_characteristic_ITU_R_BT_2100_0_HLG
};


heif_error heif_nclx_color_profile_set_transfer_characteristics(heif_color_profile_nclx* nclx, uint16_t tc)
{
  if (static_cast<std::underlying_type<heif_color_primaries>::type>(tc) > std::numeric_limits<std::underlying_type<heif_transfer_characteristics>::type>::max()) {
    return Error(heif_error_Invalid_input, heif_suberror_Unknown_NCLX_transfer_characteristics).error_struct(nullptr);
  }

  auto n = static_cast<std::underlying_type<heif_transfer_characteristics>::type>(tc);
  if (known_transfer_characteristics.find(n) != known_transfer_characteristics.end()) {
    nclx->transfer_characteristics = static_cast<heif_transfer_characteristics>(n);
  }
  else {
    nclx->transfer_characteristics = heif_transfer_characteristic_unspecified;
    return Error(heif_error_Invalid_input, heif_suberror_Unknown_NCLX_transfer_characteristics).error_struct(nullptr);
  }

  return Error::Ok.error_struct(nullptr);
}


static const std::set<std::underlying_type<heif_matrix_coefficients>::type> known_matrix_coefficients{
  heif_matrix_coefficients_RGB_GBR,
  heif_matrix_coefficients_ITU_R_BT_709_5,
  heif_matrix_coefficients_unspecified,
  heif_matrix_coefficients_US_FCC_T47,
  heif_matrix_coefficients_ITU_R_BT_470_6_System_B_G,
  heif_matrix_coefficients_ITU_R_BT_601_6,
  heif_matrix_coefficients_SMPTE_240M,
  heif_matrix_coefficients_YCgCo,
  heif_matrix_coefficients_ITU_R_BT_2020_2_non_constant_luminance,
  heif_matrix_coefficients_ITU_R_BT_2020_2_constant_luminance,
  heif_matrix_coefficients_SMPTE_ST_2085,
  heif_matrix_coefficients_chromaticity_derived_non_constant_luminance,
  heif_matrix_coefficients_chromaticity_derived_constant_luminance,
  heif_matrix_coefficients_ICtCp
};


heif_error heif_nclx_color_profile_set_matrix_coefficients(heif_color_profile_nclx* nclx, uint16_t mc)
{
  if (static_cast<std::underlying_type<heif_color_primaries>::type>(mc) > std::numeric_limits<std::underlying_type<heif_matrix_coefficients>::type>::max()) {
    return Error(heif_error_Invalid_input, heif_suberror_Unknown_NCLX_matrix_coefficients).error_struct(nullptr);
  }

  auto n = static_cast<std::underlying_type<heif_matrix_coefficients>::type>(mc);
  if (known_matrix_coefficients.find(n) != known_matrix_coefficients.end()) {
    nclx->matrix_coefficients = static_cast<heif_matrix_coefficients>(n);;
  }
  else {
    nclx->matrix_coefficients = heif_matrix_coefficients_unspecified;
    return Error(heif_error_Invalid_input, heif_suberror_Unknown_NCLX_matrix_coefficients).error_struct(nullptr);
  }

  return Error::Ok.error_struct(nullptr);
}


heif_error heif_image_handle_get_nclx_color_profile(const heif_image_handle* handle,
                                                    heif_color_profile_nclx** out_data)
{
  if (!out_data) {
    return heif_error_null_pointer_argument;
  }

  if (!handle->image->has_nclx_color_profile()) {
    Error err(heif_error_Color_profile_does_not_exist,
              heif_suberror_Unspecified);
    return err.error_struct(handle->image.get());
  }

  auto nclx_profile = handle->image->get_color_profile_nclx();
  Error err = nclx_profile.get_nclx_color_profile(out_data);

  return err.error_struct(handle->image.get());
}


heif_color_profile_nclx* heif_nclx_color_profile_alloc()
{
  auto profile = new heif_color_profile_nclx;

  if (profile) {
    profile->version = 1;

    // sRGB defaults
    profile->color_primaries = heif_color_primaries_ITU_R_BT_709_5; // 1
    profile->transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1; // 13
    profile->matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6; // 6
    profile->full_range_flag = true;
  }

  return profile;
}


void heif_nclx_color_profile_free(heif_color_profile_nclx* nclx_profile)
{
  delete nclx_profile;
}


heif_color_profile_type heif_image_get_color_profile_type(const heif_image* image)
{
  std::shared_ptr<const color_profile> profile;

  profile = image->image->get_color_profile_icc();
  if (profile) {
    return (heif_color_profile_type) profile->get_type();
  }

  if (image->image->has_nclx_color_profile()) {
    return heif_color_profile_type_nclx;
  }

  return heif_color_profile_type_not_present;
}


size_t heif_image_get_raw_color_profile_size(const heif_image* image)
{
  auto raw_profile = image->image->get_color_profile_icc();
  if (raw_profile) {
    return raw_profile->get_data().size();
  }
  else {
    return 0;
  }
}


heif_error heif_image_get_raw_color_profile(const heif_image* image,
                                            void* out_data)
{
  if (out_data == nullptr) {
    return heif_error_null_pointer_argument;
  }

  auto raw_profile = image->image->get_color_profile_icc();
  if (raw_profile) {
    memcpy(out_data,
           raw_profile->get_data().data(),
           raw_profile->get_data().size());
  }
  else {
    Error err(heif_error_Color_profile_does_not_exist,
              heif_suberror_Unspecified);
    return err.error_struct(image->image.get());
  }

  return Error::Ok.error_struct(image->image.get());
}


heif_error heif_image_get_nclx_color_profile(const heif_image* image,
                                             heif_color_profile_nclx** out_data)
{
  if (!out_data) {
    return heif_error_null_pointer_argument;
  }

  if (!image->image->has_nclx_color_profile()) {
    Error err(heif_error_Color_profile_does_not_exist,
              heif_suberror_Unspecified);
    return err.error_struct(image->image.get());
  }

  auto nclx_profile = image->image->get_color_profile_nclx();
  Error err = nclx_profile.get_nclx_color_profile(out_data);

  return err.error_struct(image->image.get());
}


heif_error heif_image_set_raw_color_profile(heif_image* image,
                                            const char* color_profile_type_fourcc,
                                            const void* profile_data,
                                            const size_t profile_size)
{
  if (strlen(color_profile_type_fourcc) != 4) {
    heif_error err = {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "Invalid color_profile_type (must be 4 characters)"
    };
    return err;
  }

  uint32_t color_profile_type = fourcc(color_profile_type_fourcc);

  std::vector<uint8_t> data;
  data.insert(data.end(),
              (const uint8_t*) profile_data,
              (const uint8_t*) profile_data + profile_size);

  auto color_profile = std::make_shared<color_profile_raw>(color_profile_type, data);

  image->image->set_color_profile_icc(color_profile);

  return heif_error_success;
}


heif_error heif_image_set_nclx_color_profile(heif_image* image,
                                             const heif_color_profile_nclx* color_profile)
{
  nclx_profile nclx;
  nclx.set_colour_primaries(color_profile->color_primaries);
  nclx.set_transfer_characteristics(color_profile->transfer_characteristics);
  nclx.set_matrix_coefficients(color_profile->matrix_coefficients);
  nclx.set_full_range_flag(color_profile->full_range_flag);

  image->image->set_color_profile_nclx(nclx);

  return heif_error_success;
}


// --- content light level ---

int heif_image_has_content_light_level(const heif_image* image)
{
  return image->image->has_clli();
}


void heif_image_get_content_light_level(const heif_image* image, heif_content_light_level* out)
{
  if (out) {
    *out = image->image->get_clli();
  }
}


int heif_image_handle_get_content_light_level(const heif_image_handle* handle, heif_content_light_level* out)
{
  auto clli = handle->image->get_property<Box_clli>();
  if (out && clli) {
    *out = clli->clli;
  }

  return clli ? 1 : 0;
}


void heif_image_set_content_light_level(const heif_image* image, const heif_content_light_level* in)
{
  if (in == nullptr) {
    return;
  }

  image->image->set_clli(*in);
}


void heif_image_handle_set_content_light_level(const heif_image_handle* handle, const heif_content_light_level* in)
{
  if (in == nullptr) {
    return;
  }

  handle->image->set_clli(*in);
}


// --- mastering display colour volume ---


int heif_image_has_mastering_display_colour_volume(const heif_image* image)
{
  return image->image->has_mdcv();
}


void heif_image_get_mastering_display_colour_volume(const heif_image* image, heif_mastering_display_colour_volume* out)
{
  *out = image->image->get_mdcv();
}


int heif_image_handle_get_mastering_display_colour_volume(const heif_image_handle* handle, heif_mastering_display_colour_volume* out)
{
  auto mdcv = handle->image->get_property<Box_mdcv>();
  if (out && mdcv) {
    *out = mdcv->mdcv;
  }

  return mdcv ? 1 : 0;
}


void heif_image_set_mastering_display_colour_volume(const heif_image* image, const heif_mastering_display_colour_volume* in)
{
  if (in == nullptr) {
    return;
  }

  image->image->set_mdcv(*in);
}


void heif_image_handle_set_mastering_display_colour_volume(const heif_image_handle* handle, const heif_mastering_display_colour_volume* in)
{
  if (in == nullptr) {
    return;
  }

  handle->image->set_mdcv(*in);
}


float mdcv_coord_decode_x(uint16_t coord)
{
  // check for unspecified value
  if (coord < 5 || coord > 37000) {
    return 0.0f;
  }

  return (float) (coord * 0.00002);
}


float mdcv_coord_decode_y(uint16_t coord)
{
  // check for unspecified value
  if (coord < 5 || coord > 42000) {
    return 0.0f;
  }

  return (float) (coord * 0.00002);
}


heif_error heif_mastering_display_colour_volume_decode(const heif_mastering_display_colour_volume* in,
                                                       heif_decoded_mastering_display_colour_volume* out)
{
  if (in == nullptr || out == nullptr) {
    return heif_error_null_pointer_argument;
  }

  for (int c = 0; c < 3; c++) {
    out->display_primaries_x[c] = mdcv_coord_decode_x(in->display_primaries_x[c]);
    out->display_primaries_y[c] = mdcv_coord_decode_y(in->display_primaries_y[c]);
  }

  out->white_point_x = mdcv_coord_decode_x(in->white_point_x);
  out->white_point_y = mdcv_coord_decode_y(in->white_point_y);

  if (in->max_display_mastering_luminance < 50000 || in->max_display_mastering_luminance > 100000000) {
    out->max_display_mastering_luminance = 0;
  }
  else {
    out->max_display_mastering_luminance = in->max_display_mastering_luminance * 0.0001;
  }

  if (in->min_display_mastering_luminance < 1 || in->min_display_mastering_luminance > 50000) {
    out->min_display_mastering_luminance = 0;
  }
  else {
    out->min_display_mastering_luminance = in->min_display_mastering_luminance * 0.0001;
  }

  return heif_error_success;
}
