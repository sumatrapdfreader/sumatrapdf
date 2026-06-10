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

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <map>
#include <iostream>
#include <cassert>
#include <utility>

#include "common_utils.h"
#include "context.h"
#include "compression.h"
#include "error.h"
#include "libheif/heif.h"
#include "codecs/uncompressed/unc_types.h"
#include "codecs/uncompressed/unc_boxes.h"
#include "unc_image.h"
#include "codecs/uncompressed/unc_dec.h"
#include "codecs/uncompressed/unc_enc.h"
#include "codecs/uncompressed/unc_codec.h"
#include "image_item.h"
#include "codecs/uncompressed/unc_encoder.h"


struct unciHeaders;

ImageItem_uncompressed::ImageItem_uncompressed(HeifContext* ctx, heif_item_id id)
    : ImageItem(ctx, id)
{
  m_encoder = std::make_shared<Encoder_uncompressed>();
}

void ImageItem_uncompressed::populate_component_descriptions()
{
  // Idempotent: this method is called from both set_properties() (where
  // unci populates from cmpd/uncC, no decoder needed) and from
  // context.cc after initialize_decoder() (where visual codecs populate).
  // For unci items the first call wins; the second is a no-op.
  if (!get_component_descriptions().empty()) {
    return;
  }

  auto uncC = get_property<Box_uncC>();
  if (!uncC) {
    return;
  }
  auto cmpd = get_property<Box_cmpd>();

  // For minimized (version-1) uncC, expand the profile fourcc into the
  // components vector and synthesize cmpd if missing.
  fill_uncC_and_cmpd_from_profile(uncC, cmpd);
  if (!cmpd) {
    return;
  }

  const auto& cmpd_components = cmpd->get_components();
  uint32_t img_width = get_ispe_width();
  uint32_t img_height = get_ispe_height();

  // Determine chroma format so we can apply the correct subsampling for Cb/Cr.
  heif_chroma chroma = heif_chroma_undefined;
  heif_colorspace colourspace = heif_colorspace_undefined;
  (void) UncompressedImageCodec::get_heif_chroma_uncompressed(uncC, cmpd, &chroma, &colourspace, nullptr);

  // Track which cmpd indices already got a description from the uncC walk,
  // so we don't duplicate them when handling cpat.
  std::vector<bool> cmpd_index_has_description(cmpd_components.size(), false);

  // 1. uncC components — these get real planes after decode.
  for (const auto& uc : uncC->get_components()) {
    if (uc.component_index >= cmpd_components.size()) {
      continue; // malformed; skip
    }
    uint16_t component_type = cmpd_components[uc.component_index].component_type;

    heif_channel channel = map_uncompressed_component_to_channel(component_type);
    uint32_t plane_w = channel_width(img_width, chroma, channel);
    uint32_t plane_h = channel_height(img_height, chroma, channel);

    ComponentDescription desc;
    desc.component_id = mint_component_id();
    desc.channel = channel;
    desc.component_type = component_type;
    desc.datatype = unc_component_format_to_datatype(uc.component_format);
    desc.bit_depth = uc.component_bit_depth;
    desc.width = plane_w;
    desc.height = plane_h;
    desc.has_data_plane = true;
    add_component_description(std::move(desc));

    cmpd_index_has_description[uc.component_index] = true;
  }

  // 2. cpat reference components — one per UNIQUE cmpd_index that's
  // referenced by the pattern but doesn't have an uncC plane. Two pattern
  // pixels that share a cmpd_index share the same component id; the
  // BayerPattern only carries per-pixel gain, not per-pixel identity.
  if (auto cpat = get_property<Box_cpat>()) {
    for (const auto& pixel : cpat->get_pattern().pixels) {
      if (pixel.cmpd_index >= cmpd_components.size()) continue;
      if (cmpd_index_has_description[pixel.cmpd_index]) continue;

      uint16_t component_type = cmpd_components[pixel.cmpd_index].component_type;

      ComponentDescription desc;
      desc.component_id = mint_component_id();
      desc.channel = map_uncompressed_component_to_channel(component_type);
      desc.component_type = component_type;
      desc.has_data_plane = false;
      add_component_description(std::move(desc));

      cmpd_index_has_description[pixel.cmpd_index] = true;
    }
  }
}


ImageItem_uncompressed::ImageItem_uncompressed(HeifContext* ctx)
    : ImageItem(ctx)
{
  m_encoder = std::make_shared<Encoder_uncompressed>();
}


Result<std::shared_ptr<HeifPixelImage>> ImageItem_uncompressed::decode_compressed_image(const heif_decoding_options& options,
                                                                                bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                                std::set<heif_item_id> processed_ids) const
{
  std::shared_ptr<HeifPixelImage> img;

  std::vector<uint8_t> data;

  Error err;

  if (decode_tile_only) {
    err = UncompressedImageCodec::decode_uncompressed_image_tile(get_context(),
                                                                 get_id(),
                                                                 img,
                                                                 tile_x0, tile_y0);
  }
  else {
    err = UncompressedImageCodec::decode_uncompressed_image(get_context(),
                                                            get_id(),
                                                            img);
  }

  if (err) {
    return err;
  }
  else {
    return img;
  }
}


Result<Encoder::CodedImageData> ImageItem_uncompressed::encode(const std::shared_ptr<HeifPixelImage>& src_image,
                                                                 heif_encoder* encoder,
                                                                 const heif_encoding_options& options,
                                                                 heif_image_input_class input_class)
{
  Result<std::unique_ptr<const unc_encoder>> uncEncoder = unc_encoder_factory::get_unc_encoder(src_image, options);
  if (!uncEncoder) {
    return {uncEncoder.error()};
  }

  return (*uncEncoder)->encode(src_image, options);
}


Result<std::shared_ptr<ImageItem_uncompressed>> ImageItem_uncompressed::add_unci_item(HeifContext* ctx,
                                                                                      const heif_unci_image_parameters* parameters,
                                                                                      const heif_encoding_options* encoding_options,
                                                                                      const std::shared_ptr<const HeifPixelImage>& prototype)
{
  assert(encoding_options != nullptr);

  // Resolve effective unci parameters: the direct argument takes precedence; otherwise
  // fall back to encoding_options->unci_parameters. At least one must be non-null.

  if (parameters == nullptr &&
      (encoding_options->version < 8 || encoding_options->unci_parameters == nullptr)) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value,
                 "heif_context_add_empty_unci_image: either the 'parameters' argument or "
                 "heif_encoding_options::unci_parameters must be non-null."};
  }

  if (parameters == nullptr) {
    parameters = encoding_options->unci_parameters;
  }

  // Check input parameters

  if (parameters->image_width % parameters->tile_width != 0 ||
      parameters->image_height % parameters->tile_height != 0) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value,
                 "ISO 23001-17 image size must be an integer multiple of the tile size."};
  }


  Result<std::unique_ptr<const unc_encoder>> uncEncoder = unc_encoder_factory::get_unc_encoder(prototype, *encoding_options);
  if (!uncEncoder) {
    return {uncEncoder.error()};
  }


  // Create 'unci' Item

  auto file = ctx->get_heif_file();

  auto unci_id_result = ctx->get_heif_file()->add_new_image(fourcc("unci"));
  if (!unci_id_result) {
    return unci_id_result.error();
  }
  heif_item_id unci_id = *unci_id_result;
  auto unci_image = std::make_shared<ImageItem_uncompressed>(ctx, unci_id);
  unci_image->set_resolution(parameters->image_width, parameters->image_height);
  unci_image->m_unc_encoder = std::move(*uncEncoder);
  unci_image->m_tile_encoding_options = *encoding_options;
  unci_image->m_tile_encoding_options.image_orientation = heif_orientation_normal;

  ctx->insert_image_item(unci_id, unci_image);


  // Generate headers

  // --- generate configuration property boxes

  auto uncC = unci_image->m_unc_encoder->get_uncC();

  uncC->set_number_of_tile_columns(parameters->image_width / parameters->tile_width);
  uncC->set_number_of_tile_rows(parameters->image_height / parameters->tile_height);

  unci_image->add_property(uncC, true);
  if (!uncC->is_minimized()) {
    unci_image->add_property(unci_image->m_unc_encoder->get_cmpd(), true);
  }


  // Add cpat property if Bayer pattern is set
  if (unci_image->m_unc_encoder->get_cpat()) {
    unci_image->add_property(unci_image->m_unc_encoder->get_cpat(), true);
  }

  // Add `ispe` property

  auto ispe = std::make_shared<Box_ispe>();
  ispe->set_size(static_cast<uint32_t>(parameters->image_width),
                 static_cast<uint32_t>(parameters->image_height));
  unci_image->add_property(ispe, true);

  if (parameters->compression != heif_unci_compression_off) {
    auto cmpC = std::make_shared<Box_cmpC>();
    cmpC->set_compression_type(unci_compression_to_fourcc(parameters->compression));
    cmpC->set_compressed_unit_type(heif_cmpC_compressed_unit_type_image_tile);

    unci_image->add_property(cmpC, true);

    auto icef = std::make_shared<Box_icef>();
    unci_image->add_property_without_deduplication(icef, true); // icef is empty. A normal add_property() would lead to a wrong deduplication.
  }

  // Add transformative properties

  ctx->get_heif_file()->add_orientation_properties(unci_id, encoding_options->image_orientation);


  // Create empty image. If we use compression, we append the data piece by piece.

  if (parameters->compression == heif_unci_compression_off) {
    uint64_t tile_size = unci_image->m_unc_encoder->compute_tile_data_size_bytes(parameters->image_width / uncC->get_number_of_tile_columns(),
                                                                                 parameters->image_height / uncC->get_number_of_tile_rows());

    std::vector<uint8_t> dummydata;
    dummydata.resize(tile_size);

    uint32_t nTiles = (parameters->image_width / parameters->tile_width) * (parameters->image_height / parameters->tile_height);

    for (uint64_t i = 0; i < nTiles; i++) {
      const int construction_method = 0; // 0=mdat 1=idat
      file->append_iloc_data(unci_id, dummydata, construction_method);
    }
  }

  // Set Brands
  //ctx->get_heif_file()->set_brand(heif_compression_uncompressed, unci_image->is_miaf_compatible());

  return {unci_image};
}


Error ImageItem_uncompressed::add_image_tile(uint32_t tile_x, uint32_t tile_y, const std::shared_ptr<const HeifPixelImage>& image, bool save_alpha)
{
  std::shared_ptr<Box_uncC> uncC = get_property<Box_uncC>();
  assert(uncC);

  uint32_t tile_width = image->get_width();
  uint32_t tile_height = image->get_height();

  uint32_t tile_idx = tile_y * uncC->get_number_of_tile_columns() + tile_x;

  if (tile_y >= uncC->get_number_of_tile_rows() ||
      tile_x >= uncC->get_number_of_tile_columns()) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value,
                 "tile_x and/or tile_y are out of range."};
  }


  if (image->has_alpha() && !save_alpha) {
    // TODO: drop alpha
  }

  Result<std::vector<uint8_t>> codedBitstreamResult = m_unc_encoder->encode_tile(image);
  if (!codedBitstreamResult) {
    return codedBitstreamResult.error();
  }

  std::shared_ptr<Box_cmpC> cmpC = get_property<Box_cmpC>();
  std::shared_ptr<Box_icef> icef = get_property<Box_icef>();

  if (!icef || !cmpC) {
    assert(!icef);
    assert(!cmpC);

    // uncompressed

    uint64_t tile_data_size = m_unc_encoder->compute_tile_data_size_bytes(tile_width, tile_height);

    get_file()->replace_iloc_data(get_id(), tile_idx * tile_data_size, *codedBitstreamResult, 0);
  }
  else {
    const std::vector<uint8_t>& raw_data = *codedBitstreamResult;

    auto compressed = compress_unci_fourcc(cmpC->get_compression_type(),
                                            raw_data.data(), raw_data.size());
    if (!compressed) {
      return compressed.error();
    }

    get_file()->append_iloc_data(get_id(), *compressed, 0);

    Box_icef::CompressedUnitInfo unit_info;
    unit_info.unit_offset = m_next_tile_write_pos;
    unit_info.unit_size = compressed->size();
    icef->set_component(tile_idx, unit_info);

    m_next_tile_write_pos += compressed->size();
  }

  return Error::Ok;
}


void ImageItem_uncompressed::get_tile_size(uint32_t& w, uint32_t& h) const
{
  auto ispe = get_property<Box_ispe>();
  auto uncC = get_property<Box_uncC>();

  if (!ispe || !uncC) {
    w = h = 0;
  }
  else {
    w = ispe->get_width() / uncC->get_number_of_tile_columns();
    h = ispe->get_height() / uncC->get_number_of_tile_rows();
  }
}


heif_image_tiling ImageItem_uncompressed::get_heif_image_tiling() const
{
  heif_image_tiling tiling{};

  auto ispe = get_property<Box_ispe>();
  auto uncC = get_property<Box_uncC>();
  assert(ispe && uncC);

  tiling.num_columns = uncC->get_number_of_tile_columns();
  tiling.num_rows = uncC->get_number_of_tile_rows();

  tiling.tile_width = ispe->get_width() / tiling.num_columns;
  tiling.tile_height = ispe->get_height() / tiling.num_rows;

  tiling.image_width = ispe->get_width();
  tiling.image_height = ispe->get_height();
  tiling.number_of_extra_dimensions = 0;

  return tiling;
}

Result<std::shared_ptr<Decoder>> ImageItem_uncompressed::get_decoder() const
{
  return {m_decoder};
}

std::shared_ptr<Encoder> ImageItem_uncompressed::get_encoder() const
{
  return m_encoder;
}

Error ImageItem_uncompressed::initialize_decoder()
{
  std::shared_ptr<Box_cmpd> cmpd = get_property<Box_cmpd>();
  std::shared_ptr<Box_uncC> uncC = get_property<Box_uncC>();
  std::shared_ptr<Box_ispe> ispe = get_property<Box_ispe>();

  if (!uncC) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "No 'uncC' box found."};
  }

  if (!ispe) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "No 'ispe' box found for uncompressed image item."};
  }

  // libheif's pixel allocation and processing (rotate/mirror) only support
  // bit depths up to 128 per component. Reject larger values up front so they
  // surface as a clean Unsupported_feature error rather than failing inside
  // HeifPixelImage::ComponentStorage::alloc().
  for (const auto& component : uncC->get_components()) {
    if (component.component_bit_depth > 128) {
      std::stringstream sstr;
      sstr << "Uncompressed image with " << component.component_bit_depth
           << " bits per component is not supported.";
      return Error{heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_bit_depth,
                   sstr.str()};
    }
  }

  m_decoder = std::make_shared<Decoder_uncompressed>(uncC, cmpd, ispe);

  return Error::Ok;
}

void ImageItem_uncompressed::set_decoder_input_data()
{
  DataExtent extent;
  extent.set_from_image_item(get_context()->get_heif_file(), get_id());

  m_decoder->set_data_extent(std::move(extent));
}


bool ImageItem_uncompressed::has_coded_alpha_channel() const
{
  return m_decoder->has_alpha_component();
}

heif_brand2 ImageItem_uncompressed::get_compatible_brand() const
{
  return 0; // TODO: not clear to me what to use
}
