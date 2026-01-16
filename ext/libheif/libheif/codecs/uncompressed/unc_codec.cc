/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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

#include "unc_codec.h"

#include "common_utils.h"
#include "context.h"
#include "error.h"
#include "libheif/heif.h"
#include "unc_types.h"
#include "unc_boxes.h"

#include "decoder_abstract.h"
#include "decoder_component_interleave.h"
#include "decoder_pixel_interleave.h"
#include "decoder_mixed_interleave.h"
#include "decoder_row_interleave.h"
#include "decoder_tile_component_interleave.h"

#include <algorithm>
#include <map>
#include <iostream>
#include <cassert>
#include "security_limits.h"


bool isKnownUncompressedFrameConfigurationBoxProfile(const std::shared_ptr<const Box_uncC>& uncC)
{
  return ((uncC != nullptr) && (uncC->get_version() == 1) && ((uncC->get_profile() == fourcc("rgb3")) || (uncC->get_profile() == fourcc("rgba")) || (uncC->get_profile() == fourcc("abgr"))));
}


static Error uncompressed_image_type_is_supported(const std::shared_ptr<const Box_uncC>& uncC,
                                                  const std::shared_ptr<const Box_cmpd>& cmpd)
{
  if (isKnownUncompressedFrameConfigurationBoxProfile(uncC)) {
    return Error::Ok;
  }
  if (!cmpd) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Missing required cmpd box (no match in uncC box) for uncompressed codec");
  }

  for (Box_uncC::Component component : uncC->get_components()) {
    uint16_t component_index = component.component_index;
    uint16_t component_type = cmpd->get_components()[component_index].component_type;
    if ((component_type > 7) && (component_type != component_type_padded) && (component_type != component_type_filter_array)) {
      std::stringstream sstr;
      sstr << "Uncompressed image with component_type " << ((int) component_type) << " is not implemented yet";
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   sstr.str());
    }

    if ((component.component_bit_depth > 16)) {
      std::stringstream sstr;
      sstr << "Uncompressed image with component_bit_depth " << ((int) component.component_bit_depth) << " is not implemented yet";
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   sstr.str());
    }
    if (component.component_format != component_format_unsigned) {
      std::stringstream sstr;
      sstr << "Uncompressed image with component_format " << ((int) component.component_format) << " is not implemented yet";
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   sstr.str());
    }
    if (component.component_align_size > 2) {
      std::stringstream sstr;
      sstr << "Uncompressed image with component_align_size " << ((int) component.component_align_size) << " is not implemented yet";
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   sstr.str());
    }
  }
  if ((uncC->get_sampling_type() != sampling_mode_no_subsampling)
      && (uncC->get_sampling_type() != sampling_mode_422)
      && (uncC->get_sampling_type() != sampling_mode_420)
      ) {
    std::stringstream sstr;
    sstr << "Uncompressed sampling_type of " << ((int) uncC->get_sampling_type()) << " is not implemented yet";
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }
  if ((uncC->get_interleave_type() != interleave_mode_component)
      && (uncC->get_interleave_type() != interleave_mode_pixel)
      && (uncC->get_interleave_type() != interleave_mode_mixed)
      && (uncC->get_interleave_type() != interleave_mode_row)
      && (uncC->get_interleave_type() != interleave_mode_tile_component)
      ) {
    std::stringstream sstr;
    sstr << "Uncompressed interleave_type of " << ((int) uncC->get_interleave_type()) << " is not implemented yet";
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }
  // Validity checks per ISO/IEC 23001-17 Section 5.2.1.5.3
  if (uncC->get_sampling_type() == sampling_mode_422) {
    // We check Y Cb and Cr appear in the chroma test
    // TODO: error for tile width not multiple of 2
    if ((uncC->get_interleave_type() != interleave_mode_component)
        && (uncC->get_interleave_type() != interleave_mode_mixed)
        && (uncC->get_interleave_type() != interleave_mode_multi_y)) {
      std::stringstream sstr;
      sstr << "YCbCr 4:2:2 subsampling is only valid with component, mixed or multi-Y interleave mode (ISO/IEC 23001-17 5.2.1.5.3).";
      return Error(heif_error_Invalid_input,
                   heif_suberror_Invalid_parameter_value,
                   sstr.str());
    }
    if ((uncC->get_row_align_size() != 0) && (uncC->get_interleave_type() == interleave_mode_component)) {
      if (uncC->get_row_align_size() % 2 != 0) {
        std::stringstream sstr;
        sstr << "YCbCr 4:2:2 subsampling with component interleave requires row_align_size to be a multiple of 2 (ISO/IEC 23001-17 5.2.1.5.3).";
        return Error(heif_error_Invalid_input,
                     heif_suberror_Invalid_parameter_value,
                     sstr.str());
      }
    }
    if (uncC->get_tile_align_size() != 0) {
      if (uncC->get_tile_align_size() % 2 != 0) {
        std::stringstream sstr;
        sstr << "YCbCr 4:2:2 subsampling requires tile_align_size to be a multiple of 2 (ISO/IEC 23001-17 5.2.1.5.3).";
        return Error(heif_error_Invalid_input,
                     heif_suberror_Invalid_parameter_value,
                     sstr.str());
      }
    }
  }
  // Validity checks per ISO/IEC 23001-17 Section 5.2.1.5.4
  if (uncC->get_sampling_type() == sampling_mode_422) {
    // We check Y Cb and Cr appear in the chroma test
    // TODO: error for tile width not multiple of 2
    if ((uncC->get_interleave_type() != interleave_mode_component)
        && (uncC->get_interleave_type() != interleave_mode_mixed)) {
      std::stringstream sstr;
      sstr << "YCbCr 4:2:0 subsampling is only valid with component or mixed interleave mode (ISO/IEC 23001-17 5.2.1.5.4).";
      return Error(heif_error_Invalid_input,
                   heif_suberror_Invalid_parameter_value,
                   sstr.str());
    }
    if ((uncC->get_row_align_size() != 0) && (uncC->get_interleave_type() == interleave_mode_component)) {
      if (uncC->get_row_align_size() % 2 != 0) {
        std::stringstream sstr;
        sstr << "YCbCr 4:2:2 subsampling with component interleave requires row_align_size to be a multiple of 2 (ISO/IEC 23001-17 5.2.1.5.4).";
        return Error(heif_error_Invalid_input,
                     heif_suberror_Invalid_parameter_value,
                     sstr.str());
      }
    }
    if (uncC->get_tile_align_size() != 0) {
      if (uncC->get_tile_align_size() % 4 != 0) {
        std::stringstream sstr;
        sstr << "YCbCr 4:2:2 subsampling requires tile_align_size to be a multiple of 4 (ISO/IEC 23001-17 5.2.1.5.3).";
        return Error(heif_error_Invalid_input,
                     heif_suberror_Invalid_parameter_value,
                     sstr.str());
      }
    }
  }
  if ((uncC->get_interleave_type() == interleave_mode_mixed) && (uncC->get_sampling_type() == sampling_mode_no_subsampling)) {
    std::stringstream sstr;
    sstr << "Interleave interleave mode is not valid with subsampling mode (ISO/IEC 23001-17 5.2.1.6.4).";
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_parameter_value,
                 sstr.str());
  }
  if ((uncC->get_interleave_type() == interleave_mode_multi_y)
      && ((uncC->get_sampling_type() != sampling_mode_422) && (uncC->get_sampling_type() != sampling_mode_411))) {
    std::stringstream sstr;
    sstr << "Multi-Y interleave mode is only valid with 4:2:2 and 4:1:1 subsampling modes (ISO/IEC 23001-17 5.2.1.6.7).";
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_parameter_value,
                 sstr.str());
  }
  // TODO: throw error if mixed and Cb and Cr are not adjacent.

  if (uncC->get_block_size() != 0) {
    std::stringstream sstr;
    sstr << "Uncompressed block_size of " << ((int) uncC->get_block_size()) << " is not implemented yet";
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }

  if (uncC->is_components_little_endian()) {
    const auto& comps = uncC->get_components();
    bool all_8_bit = std::all_of(comps.begin(), comps.end(),
                                 [](const Box_uncC::Component& c) { return c.component_bit_depth==8; });
    if (!all_8_bit) {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "Uncompressed components_little_endian == 1 is not implemented yet");
    }
  }

  if (uncC->is_block_pad_lsb()) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Uncompressed block_pad_lsb == 1 is not implemented yet");
  }
  if (uncC->is_block_little_endian()) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Uncompressed block_little_endian == 1 is not implemented yet");
  }
  if (uncC->is_block_reversed()) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Uncompressed block_reversed == 1 is not implemented yet");
  }
  if ((uncC->get_pixel_size() != 0) && ((uncC->get_interleave_type() != interleave_mode_pixel) && (uncC->get_interleave_type() != interleave_mode_multi_y))) {
    std::stringstream sstr;
    sstr << "Uncompressed pixel_size of " << ((int) uncC->get_pixel_size()) << " is only valid with interleave_type 1 or 5 (ISO/IEC 23001-17 5.2.1.7)";
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_parameter_value,
                 sstr.str());
  }
  return Error::Ok;
}


Error UncompressedImageCodec::get_heif_chroma_uncompressed(const std::shared_ptr<const Box_uncC>& uncC,
                                                           const std::shared_ptr<const Box_cmpd>& cmpd,
                                                           heif_chroma* out_chroma, heif_colorspace* out_colourspace,
                                                           bool* out_has_alpha)
{
  bool dummy_has_alpha;
  if (out_has_alpha == nullptr) {
    out_has_alpha = &dummy_has_alpha;
  }

  *out_chroma = heif_chroma_undefined;
  *out_colourspace = heif_colorspace_undefined;
  *out_has_alpha = false;

  Error error = check_header_validity(std::nullopt, cmpd, uncC);
  if (error) {
    return error;
  }


  if (uncC != nullptr && uncC->get_version() == 1) {
    switch (uncC->get_profile()) {
      case fourcc("rgb3"):
        *out_chroma = heif_chroma_444;
        *out_colourspace = heif_colorspace_RGB;
        *out_has_alpha = false;
        return Error::Ok;

      case fourcc("abgr"):
      case fourcc("rgba"):
        *out_chroma = heif_chroma_444;
        *out_colourspace = heif_colorspace_RGB;
        *out_has_alpha = true;
        return Error::Ok;

      default:
        return Error(heif_error_Unsupported_feature, heif_suberror_Unsupported_image_type,
                     "unci image has unsupported profile");
    }
  }


  // each 1-bit represents an existing component in the image
  uint16_t componentSet = 0;

  for (Box_uncC::Component component : uncC->get_components()) {
    uint16_t component_index = component.component_index;
    uint16_t component_type = cmpd->get_components()[component_index].component_type;

    if (component_type > component_type_max_valid) {
      std::stringstream sstr;
      sstr << "a component_type > " << component_type_max_valid << " is not supported";
      return {heif_error_Unsupported_feature, heif_suberror_Invalid_parameter_value, sstr.str()};
    }
    if (component_type == component_type_padded) {
      // not relevant for determining chroma
      continue;
    }
    componentSet |= (1 << component_type);
  }

  *out_has_alpha = (componentSet & (1 << component_type_alpha)) != 0;

  if (componentSet == ((1 << component_type_red) | (1 << component_type_green) | (1 << component_type_blue)) ||
      componentSet == ((1 << component_type_red) | (1 << component_type_green) | (1 << component_type_blue) | (1 << component_type_alpha))) {
    *out_chroma = heif_chroma_444;
    *out_colourspace = heif_colorspace_RGB;
  }

  if (componentSet == ((1 << component_type_Y) | (1 << component_type_Cb) | (1 << component_type_Cr))) {
    switch (uncC->get_sampling_type()) {
      case sampling_mode_no_subsampling:
        *out_chroma = heif_chroma_444;
        break;
      case sampling_mode_422:
        *out_chroma = heif_chroma_422;
        break;
      case sampling_mode_420:
        *out_chroma = heif_chroma_420;
        break;
    }
    *out_colourspace = heif_colorspace_YCbCr;
  }

  if (componentSet == ((1 << component_type_monochrome)) || componentSet == ((1 << component_type_monochrome) | (1 << component_type_alpha))) {
    // mono or mono + alpha input, mono output.
    *out_chroma = heif_chroma_monochrome;
    *out_colourspace = heif_colorspace_monochrome;
  }

  if (componentSet == (1 << component_type_filter_array)) {
    // TODO - we should look up the components
    *out_chroma = heif_chroma_monochrome;
    *out_colourspace = heif_colorspace_monochrome;
  }

  // TODO: more combinations

  if (*out_chroma == heif_chroma_undefined) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Could not determine chroma");
  }
  else if (*out_colourspace == heif_colorspace_undefined) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Could not determine colourspace");
  }
  else {
    return Error::Ok;
  }
}

bool map_uncompressed_component_to_channel(const std::shared_ptr<const Box_cmpd>& cmpd,
                                           const std::shared_ptr<const Box_uncC>& uncC,
                                           Box_uncC::Component component,
                                           heif_channel* channel)
{
  uint16_t component_index = component.component_index;
  if (isKnownUncompressedFrameConfigurationBoxProfile(uncC)) {
    if (uncC->get_profile() == fourcc("rgb3")) {
      switch (component_index) {
        case 0:
          *channel = heif_channel_R;
          return true;
        case 1:
          *channel = heif_channel_G;
          return true;
        case 2:
          *channel = heif_channel_B;
          return true;
      }
    }
    else if (uncC->get_profile() == fourcc("rgba")) {
      switch (component_index) {
        case 0:
          *channel = heif_channel_R;
          return true;
        case 1:
          *channel = heif_channel_G;
          return true;
        case 2:
          *channel = heif_channel_B;
          return true;
        case 3:
          *channel = heif_channel_Alpha;
          return true;
      }
    }
    else if (uncC->get_profile() == fourcc("abgr")) {
      switch (component_index) {
        case 0:
          *channel = heif_channel_Alpha;
          return true;
        case 1:
          *channel = heif_channel_B;
          return true;
        case 2:
          *channel = heif_channel_G;
          return true;
        case 3:
          *channel = heif_channel_R;
          return true;
      }
    }
  }
  uint16_t component_type = cmpd->get_components()[component_index].component_type;

  switch (component_type) {
    case component_type_monochrome:
      *channel = heif_channel_Y;
      return true;
    case component_type_Y:
      *channel = heif_channel_Y;
      return true;
    case component_type_Cb:
      *channel = heif_channel_Cb;
      return true;
    case component_type_Cr:
      *channel = heif_channel_Cr;
      return true;
    case component_type_red:
      *channel = heif_channel_R;
      return true;
    case component_type_green:
      *channel = heif_channel_G;
      return true;
    case component_type_blue:
      *channel = heif_channel_B;
      return true;
    case component_type_alpha:
      *channel = heif_channel_Alpha;
      return true;
    case component_type_filter_array:
      // TODO: this is just a temporary hack
      *channel = heif_channel_Y;
      return true;
    case component_type_padded:
      return false;
    default:
      return false;
  }
}



static AbstractDecoder* makeDecoder(uint32_t width, uint32_t height, const std::shared_ptr<const Box_cmpd>& cmpd, const std::shared_ptr<const Box_uncC>& uncC)
{
  switch (uncC->get_interleave_type()) {
    case interleave_mode_component:
      return new ComponentInterleaveDecoder(width, height, cmpd, uncC);
    case interleave_mode_pixel:
      return new PixelInterleaveDecoder(width, height, cmpd, uncC);
    case interleave_mode_mixed:
      return new MixedInterleaveDecoder(width, height, cmpd, uncC);
    case interleave_mode_row:
      return new RowInterleaveDecoder(width, height, cmpd, uncC);
    case interleave_mode_tile_component:
      return new TileComponentInterleaveDecoder(width, height, cmpd, uncC);
    default:
      return nullptr;
  }
}


Result<std::shared_ptr<HeifPixelImage>> UncompressedImageCodec::create_image(const std::shared_ptr<const Box_cmpd> cmpd,
                                                                             const std::shared_ptr<const Box_uncC> uncC,
                                                                             uint32_t width,
                                                                             uint32_t height,
                                                                             const heif_security_limits* limits)
{
  auto img = std::make_shared<HeifPixelImage>();
  heif_chroma chroma = heif_chroma_undefined;
  heif_colorspace colourspace = heif_colorspace_undefined;

  Error error = get_heif_chroma_uncompressed(uncC, cmpd, &chroma, &colourspace, nullptr);
  if (error) {
    return error;
  }
  img->create(width, height,
              colourspace,
              chroma);

  for (Box_uncC::Component component : uncC->get_components()) {
    heif_channel channel;
    if (map_uncompressed_component_to_channel(cmpd, uncC, component, &channel)) {
      if (img->has_channel(channel)) {
        return Error{heif_error_Unsupported_feature,
                     heif_suberror_Unspecified,
                     "Cannot generate image with several similar heif_channels."};
      }

      if ((channel == heif_channel_Cb) || (channel == heif_channel_Cr)) {
        if (auto err = img->add_plane(channel, (width / chroma_h_subsampling(chroma)), (height / chroma_v_subsampling(chroma)), component.component_bit_depth,
                                      limits)) {
          return err;
        }
      }
      else {
        if (auto err = img->add_plane(channel, width, height, component.component_bit_depth, limits)) {
          return err;
        }
      }
    }
  }

  return img;
}


Error UncompressedImageCodec::decode_uncompressed_image_tile(const HeifContext* context,
                                                             heif_item_id ID,
                                                             std::shared_ptr<HeifPixelImage>& img,
                                                             uint32_t tile_x0, uint32_t tile_y0)
{
  auto file = context->get_heif_file();
  auto image = context->get_image(ID, false);
  if (!image) {
    return {heif_error_Invalid_input,
            heif_suberror_Nonexisting_item_referenced};
  }

  UncompressedImageCodec::unci_properties properties;
  properties.fill_from_image_item(image);

  auto ispe = properties.ispe;
  auto uncC = properties.uncC;
  auto cmpd = properties.cmpd;

  Error error = check_header_validity(ispe, cmpd, uncC);
  if (error) {
    return error;
  }

  uint32_t tile_width = ispe->get_width() / uncC->get_number_of_tile_columns();
  uint32_t tile_height = ispe->get_height() / uncC->get_number_of_tile_rows();

  Result<std::shared_ptr<HeifPixelImage>> createImgResult = create_image(cmpd, uncC, tile_width, tile_height, context->get_security_limits());
  if (!createImgResult) {
    return createImgResult.error();
  }

  img = *createImgResult;


  AbstractDecoder* decoder = makeDecoder(ispe->get_width(), ispe->get_height(), cmpd, uncC);
  if (decoder == nullptr) {
    std::stringstream sstr;
    sstr << "Uncompressed interleave_type of " << ((int) uncC->get_interleave_type()) << " is not implemented yet";
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }

  decoder->buildChannelList(img);

  DataExtent dataExtent;
  dataExtent.set_from_image_item(file, ID);

  Error result = decoder->decode_tile(dataExtent, properties, img, 0, 0,
                                      ispe->get_width(), ispe->get_height(),
                                      tile_x0, tile_y0);
  delete decoder;
  return result;
}


Error UncompressedImageCodec::check_header_validity(std::optional<const std::shared_ptr<const Box_ispe>> ispe,
                                                    const std::shared_ptr<const Box_cmpd>& cmpd,
                                                    const std::shared_ptr<const Box_uncC>& uncC)
{
  // if we miss a required box, show error

  if (!uncC) {
    return {heif_error_Unsupported_feature,
            heif_suberror_Unsupported_data_version,
            "Missing required uncC box for uncompressed codec"};
  }

  if (!cmpd && (uncC->get_version() != 1)) {
    return {heif_error_Unsupported_feature,
            heif_suberror_Unsupported_data_version,
            "Missing required cmpd or uncC version 1 box for uncompressed codec"};
  }

  if (cmpd) {
    for (const auto& comp : uncC->get_components()) {
      if (comp.component_index >= cmpd->get_components().size()) {
        return {heif_error_Invalid_input,
                heif_suberror_Unspecified,
                "Invalid component index in uncC box"};
      }
    }
  }


  if (ispe) {
    if (!*ispe) {
      return {heif_error_Unsupported_feature,
              heif_suberror_Unsupported_data_version,
              "Missing required ispe box for uncompressed codec"};
    }

    if (uncC->get_number_of_tile_rows() > (*ispe)->get_height() ||
        uncC->get_number_of_tile_columns() > (*ispe)->get_width()) {
      return {heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "More tiles than pixels in uncC box"};
    }

    if ((*ispe)->get_height() % uncC->get_number_of_tile_rows() != 0 ||
        (*ispe)->get_width() % uncC->get_number_of_tile_columns() != 0) {
      return {heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "Invalid tile size (image size not a multiple of the tile size)"};
    }
  }

  return Error::Ok;
}


// TODO: this should be deprecated and replaced with the function taking unci_properties/DataExtent
Error UncompressedImageCodec::decode_uncompressed_image(const HeifContext* context,
                                                        heif_item_id ID,
                                                        std::shared_ptr<HeifPixelImage>& img)
{
  // Get the properties for this item
  // We need: ispe, cmpd, uncC
  std::vector<std::shared_ptr<Box>> item_properties;
  Error error = context->get_heif_file()->get_properties(ID, item_properties);
  if (error) {
    return error;
  }

  auto image = context->get_image(ID, false);
  if (!image) {
    return {heif_error_Invalid_input,
            heif_suberror_Nonexisting_item_referenced};
  }

  UncompressedImageCodec::unci_properties properties;
  properties.fill_from_image_item(image);

  auto ispe = properties.ispe;
  auto uncC = properties.uncC;
  auto cmpd = properties.cmpd;

  error = check_header_validity(ispe, cmpd, uncC);
  if (error) {
    return error;
  }

  // check if we support the type of image

  error = uncompressed_image_type_is_supported(uncC, cmpd); // TODO TODO TODO
  if (error) {
    return error;
  }

  assert(ispe);
  uint32_t width = ispe->get_width();
  uint32_t height = ispe->get_height();
  error = check_for_valid_image_size(context->get_security_limits(), width, height);
  if (error) {
    return error;
  }

  if (uncC->get_pixel_size() > 0 &&
      UINT32_MAX / uncC->get_pixel_size() / width < height) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Aligned total image size exceeds maximum integer range"
    };
  }

  if (uncC->get_row_align_size() > 0 &&
      UINT32_MAX / uncC->get_row_align_size() < 8) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Aligned row size larger than supported maximum"
    };
  }

  Result<std::shared_ptr<HeifPixelImage>> createImgResult = create_image(cmpd, uncC, width, height, context->get_security_limits());
  if (!createImgResult) {
    return createImgResult.error();
  }
  else {
    img = *createImgResult;
  }

  AbstractDecoder* decoder = makeDecoder(width, height, cmpd, uncC);
  if (decoder == nullptr) {
    std::stringstream sstr;
    sstr << "Uncompressed interleave_type of " << ((int) uncC->get_interleave_type()) << " is not implemented yet";
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }

  decoder->buildChannelList(img);

  uint32_t tile_width = width / uncC->get_number_of_tile_columns();
  uint32_t tile_height = height / uncC->get_number_of_tile_rows();

  DataExtent dataExtent;
  dataExtent.set_from_image_item(context->get_heif_file(), ID);

  for (uint32_t tile_y0 = 0; tile_y0 < height; tile_y0 += tile_height)
    for (uint32_t tile_x0 = 0; tile_x0 < width; tile_x0 += tile_width) {
      error = decoder->decode_tile(dataExtent, properties, img, tile_x0, tile_y0,
                                   width, height,
                                   tile_x0 / tile_width, tile_y0 / tile_height);
      if (error) {
        delete decoder;
        return error;
      }
    }

  //Error result = decoder->decode(source_data, img);
  delete decoder;
  return Error::Ok;
}


void UncompressedImageCodec::unci_properties::fill_from_image_item(const std::shared_ptr<const ImageItem>& image)
{
  ispe = image->get_property<Box_ispe>();
  cmpd = image->get_property<Box_cmpd>();
  uncC = image->get_property<Box_uncC>();
  cmpC = image->get_property<Box_cmpC>();
  icef = image->get_property<Box_icef>();
}


Result<std::shared_ptr<HeifPixelImage>>
UncompressedImageCodec::decode_uncompressed_image(const UncompressedImageCodec::unci_properties& properties,
                                                  const DataExtent& extent,
                                                  const heif_security_limits* securityLimits)
{
  std::shared_ptr<HeifPixelImage> img;

  const std::shared_ptr<const Box_ispe>& ispe = properties.ispe;
  const std::shared_ptr<const Box_cmpd>& cmpd = properties.cmpd;
  const std::shared_ptr<const Box_uncC>& uncC = properties.uncC;

  Error error = check_header_validity(ispe, cmpd, uncC);
  if (error) {
    return error;
  }

  // check if we support the type of image

  error = uncompressed_image_type_is_supported(uncC, cmpd); // TODO TODO TODO
  if (error) {
    return error;
  }

  assert(ispe);
  uint32_t width = ispe->get_width();
  uint32_t height = ispe->get_height();
  error = check_for_valid_image_size(securityLimits, width, height);
  if (error) {
    return error;
  }

  if (uncC->get_pixel_size() > 0 &&
      UINT32_MAX / uncC->get_pixel_size() / width < height) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Aligned total image size exceeds maximum integer range"
    };
  }

  Result<std::shared_ptr<HeifPixelImage>> createImgResult = create_image(cmpd, uncC, width, height, securityLimits);
  if (!createImgResult) {
    return createImgResult.error();
  }
  else {
    img = *createImgResult;
  }

  AbstractDecoder* decoder = makeDecoder(width, height, cmpd, uncC);
  if (decoder == nullptr) {
    std::stringstream sstr;
    sstr << "Uncompressed interleave_type of " << ((int) uncC->get_interleave_type()) << " is not implemented yet";
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 sstr.str());
  }

  decoder->buildChannelList(img);

  uint32_t tile_width = width / uncC->get_number_of_tile_columns();
  uint32_t tile_height = height / uncC->get_number_of_tile_rows();

  for (uint32_t tile_y0 = 0; tile_y0 < height; tile_y0 += tile_height)
    for (uint32_t tile_x0 = 0; tile_x0 < width; tile_x0 += tile_width) {
      error = decoder->decode_tile(extent, properties, img, tile_x0, tile_y0,
                                   width, height,
                                   tile_x0 / tile_width, tile_y0 / tile_height);
      if (error) {
        delete decoder;
        return error;
      }
    }

  //Error result = decoder->decode(source_data, img);
  delete decoder;
  return img;
}


Error fill_cmpd_and_uncC(std::shared_ptr<Box_cmpd>& cmpd,
                         std::shared_ptr<Box_uncC>& uncC,
                         const std::shared_ptr<const HeifPixelImage>& image,
                         const heif_unci_image_parameters* parameters,
                         bool save_alpha_channel)
{
  uint32_t nTileColumns = parameters->image_width / parameters->tile_width;
  uint32_t nTileRows = parameters->image_height / parameters->tile_height;

  const heif_colorspace colourspace = image->get_colorspace();
  if (colourspace == heif_colorspace_YCbCr) {
    if (!(image->has_channel(heif_channel_Y) && image->has_channel(heif_channel_Cb) && image->has_channel(heif_channel_Cr))) {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "Invalid colourspace / channel combination - YCbCr");
    }
    Box_cmpd::Component yComponent = {component_type_Y};
    cmpd->add_component(yComponent);
    Box_cmpd::Component cbComponent = {component_type_Cb};
    cmpd->add_component(cbComponent);
    Box_cmpd::Component crComponent = {component_type_Cr};
    cmpd->add_component(crComponent);
    uint8_t bpp_y = image->get_bits_per_pixel(heif_channel_Y);
    Box_uncC::Component component0 = {0, bpp_y, component_format_unsigned, 0};
    uncC->add_component(component0);
    uint8_t bpp_cb = image->get_bits_per_pixel(heif_channel_Cb);
    Box_uncC::Component component1 = {1, bpp_cb, component_format_unsigned, 0};
    uncC->add_component(component1);
    uint8_t bpp_cr = image->get_bits_per_pixel(heif_channel_Cr);
    Box_uncC::Component component2 = {2, bpp_cr, component_format_unsigned, 0};
    uncC->add_component(component2);
    if (image->get_chroma_format() == heif_chroma_444) {
      uncC->set_sampling_type(sampling_mode_no_subsampling);
    }
    else if (image->get_chroma_format() == heif_chroma_422) {
      uncC->set_sampling_type(sampling_mode_422);
    }
    else if (image->get_chroma_format() == heif_chroma_420) {
      uncC->set_sampling_type(sampling_mode_420);
    }
    else {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "Unsupported YCbCr sub-sampling type");
    }
    uncC->set_interleave_type(interleave_mode_component);
    uncC->set_block_size(0);
    uncC->set_components_little_endian(false);
    uncC->set_block_pad_lsb(false);
    uncC->set_block_little_endian(false);
    uncC->set_block_reversed(false);
    uncC->set_pad_unknown(false);
    uncC->set_pixel_size(0);
    uncC->set_row_align_size(0);
    uncC->set_tile_align_size(0);
    uncC->set_number_of_tile_columns(nTileColumns);
    uncC->set_number_of_tile_rows(nTileRows);
  }
  else if (colourspace == heif_colorspace_RGB) {
    if (!((image->get_chroma_format() == heif_chroma_444) ||
          (image->get_chroma_format() == heif_chroma_interleaved_RGB) ||
          (image->get_chroma_format() == heif_chroma_interleaved_RGBA) ||
          (image->get_chroma_format() == heif_chroma_interleaved_RRGGBB_BE) ||
          (image->get_chroma_format() == heif_chroma_interleaved_RRGGBB_LE) ||
          (image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE) ||
          (image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE))) {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "Unsupported colourspace / chroma combination - RGB");
    }

    Box_cmpd::Component rComponent = {component_type_red};
    cmpd->add_component(rComponent);
    Box_cmpd::Component gComponent = {component_type_green};
    cmpd->add_component(gComponent);
    Box_cmpd::Component bComponent = {component_type_blue};
    cmpd->add_component(bComponent);

    if (save_alpha_channel &&
        (image->get_chroma_format() == heif_chroma_interleaved_RGBA ||
         image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
         image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE ||
         image->has_channel(heif_channel_Alpha))) {
      Box_cmpd::Component alphaComponent = {component_type_alpha};
      cmpd->add_component(alphaComponent);
    }

    if (image->get_chroma_format() == heif_chroma_interleaved_RGB ||
        image->get_chroma_format() == heif_chroma_interleaved_RGBA ||
        image->get_chroma_format() == heif_chroma_interleaved_RRGGBB_BE ||
        image->get_chroma_format() == heif_chroma_interleaved_RRGGBB_LE ||
        image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
        image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE) {
      uncC->set_interleave_type(interleave_mode_pixel);
      int bpp = image->get_bits_per_pixel(heif_channel_interleaved);
      uint8_t component_align = 1;
      if (bpp == 8) {
        component_align = 0;
      }
      else if (bpp > 8) {
        component_align = 2;
      }

      Box_uncC::Component component0 = {0, (uint8_t) (bpp), component_format_unsigned, component_align};
      uncC->add_component(component0);
      Box_uncC::Component component1 = {1, (uint8_t) (bpp), component_format_unsigned, component_align};
      uncC->add_component(component1);
      Box_uncC::Component component2 = {2, (uint8_t) (bpp), component_format_unsigned, component_align};
      uncC->add_component(component2);

      if (save_alpha_channel &&
          (image->get_chroma_format() == heif_chroma_interleaved_RGBA ||
           image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
           image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE)) {
        Box_uncC::Component component3 = {
            3, (uint8_t) (bpp), component_format_unsigned, component_align};
        uncC->add_component(component3);
      }
    }
    else {
      uncC->set_interleave_type(interleave_mode_component);

      int bpp_red = image->get_bits_per_pixel(heif_channel_R);
      Box_uncC::Component component0 = {0, (uint8_t) (bpp_red), component_format_unsigned, 0};
      uncC->add_component(component0);

      int bpp_green = image->get_bits_per_pixel(heif_channel_G);
      Box_uncC::Component component1 = {1, (uint8_t) (bpp_green), component_format_unsigned, 0};
      uncC->add_component(component1);

      int bpp_blue = image->get_bits_per_pixel(heif_channel_B);
      Box_uncC::Component component2 = {2, (uint8_t) (bpp_blue), component_format_unsigned, 0};
      uncC->add_component(component2);

      if (save_alpha_channel && image->has_channel(heif_channel_Alpha)) {
        int bpp_alpha = image->get_bits_per_pixel(heif_channel_Alpha);
        Box_uncC::Component component3 = {3, (uint8_t) (bpp_alpha), component_format_unsigned, 0};
        uncC->add_component(component3);
      }
    }

    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_block_size(0);

    if ((image->get_chroma_format() == heif_chroma_interleaved_RRGGBB_LE) ||
        (image->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE)) {
      uncC->set_components_little_endian(true);
    }
    else {
      uncC->set_components_little_endian(false);
    }

    uncC->set_block_pad_lsb(false);
    uncC->set_block_little_endian(false);
    uncC->set_block_reversed(false);
    uncC->set_pad_unknown(false);
    uncC->set_pixel_size(0);
    uncC->set_row_align_size(0);
    uncC->set_tile_align_size(0);
    uncC->set_number_of_tile_columns(nTileColumns);
    uncC->set_number_of_tile_rows(nTileRows);
  }
  else if (colourspace == heif_colorspace_monochrome) {
    Box_cmpd::Component monoComponent = {component_type_monochrome};
    cmpd->add_component(monoComponent);

    if (save_alpha_channel && image->has_channel(heif_channel_Alpha)) {
      Box_cmpd::Component alphaComponent = {component_type_alpha};
      cmpd->add_component(alphaComponent);
    }

    int bpp = image->get_bits_per_pixel(heif_channel_Y);
    Box_uncC::Component component0 = {0, (uint8_t) (bpp), component_format_unsigned, 0};
    uncC->add_component(component0);

    if (save_alpha_channel && image->has_channel(heif_channel_Alpha)) {
      bpp = image->get_bits_per_pixel(heif_channel_Alpha);
      Box_uncC::Component component1 = {1, (uint8_t) (bpp), component_format_unsigned, 0};
      uncC->add_component(component1);
    }

    uncC->set_sampling_type(sampling_mode_no_subsampling);
    uncC->set_interleave_type(interleave_mode_component);
    uncC->set_block_size(0);
    uncC->set_components_little_endian(false);
    uncC->set_block_pad_lsb(false);
    uncC->set_block_little_endian(false);
    uncC->set_block_reversed(false);
    uncC->set_pad_unknown(false);
    uncC->set_pixel_size(0);
    uncC->set_row_align_size(0);
    uncC->set_tile_align_size(0);
    uncC->set_number_of_tile_columns(nTileColumns);
    uncC->set_number_of_tile_rows(nTileRows);
  }
  else {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Unsupported colourspace");
  }
  return Error::Ok;
}
