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

#include "codecs/encoder.h"

#include "error.h"
#include "context.h"
#include "plugin_registry.h"
#include "api_structs.h"
#include "color-conversion/colorconversion.h"


void Encoder::CodedImageData::append(const uint8_t* data, size_t size)
{
  bitstream.insert(bitstream.end(), data, data + size);
}


void Encoder::CodedImageData::append_with_4bytes_size(const uint8_t* data, size_t size)
{
  assert(size <= 0xFFFFFFFF);

  uint8_t size_field[4];
  size_field[0] = (uint8_t) ((size >> 24) & 0xFF);
  size_field[1] = (uint8_t) ((size >> 16) & 0xFF);
  size_field[2] = (uint8_t) ((size >> 8) & 0xFF);
  size_field[3] = (uint8_t) ((size >> 0) & 0xFF);

  bitstream.insert(bitstream.end(), size_field, size_field + 4);
  bitstream.insert(bitstream.end(), data, data + size);
}



static nclx_profile compute_target_nclx_profile(const std::shared_ptr<HeifPixelImage>& image, const heif_color_profile_nclx* output_nclx_profile)
{
  nclx_profile target_nclx_profile;

  // If there is an output NCLX specified, use that.
  if (output_nclx_profile) {
    target_nclx_profile.set_from_heif_color_profile_nclx(output_nclx_profile);
  }
    // Otherwise, if there is an input NCLX, keep that.
  else if (image->has_nclx_color_profile()) {
    target_nclx_profile = image->get_color_profile_nclx();
  }
    // Otherwise, just use the defaults (set below)
  else {
    target_nclx_profile.set_undefined();
  }

  target_nclx_profile.replace_undefined_values_with_sRGB_defaults();

  return target_nclx_profile;
}


static bool nclx_profile_matches_spec(heif_colorspace colorspace,
                                      std::optional<nclx_profile> image_nclx,
                                      const heif_color_profile_nclx* spec_nclx)
{
  if (colorspace != heif_colorspace_YCbCr) {
    return true;
  }

  // No target specification -> always matches
  if (!spec_nclx) {
    return true;
  }

  if (!image_nclx) {
    static nclx_profile default_nclx;
    default_nclx.set_sRGB_defaults();

    // if no input nclx is specified, compare against default one
    image_nclx = default_nclx;
  }

  if (image_nclx->get_full_range_flag() != (spec_nclx->full_range_flag == 0 ? false : true)) {
    return false;
  }

  if (image_nclx->get_matrix_coefficients() != spec_nclx->matrix_coefficients) {
    return false;
  }

  // TODO: are the colour primaries relevant for matrix-coefficients != 12,13 ?
  //       If not, we should skip this test for anything else than matrix-coefficients != 12,13.
  if (image_nclx->get_colour_primaries() != spec_nclx->color_primaries) {
    return false;
  }

  return true;
}


extern void fill_default_color_conversion_options_ext(heif_color_conversion_options_ext& options);

Result<std::shared_ptr<HeifPixelImage>> Encoder::convert_colorspace_for_encoding(const std::shared_ptr<HeifPixelImage>& image,
                                                                                 heif_encoder* encoder,
                                                                                 const heif_color_profile_nclx* user_requested_output_nclx,
                                                                                 const heif_color_conversion_options* color_conversion_options,
                                                                                 const heif_security_limits* security_limits)
{
  const heif_color_profile_nclx* output_nclx_profile;

  if (const auto* nclx = get_forced_output_nclx()) {
    output_nclx_profile = nclx;
  } else {
    output_nclx_profile = user_requested_output_nclx;
  }


  heif_colorspace colorspace = image->get_colorspace();
  heif_chroma chroma = image->get_chroma_format();

  if (encoder->plugin->plugin_api_version >= 2) {
    encoder->plugin->query_input_colorspace2(encoder->encoder, &colorspace, &chroma);
  }
  else {
    encoder->plugin->query_input_colorspace(&colorspace, &chroma);
  }


  // If output format forces an NCLX, use that. Otherwise use user selected NCLX.

  nclx_profile target_nclx_profile = compute_target_nclx_profile(image, output_nclx_profile);

  // --- convert colorspace

  std::shared_ptr<HeifPixelImage> output_image;

  const std::optional<nclx_profile> image_nclx = image->get_color_profile_nclx();

  if (colorspace == image->get_colorspace() &&
      chroma == image->get_chroma_format() &&
      nclx_profile_matches_spec(colorspace, image_nclx, output_nclx_profile)) {
    return image;
  }


  // @TODO: use color profile when converting
  int output_bpp = 0; // same as input

  //auto target_nclx = std::make_shared<color_profile_nclx>();
  //target_nclx->set_from_heif_color_profile_nclx(target_heif_nclx);

  return convert_colorspace(image, colorspace, chroma, target_nclx_profile,
                            output_bpp, *color_conversion_options, nullptr,
                            security_limits);
}
