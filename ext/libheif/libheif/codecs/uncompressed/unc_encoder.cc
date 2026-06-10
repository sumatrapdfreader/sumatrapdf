/*
 * HEIF codec.
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#include "unc_encoder.h"

#include <cassert>
#include <cstring>
#include <map>
#include <set>

#include "image/pixelimage.h"
#include "unc_boxes.h"
#include "unc_encoder_component_interleave.h"
#include "unc_encoder_rgb_block_pixel_interleave.h"
#include "unc_encoder_rgb_pixel_interleave.h"
#include "unc_encoder_rgb_bytealign_pixel_interleave.h"
#include "libheif/heif_uncompressed.h"
#include "compression.h"
#include <utility>
#include <algorithm>


heif_cmpd_component_type heif_channel_to_component_type(heif_channel channel)
{
  switch (channel) {
    case heif_channel_Y: return heif_cmpd_component_type_Y;
    case heif_channel_Cb: return heif_cmpd_component_type_Cb;
    case heif_channel_Cr: return heif_cmpd_component_type_Cr;
    case heif_channel_R: return heif_cmpd_component_type_red;
    case heif_channel_G: return heif_cmpd_component_type_green;
    case heif_channel_B: return heif_cmpd_component_type_blue;
    case heif_channel_Alpha: return heif_cmpd_component_type_alpha;
    case heif_channel_interleaved: assert(false);
      break;
    case heif_channel_filter_array: return heif_cmpd_component_type_filter_array;
    case heif_channel_depth: return heif_cmpd_component_type_depth;
    case heif_channel_disparity: return heif_cmpd_component_type_disparity;
    case heif_channel_unknown: return heif_cmpd_component_type_padded;
  }

  return heif_cmpd_component_type_padded;
}


heif_uncompressed_component_format to_unc_component_format(heif_component_datatype channel_datatype)
{
  // heif_component_datatype values are aligned with ISO/IEC 23001-17 Table 2.
  if (channel_datatype == heif_component_datatype_undefined) {
    return component_format_unsigned;
  }
  return static_cast<heif_uncompressed_component_format>(channel_datatype);
}


unc_encoder::unc_encoder(const std::shared_ptr<const HeifPixelImage>& image)
{
  m_cmpd = std::make_shared<Box_cmpd>();
  m_uncC = std::make_shared<Box_uncC>();

  // --- fill component-id to cmpd-index map

  std::vector<uint32_t> ids = image->get_used_component_ids();

  for (size_t i = 0; i < ids.size(); i++) {
    m_map_id_to_cmpd_index[ids[i]] = static_cast<uint32_t>(i);
  }

  // TODO: we could combine component_ids with similar types if they are also used in the same way in the metadata boxes

  // --- create cmpd component types

  uint32_t max_cmpd_index=0;
  for (auto iter : m_map_id_to_cmpd_index) {
    max_cmpd_index = std::max(max_cmpd_index, iter.second);
  }

  std::vector<uint16_t> cmpd_types(static_cast<size_t>(max_cmpd_index) + 1);

  for (auto iter : m_map_id_to_cmpd_index) {
    uint16_t comp_type = image->get_component_type(iter.first);
    cmpd_types[iter.second] = comp_type;
  }

  m_cmpd->set_components(cmpd_types);

  // --- Bayer pattern: add reference components to cmpd and generate cpat box

  if (image->has_any_bayer_pattern()) {
    const BayerPattern& bayer = image->get_any_bayer_pattern();

    // Build cpat box with resolved cmpd indices

    m_cpat = std::make_shared<Box_cpat>();
    m_cpat->set_pattern(bayer.resolve_to_cmpd(m_map_id_to_cmpd_index));
  }

  if (image->has_polarization_patterns()) {
    for (const auto& pol : image->get_polarization_patterns()) {
      PolarizationPattern polCmpd = pol;
      polCmpd.component_ids = map_component_ids_to_cmpd(pol.component_ids, m_map_id_to_cmpd_index);
      auto splz = std::make_shared<Box_splz>();
      splz->set_pattern(polCmpd);
      m_splz.push_back(splz);
    }
  }

  if (image->has_sensor_bad_pixels_maps()) {
    for (const auto& bpm : image->get_sensor_bad_pixels_maps()) {
      SensorBadPixelsMap bpmCmpd = bpm;
      bpmCmpd.component_ids = map_component_ids_to_cmpd(bpm.component_ids, m_map_id_to_cmpd_index);
      auto sbpm = std::make_shared<Box_sbpm>();
      sbpm->set_bad_pixels_map(bpmCmpd);
      m_sbpm.push_back(sbpm);
    }
  }

  if (image->has_sensor_nuc()) {
    for (const auto& nuc : image->get_sensor_nuc()) {
      SensorNonUniformityCorrection nucCmpd = nuc;
      nucCmpd.component_ids = map_component_ids_to_cmpd(nuc.component_ids, m_map_id_to_cmpd_index);
      auto snuc = std::make_shared<Box_snuc>();
      snuc->set_nuc(nucCmpd);
      m_snuc.push_back(snuc);
    }
  }

  if (image->has_chroma_location()) {
    m_cloc = std::make_shared<Box_cloc>();
    m_cloc->set_chroma_location(image->get_chroma_location());
  }
}


Result<std::unique_ptr<const unc_encoder> > unc_encoder_factory::get_unc_encoder(const std::shared_ptr<const HeifPixelImage>& prototype_image,
                                                                                 const heif_encoding_options& options)
{
  static unc_encoder_factory_rgb_pixel_interleave enc_rgb_pixel_interleave;
  static unc_encoder_factory_rgb_block_pixel_interleave enc_rgb_block_pixel_interleave;
  static unc_encoder_factory_rgb_bytealign_pixel_interleave enc_rgb_bytealign_pixel_interleave;
  static unc_encoder_factory_component_interleave enc_component_interleave;

  static const unc_encoder_factory* encoders[]{
    &enc_rgb_pixel_interleave,
    &enc_rgb_block_pixel_interleave,
    &enc_rgb_bytealign_pixel_interleave,
    &enc_component_interleave
  };

  for (const unc_encoder_factory* enc : encoders) {
    if (enc->can_encode(prototype_image, options)) {
      return {enc->create(prototype_image, options)};
    }
  }

  return Error{
    heif_error_Unsupported_filetype,
    heif_suberror_Unspecified,
    "Input image configuration unsupported by uncompressed codec."
  };
}


heif_uncompressed_component_format to_unc_component_format(const std::shared_ptr<const HeifPixelImage>& image, heif_channel channel)
{
  heif_component_datatype datatype = image->get_datatype(channel);
  heif_uncompressed_component_format component_format = to_unc_component_format(datatype);
  return component_format;
}


Result<Encoder::CodedImageData> unc_encoder::encode(const std::shared_ptr<const HeifPixelImage>& src_image,
                                                           const heif_encoding_options& in_options) const
{
  auto parameters = std::unique_ptr<heif_unci_image_parameters,
                                    void (*)(heif_unci_image_parameters*)>(heif_unci_image_parameters_alloc(),
                                                                           heif_unci_image_parameters_release);

  parameters->image_width = src_image->get_width();
  parameters->image_height = src_image->get_height();
  parameters->tile_width = parameters->image_width;
  parameters->tile_height = parameters->image_height;


  heif_encoding_options options = in_options;
  if (src_image->has_alpha() && !options.save_alpha_channel) {
    // TODO: drop alpha channel
  }


  // --- generate configuration property boxes

  auto uncC = this->get_uncC();
  uncC->derive_box_version();

  Encoder::CodedImageData codedImageData;
  codedImageData.properties.push_back(uncC);
  if (!uncC->is_minimized()) {
    codedImageData.properties.push_back(this->get_cmpd());
  }

  if (m_cpat) {
    codedImageData.properties.push_back(m_cpat);
  }

  for (const auto& splz : m_splz) {
    codedImageData.properties.push_back(splz);
  }

  for (const auto& sbpm : m_sbpm) {
    codedImageData.properties.push_back(sbpm);
  }

  for (const auto& snuc : m_snuc) {
    codedImageData.properties.push_back(snuc);
  }

  if (m_cloc) {
    codedImageData.properties.push_back(m_cloc);
  }


  // --- encode image

  Result<std::vector<uint8_t> > codedBitstreamResult = this->encode_tile(src_image);
  if (!codedBitstreamResult) {
    return codedBitstreamResult.error();
  }

  // --- optionally compress

  heif_unci_compression compression = heif_unci_compression_off;
  if (in_options.version >= 8 && in_options.unci_parameters != nullptr) {
    compression = in_options.unci_parameters->compression;
  }

  if (compression != heif_unci_compression_off) {
    uint32_t compr_fourcc = unci_compression_to_fourcc(compression);

    auto compressed = compress_unci_fourcc(compr_fourcc,
                                            codedBitstreamResult->data(),
                                            codedBitstreamResult->size());
    if (!compressed) {
      return compressed.error();
    }

    auto cmpC = std::make_shared<Box_cmpC>();
    cmpC->set_compression_type(compr_fourcc);
    cmpC->set_compressed_unit_type(heif_cmpC_compressed_unit_type_image_tile);
    codedImageData.properties.push_back(cmpC);

    /* Generating an icef for a single unit is redundant because when no icef is present,
     * the whole data extent will automatically be used.

    auto icef = std::make_shared<Box_icef>();
    Box_icef::CompressedUnitInfo info;
    info.unit_offset = 0;
    info.unit_size = compressed->size();
    icef->add_component(info);
    codedImageData.properties.push_back(icef);
    */

    codedImageData.bitstream = std::move(*compressed);
  }
  else {
    codedImageData.bitstream = std::move(*codedBitstreamResult);
  }

  return codedImageData;
}
