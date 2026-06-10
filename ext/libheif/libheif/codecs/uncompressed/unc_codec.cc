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

#include "unc_decoder.h"
#include "codecs/decoder.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include "security_limits.h"
#include <utility>


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
    uint32_t component_index = component.component_index;
    uint16_t component_type = cmpd->get_components()[component_index].component_type;

    if (component_type > heif_cmpd_component_type_max_valid) {
      std::stringstream sstr;
      sstr << "a component_type > " << heif_cmpd_component_type_max_valid << " is not supported";
      return {heif_error_Unsupported_feature, heif_suberror_Invalid_parameter_value, sstr.str()};
    }
    if (component_type == heif_cmpd_component_type_padded) {
      // not relevant for determining chroma
      continue;
    }
    componentSet |= (1 << component_type);
  }

  *out_has_alpha = (componentSet & (1 << heif_cmpd_component_type_alpha)) != 0;

  if (componentSet == ((1 << heif_cmpd_component_type_red) | (1 << heif_cmpd_component_type_green) | (1 << heif_cmpd_component_type_blue)) ||
      componentSet == ((1 << heif_cmpd_component_type_red) | (1 << heif_cmpd_component_type_green) | (1 << heif_cmpd_component_type_blue) | (1 << heif_cmpd_component_type_alpha))) {
    *out_chroma = heif_chroma_444;
    *out_colourspace = heif_colorspace_RGB;
  }

  if (componentSet == ((1 << heif_cmpd_component_type_Y) | (1 << heif_cmpd_component_type_Cb) | (1 << heif_cmpd_component_type_Cr))) {
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

  if (componentSet == (1 << heif_cmpd_component_type_monochrome) || componentSet == ((1 << heif_cmpd_component_type_monochrome) | (1 << heif_cmpd_component_type_alpha)) ||
      componentSet == (1 << heif_cmpd_component_type_Y) || componentSet == ((1 << heif_cmpd_component_type_Y) | (1 << heif_cmpd_component_type_alpha))) {
    // mono or mono + alpha input, mono output.
    *out_chroma = heif_chroma_monochrome;
    *out_colourspace = heif_colorspace_monochrome;
  }

  if (componentSet == (1 << heif_cmpd_component_type_filter_array)) {
    // TODO - we should look up the components
    *out_chroma = heif_chroma_planar;
    *out_colourspace = heif_colorspace_filter_array;
  }

  // TODO: more combinations

  // out_colourspace remains heif_colorspace_undefined iff no branch above
  // matched any known component set.
  if (*out_colourspace == heif_colorspace_undefined) {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Could not determine colourspace");
  }

  return Error::Ok;
}

bool map_uncompressed_component_to_channel(const std::shared_ptr<const Box_cmpd>& cmpd,
                                           Box_uncC::Component component,
                                           heif_channel* channel)
{
  uint32_t component_index = component.component_index;
  uint16_t component_type = cmpd->get_components()[component_index].component_type;

  *channel = map_uncompressed_component_to_channel(component_type);
  return true;
}


heif_component_datatype unc_component_format_to_datatype(uint8_t format)
{
  // heif_component_datatype values are aligned with ISO/IEC 23001-17 Table 2.
  // is_valid_component_format() in unc_boxes.cc rejects out-of-range bytes
  // before they reach here, so any spec-defined byte casts cleanly.
  if (format > component_format_max_valid) {
    return heif_component_datatype_undefined;
  }
  return static_cast<heif_component_datatype>(format);
}


static Error validate_component_indices(const std::vector<uint32_t>& indices,
                                        size_t cmpd_size,
                                        const char* box_name)
{
  for (uint32_t idx : indices) {
    if (idx >= cmpd_size) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_parameter_value,
              std::string(box_name) + " component index out of range of cmpd table"};
    }
  }
  return Error::Ok;
}


Result<std::shared_ptr<HeifPixelImage>> UncompressedImageCodec::create_image(const unci_properties& properties,
                                                                             uint32_t width,
                                                                             uint32_t height,
                                                                             std::vector<uint32_t>& uncC_index_to_comp_ids,
                                                                             const heif_security_limits* limits)
{
  auto cmpd = properties.cmpd;
  auto uncC = properties.uncC;

  const auto& components = cmpd->get_components();

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


  // Clone the per-component descriptions (id, channel, type, format, datatype,
  // bit_depth) from the source ImageItem, populated at parse time. This is
  // what makes component IDs stable across the handle and decoded image.
  if (properties.source_extra_data) {
    img->clone_component_descriptions_from(*properties.source_extra_data);

    // Source descriptions carry the full-ispe plane sizes populated at parse
    // time. When this call is for a single tile, width/height are smaller, so
    // recompute the per-plane sizes from the actual target dimensions before
    // allocate_buffer_for_component() reads desc->width/height.
    for (const auto& desc_view : img->get_component_descriptions()) {
      if (!desc_view.has_data_plane) {
        continue;
      }
      ComponentDescription* desc = img->find_component_description(desc_view.component_id);
      desc->width = channel_width(width, chroma, desc->channel);
      desc->height = channel_height(height, chroma, desc->channel);
    }
  }

  // Remember which components reference which cmpd indices.
  // There can be several component ids referencing the same cmpd index.
  std::vector<std::vector<uint32_t>> cmpd_index_to_comp_ids(components.size());

  // Walk uncC. populate_component_descriptions() emitted one description per
  // uncC entry first, in order, so the first N descriptions in m_components
  // (where N == uncC->get_components().size()) correspond positionally to
  // the uncC entries we're walking here.
  uint32_t desc_idx = 0;
  for (Box_uncC::Component component : uncC->get_components()) {
    if (component.component_index >= components.size()) {
      return Error{
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Component index out of range."
      };
    }

    if (desc_idx >= img->get_component_descriptions().size()) {
      // Fallback: source did not populate descriptions (shouldn't happen
      // for an item that went through populate_component_descriptions, but
      // keep the historical add_component path as a safety net).
      auto component_type = components[component.component_index].component_type;
      uint32_t plane_w = width;
      uint32_t plane_h = height;
      if (component_type == heif_cmpd_component_type_Cb ||
          component_type == heif_cmpd_component_type_Cr) {
        plane_w = width / chroma_h_subsampling(chroma);
        plane_h = height / chroma_v_subsampling(chroma);
      }
      auto result = img->add_component(plane_w, plane_h, component_type,
                                       unc_component_format_to_datatype(component.component_format),
                                       component.component_bit_depth, limits);
      if (result.is_error()) {
        return result.error();
      }
      cmpd_index_to_comp_ids[component.component_index].push_back(*result);
      uncC_index_to_comp_ids.push_back(*result);
      continue;
    }

    // Pre-populated description path: the cloned description already has
    // the correct (chroma-subsampled) dimensions from
    // populate_component_descriptions(). Just allocate the buffer.
    uint32_t comp_id = img->get_component_descriptions()[desc_idx].component_id;
    if (auto err = img->allocate_buffer_for_component(comp_id, limits)) {
      return err;
    }

    cmpd_index_to_comp_ids[component.component_index].push_back(comp_id);
    uncC_index_to_comp_ids.push_back(comp_id);
    desc_idx++;
  }


  // --- assign the metadata boxes

  size_t cmpd_size = cmpd ? cmpd->get_components().size() : 0;

  if (properties.cpat) {
    const auto& pattern_cmpd = properties.cpat->get_pattern();
    std::vector<uint32_t> cpat_indices;
    for (const auto& pixel : pattern_cmpd.pixels) {
      cpat_indices.push_back(pixel.cmpd_index);
    }
    Error err = validate_component_indices(cpat_indices, cmpd_size, "cpat");
    if (err) {
      return err;
    }

    // Build BayerPattern. populate_component_descriptions added one
    // reference component per UNIQUE cmpd_index referenced by the pattern,
    // in first-occurrence order, immediately after the uncC components.
    // We rebuild the cmpd_index -> comp_id map in the same way and reuse
    // the existing IDs. (If descriptions weren't populated — fallback
    // path — we fall back to minting reference components here.)
    BayerPattern pattern;
    pattern.pattern_width = pattern_cmpd.pattern_width;
    pattern.pattern_height = pattern_cmpd.pattern_height;
    std::map<uint32_t, uint32_t> cpat_cmpd_idx_to_comp_id;
    for (auto p : pattern_cmpd.pixels) {
      uint32_t comp_id;
      auto it = cpat_cmpd_idx_to_comp_id.find(p.cmpd_index);
      if (it != cpat_cmpd_idx_to_comp_id.end()) {
        comp_id = it->second;
      }
      else if (desc_idx < img->get_component_descriptions().size()) {
        comp_id = img->get_component_descriptions()[desc_idx].component_id;
        desc_idx++;
        cpat_cmpd_idx_to_comp_id[p.cmpd_index] = comp_id;
      }
      else {
        comp_id = img->add_component_without_data(components[p.cmpd_index].component_type);
        cpat_cmpd_idx_to_comp_id[p.cmpd_index] = comp_id;
      }
      pattern.pixels.push_back({comp_id, p.component_gain});
      cmpd_index_to_comp_ids[p.cmpd_index].push_back(comp_id);
    }

    img->set_bayer_pattern(pattern);
  }

  for (const auto& splz_box : properties.splz) {
    const auto& pattern_cmpd = splz_box->get_pattern();
    Error err = validate_component_indices(pattern_cmpd.component_ids, cmpd_size, "splz");
    if (err) {
      return err;
    }
    PolarizationPattern pattern = pattern_cmpd;
    pattern.component_ids = map_cmpd_to_component_ids(pattern_cmpd.component_ids, cmpd_index_to_comp_ids);
    img->add_polarization_pattern(pattern);
  }

  for (const auto& sbpm_box : properties.sbpm) {
    const auto& bad_pixels_map_cmpd = sbpm_box->get_bad_pixels_map();
    Error err = validate_component_indices(bad_pixels_map_cmpd.component_ids, cmpd_size, "sbpm");
    if (err) {
      return err;
    }
    SensorBadPixelsMap bad_pixels_map = bad_pixels_map_cmpd;
    bad_pixels_map.component_ids = map_cmpd_to_component_ids(bad_pixels_map_cmpd.component_ids, cmpd_index_to_comp_ids);
    img->add_sensor_bad_pixels_map(bad_pixels_map);
  }

  for (const auto& snuc_box : properties.snuc) {
    const auto& nuc_cmpd = snuc_box->get_nuc();
    Error err = validate_component_indices(nuc_cmpd.component_ids, cmpd_size, "snuc");
    if (err) {
      return err;
    }
    SensorNonUniformityCorrection nuc = nuc_cmpd;
    nuc.component_ids = map_cmpd_to_component_ids(nuc_cmpd.component_ids, cmpd_index_to_comp_ids);
    img->add_sensor_nuc(nuc);
  }

  if (properties.cloc) {
    img->set_chroma_location(properties.cloc->get_chroma_location());
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


  // Remember which components reference which cmpd indices.
  // There can be several component ids referencing the same cmpd index.
  std::vector<std::vector<uint32_t>> cmpd_index_to_comp_ids;
  std::vector<uint32_t> uncC_index_to_comp_ids;

  Result<std::shared_ptr<HeifPixelImage>> createImgResult = create_image(properties, tile_width, tile_height, uncC_index_to_comp_ids, context->get_security_limits());
  if (!createImgResult) {
    return createImgResult.error();
  }

  img = *createImgResult;

  auto decoderResult = unc_decoder_factory::get_unc_decoder(ispe->get_width(), ispe->get_height(), cmpd, uncC, uncC_index_to_comp_ids);
  if (!decoderResult) {
    return decoderResult.error();
  }

  auto& decoder = *decoderResult;

  DataExtent dataExtent;
  dataExtent.set_from_image_item(file, ID);

  decoder->ensure_channel_list(img);

  std::vector<uint8_t> tile_data;
  Error err = decoder->fetch_tile_data(dataExtent, properties, tile_x0, tile_y0, tile_data);
  if (err) {
    return err;
  }

  return decoder->decode_tile(tile_data, img, 0, 0);
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

      uint16_t component_type = cmpd->get_components()[comp.component_index].component_type;
      if (component_type > 7 && component_type != heif_cmpd_component_type_padded && component_type != heif_cmpd_component_type_filter_array) {
        std::stringstream sstr;
        sstr << "Uncompressed image with component_type " << ((int) component_type) << " is not implemented yet";
        return {heif_error_Unsupported_feature,
                heif_suberror_Unsupported_data_version,
                sstr.str()};
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

  DataExtent dataExtent;
  dataExtent.set_from_image_item(context->get_heif_file(), ID);

  auto result = unc_decoder::decode_full_image(properties, dataExtent, context->get_security_limits());
  if (!result) {
    return result.error();
  }

  img = *result;
  return Error::Ok;
}


void UncompressedImageCodec::unci_properties::fill_from_image_item(const std::shared_ptr<const ImageItem>& image)
{
  ispe = image->get_property<Box_ispe>();
  auto cmpd_mut = image->get_property<Box_cmpd>();
  auto uncC_mut = image->get_property<Box_uncC>();
  if (uncC_mut) {
    fill_uncC_and_cmpd_from_profile(uncC_mut, cmpd_mut);
  }
  cmpd = cmpd_mut;
  uncC = uncC_mut;
  cmpC = image->get_property<Box_cmpC>();
  icef = image->get_property<Box_icef>();
  cpat = image->get_property<Box_cpat>();
  splz = image->get_all_properties<Box_splz>();
  sbpm = image->get_all_properties<Box_sbpm>();
  snuc = image->get_all_properties<Box_snuc>();
  cloc = image->get_property<Box_cloc>();

  // The ImageItem already populated its ImageDescription::m_components in
  // ImageItem_uncompressed::populate_component_descriptions(). The decoder
  // clones those descriptions into the new HeifPixelImage instead of minting
  // ids itself.
  source_extra_data = image.get();
}


Result<std::shared_ptr<HeifPixelImage>>
UncompressedImageCodec::decode_uncompressed_image(const UncompressedImageCodec::unci_properties& properties,
                                                  const DataExtent& extent,
                                                  const heif_security_limits* securityLimits)
{
  const std::shared_ptr<const Box_ispe>& ispe = properties.ispe;
  const std::shared_ptr<const Box_cmpd>& cmpd = properties.cmpd;
  const std::shared_ptr<const Box_uncC>& uncC = properties.uncC;

  Error error = check_header_validity(ispe, cmpd, uncC);
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

  return unc_decoder::decode_full_image(properties, extent, securityLimits);
}
