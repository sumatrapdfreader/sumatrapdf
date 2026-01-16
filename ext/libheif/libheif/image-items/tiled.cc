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

#include "tiled.h"
#include "context.h"
#include "file.h"
#include <algorithm>
#include "security_limits.h"
#include "codecs/hevc_dec.h"
#include "api_structs.h"


static uint64_t readvec(const std::vector<uint8_t>& data, size_t& ptr, int len)
{
  uint64_t val = 0;
  while (len--) {
    val <<= 8;
    val |= data[ptr++];
  }

  return val;
}


uint64_t number_of_tiles(const heif_tiled_image_parameters& params)
{
  uint64_t nTiles = nTiles_h(params) * static_cast<uint64_t>(nTiles_v(params));

  for (int i = 0; i < params.number_of_extra_dimensions; i++) {
    // We only support up to 8 extra dimensions
    if (i == 8) {
      break;
    }

    nTiles *= params.extra_dimensions[i];
  }

  return nTiles;
}


uint32_t nTiles_h(const heif_tiled_image_parameters& params)
{
  return (params.image_width + params.tile_width - 1) / params.tile_width;
}


uint32_t nTiles_v(const heif_tiled_image_parameters& params)
{
  return (params.image_height + params.tile_height - 1) / params.tile_height;
}


void Box_tilC::init_heif_tiled_image_parameters(heif_tiled_image_parameters& params)
{
  params.version = 1;

  params.image_width = 0;
  params.image_height = 0;
  params.tile_width = 0;
  params.tile_height = 0;
  params.compression_format_fourcc = 0;
  params.offset_field_length = 40;
  params.size_field_length = 24;
  params.number_of_extra_dimensions = 0;

  for (uint32_t& dim : params.extra_dimensions) {
    dim = 0;
  }

  params.tiles_are_sequential = false;
}


void Box_tilC::derive_box_version()
{
  set_version(0);

  uint8_t flags = 0;

  switch (m_parameters.offset_field_length) {
    case 32:
      flags |= 0;
      break;
    case 40:
      flags |= 0x01;
      break;
    case 48:
      flags |= 0x02;
      break;
    case 64:
      flags |= 0x03;
      break;
    default:
      assert(false); // TODO: return error
  }

  switch (m_parameters.size_field_length) {
    case 0:
      flags |= 0;
      break;
    case 24:
      flags |= 0x04;
      break;
    case 32:
      flags |= 0x08;
      break;
    case 64:
      flags |= 0x0c;
      break;
    default:
      assert(false); // TODO: return error
  }

  if (m_parameters.tiles_are_sequential) {
    flags |= 0x10;
  }

  set_flags(flags);
}


Error Box_tilC::write(StreamWriter& writer) const
{
  assert(m_parameters.version == 1);

  size_t box_start = reserve_box_header_space(writer);

  if (m_parameters.number_of_extra_dimensions > 8) {
    assert(false); // currently not supported
  }

  writer.write32(m_parameters.tile_width);
  writer.write32(m_parameters.tile_height);
  writer.write32(m_parameters.compression_format_fourcc);

  writer.write8(m_parameters.number_of_extra_dimensions);

  for (int i = 0; i < m_parameters.number_of_extra_dimensions; i++) {
    writer.write32(m_parameters.extra_dimensions[i]);
  }

  auto& tile_properties = m_children;
  if (tile_properties.size() > 255) {
    return {heif_error_Encoding_error,
            heif_suberror_Unspecified,
            "Cannot write more than 255 tile properties in tilC header"};
  }

  writer.write8(static_cast<uint8_t>(tile_properties.size()));
  for (const auto& property : tile_properties) {
    property->write(writer);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_tilC::dump(Indent& indent) const
{
  std::ostringstream sstr;

  sstr << BoxHeader::dump(indent);

  sstr << indent << "version: " << ((int) get_version()) << "\n"
       //<< indent << "image size: " << m_parameters.image_width << "x" << m_parameters.image_height << "\n"
       << indent << "tile size: " << m_parameters.tile_width << "x" << m_parameters.tile_height << "\n"
       << indent << "compression: " << fourcc_to_string(m_parameters.compression_format_fourcc) << "\n"
       << indent << "tiles are sequential: " << (m_parameters.tiles_are_sequential ? "yes" : "no") << "\n"
       << indent << "offset field length: " << ((int) m_parameters.offset_field_length) << " bits\n"
       << indent << "size field length: " << ((int) m_parameters.size_field_length) << " bits\n"
       << indent << "number of extra dimensions: " << ((int) m_parameters.number_of_extra_dimensions) << "\n";

  sstr << indent << "tile properties:\n"
       << dump_children(indent, true);

  return sstr.str();

}


Error Box_tilC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  // Note: actually, we should allow 0 only, but there are a few images around that use version 1.
  if (get_version() > 1) {
    std::stringstream sstr;
    sstr << "'tili' image version " << ((int) get_version()) << " is not implemented yet";

    return {heif_error_Unsupported_feature,
            heif_suberror_Unsupported_data_version,
            sstr.str()};
  }

  m_parameters.version = get_version();

  uint32_t flags = get_flags();

  switch (flags & 0x03) {
    case 0:
      m_parameters.offset_field_length = 32;
      break;
    case 1:
      m_parameters.offset_field_length = 40;
      break;
    case 2:
      m_parameters.offset_field_length = 48;
      break;
    case 3:
      m_parameters.offset_field_length = 64;
      break;
  }

  switch (flags & 0x0c) {
    case 0x00:
      m_parameters.size_field_length = 0;
      break;
    case 0x04:
      m_parameters.size_field_length = 24;
      break;
    case 0x08:
      m_parameters.size_field_length = 32;
      break;
    case 0x0c:
      m_parameters.size_field_length = 64;
      break;
  }

  m_parameters.tiles_are_sequential = !!(flags & 0x10);


  m_parameters.tile_width = range.read32();
  m_parameters.tile_height = range.read32();
  m_parameters.compression_format_fourcc = range.read32();

  if (m_parameters.tile_width == 0 || m_parameters.tile_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Unspecified,
            "Tile with zero width or height."};
  }


  // --- extra dimensions

  m_parameters.number_of_extra_dimensions = range.read8();

  for (int i = 0; i < m_parameters.number_of_extra_dimensions; i++) {
    uint32_t size = range.read32();

    if (size == 0) {
      return {heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "'tili' extra dimension may not be zero."};
    }

    if (i < 8) {
      m_parameters.extra_dimensions[i] = size;
    }
    else {
      // TODO: error: too many dimensions (not supported)
    }
  }

  // --- read tile properties

  // Check version for backwards compatibility with old format.
  // TODO: remove when spec is final and old test images have been converted
  if (get_version() == 0) {
    uint8_t num_properties = range.read8();

    Error error = read_children(range, num_properties, limits);
    if (error) {
      return error;
    }
  }

  return range.get_error();
}


Error TiledHeader::set_parameters(const heif_tiled_image_parameters& params)
{
  m_parameters = params;

  auto max_tiles = heif_get_global_security_limits()->max_number_of_tiles;

  if (max_tiles && number_of_tiles(params) > max_tiles) {
    return {heif_error_Unsupported_filetype,
            heif_suberror_Security_limit_exceeded,
            "Number of tiles exceeds security limit"};
  }

  m_offsets.resize(number_of_tiles(params));

  for (auto& tile: m_offsets) {
    tile.offset = TILD_OFFSET_NOT_LOADED;
  }

  return Error::Ok;
}


Error TiledHeader::read_full_offset_table(const std::shared_ptr<HeifFile>& file, heif_item_id tild_id, const heif_security_limits* limits)
{
  auto max_tiles = heif_get_global_security_limits()->max_number_of_tiles;

  uint64_t nTiles = number_of_tiles(m_parameters);
  if (max_tiles && nTiles > max_tiles) {
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            "Number of tiles exceeds security limit."};
  }

  return read_offset_table_range(file, tild_id, 0, nTiles);
}


Error TiledHeader::read_offset_table_range(const std::shared_ptr<HeifFile>& file, heif_item_id tild_id,
                                           uint64_t start, uint64_t end)
{
  const Error eofError(heif_error_Invalid_input,
                       heif_suberror_Unspecified,
                       "Tili header data incomplete");

  std::vector<uint8_t> data;



  // --- load offsets

  size_t size_to_read = (end - start) * (m_parameters.offset_field_length + m_parameters.size_field_length) / 8;
  size_t start_offset = start * (m_parameters.offset_field_length + m_parameters.size_field_length) / 8;

  // TODO: when we request a file range from the stream reader, it may return a larger range.
  //       We should then also use this larger range to read more table entries.
  //       But this is not easy since our data may span several iloc extents and we have to map this back to item addresses.
  //       Maybe it is easier to just ignore the extra data and rely on the stream read to cache this data.

  Error err = file->append_data_from_iloc(tild_id, data, start_offset, size_to_read);
  if (err) {
    return err;
  }

  size_t idx = 0;
  for (uint64_t i = start; i < end; i++) {
    m_offsets[i].offset = readvec(data, idx, m_parameters.offset_field_length / 8);

    if (m_parameters.size_field_length) {
      assert(m_parameters.size_field_length <= 32);
      m_offsets[i].size = static_cast<uint32_t>(readvec(data, idx, m_parameters.size_field_length / 8));
    }

    // printf("[%zu] : offset/size: %zu %d\n", i, m_offsets[i].offset, m_offsets[i].size);
  }

  return Error::Ok;
}


size_t TiledHeader::get_header_size() const
{
  assert(m_header_size);
  return m_header_size;
}


uint32_t TiledHeader::get_offset_table_entry_size() const
{
  return (m_parameters.offset_field_length + m_parameters.size_field_length) / 8;
}


std::pair<uint32_t, uint32_t> TiledHeader::get_tile_offset_table_range_to_read(uint32_t idx, uint32_t nEntries) const
{
  uint32_t start = idx;
  uint32_t end = idx+1;

  while (end < m_offsets.size() && end - idx < nEntries && m_offsets[end].offset == TILD_OFFSET_NOT_LOADED) {
    end++;
  }

  while (start > 0 && idx - start < nEntries && m_offsets[start-1].offset == TILD_OFFSET_NOT_LOADED) {
    start--;
  }

  // try to fill the smaller hole

  if (end - start > nEntries) {
    if (idx - start < end - idx) {
      end = start + nEntries;
    }
    else {
      start = end - nEntries;
    }
  }

  return {start, end};
}


void TiledHeader::set_tild_tile_range(uint32_t tile_x, uint32_t tile_y, uint64_t offset, uint32_t size)
{
  uint64_t idx = uint64_t{tile_y} * nTiles_h(m_parameters) + tile_x;
  m_offsets[idx].offset = offset;
  m_offsets[idx].size = size;
}


template<typename I>
void writevec(uint8_t* data, size_t& idx, I value, int len)
{
  for (int i = 0; i < len; i++) {
    data[idx + i] = static_cast<uint8_t>((value >> (len - 1 - i) * 8) & 0xFF);
  }

  idx += len;
}


std::vector<uint8_t> TiledHeader::write_offset_table()
{
  uint64_t nTiles = number_of_tiles(m_parameters);

  int offset_entry_size = (m_parameters.offset_field_length + m_parameters.size_field_length) / 8;
  uint64_t size = nTiles * offset_entry_size;

  std::vector<uint8_t> data;
  data.resize(size);

  size_t idx = 0;

  for (const auto& offset: m_offsets) {
    writevec(data.data(), idx, offset.offset, m_parameters.offset_field_length / 8);

    if (m_parameters.size_field_length != 0) {
      writevec(data.data(), idx, offset.size, m_parameters.size_field_length / 8);
    }
  }

  assert(idx == data.size());

  m_header_size = data.size();

  return data;
}


std::string TiledHeader::dump() const
{
  std::stringstream sstr;

  sstr << "offsets: ";

  // TODO

  for (const auto& offset: m_offsets) {
    sstr << offset.offset << ", size: " << offset.size << "\n";
  }

  return sstr.str();
}


ImageItem_Tiled::ImageItem_Tiled(HeifContext* ctx)
        : ImageItem(ctx)
{
}


ImageItem_Tiled::ImageItem_Tiled(HeifContext* ctx, heif_item_id id)
        : ImageItem(ctx, id)
{
}


heif_compression_format ImageItem_Tiled::get_compression_format() const
{
  return compression_format_from_fourcc_infe_type(m_tild_header.get_parameters().compression_format_fourcc);
}


Error ImageItem_Tiled::initialize_decoder()
{
  auto heif_file = get_context()->get_heif_file();

  auto tilC_box = get_property<Box_tilC>();
  if (!tilC_box) {
    return {heif_error_Invalid_input,
            heif_suberror_Unspecified,
            "Tiled image without 'tilC' property box."};
  }

  auto ispe_box = get_property<Box_ispe>();
  if (!ispe_box) {
    return {heif_error_Invalid_input,
            heif_suberror_Unspecified,
            "Tiled image without 'ispe' property box."};
  }

  heif_tiled_image_parameters parameters = tilC_box->get_parameters();
  parameters.image_width = ispe_box->get_width();
  parameters.image_height = ispe_box->get_height();

  if (parameters.image_width == 0 || parameters.image_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Unspecified,
            "'tili' image with zero width or height."};
  }

  if (Error err = m_tild_header.set_parameters(parameters)) {
    return err;
  }


  // --- create a dummy image item for decoding tiles

  heif_compression_format format = compression_format_from_fourcc_infe_type(m_tild_header.get_parameters().compression_format_fourcc);
  m_tile_item = ImageItem::alloc_for_compression_format(get_context(), format);

  // For backwards compatibility: copy over properties from `tili` item.
  // TODO: remove when spec is final and old test images have been converted
  if (tilC_box->get_version() == 1) {
    auto propertiesResult = get_properties();
    if (!propertiesResult) {
      return propertiesResult.error();
    }

    m_tile_item->set_properties(*propertiesResult);
  }
  else {
    // --- This is the new method

    // Synthesize an ispe box if there was none in the file

    auto tile_properties = tilC_box->get_all_child_boxes();

    bool have_ispe = false;
    for (const auto& property : tile_properties) {
      if (property->get_short_type() == fourcc("ispe")) {
        have_ispe = true;
        break;
      }
    }

    if (!have_ispe) {
      auto ispe = std::make_shared<Box_ispe>();
      ispe->set_size(parameters.tile_width, parameters.tile_height);
      tile_properties.emplace_back(std::move(ispe));
    }

    m_tile_item->set_properties(tile_properties);
  }

  m_tile_decoder = Decoder::alloc_for_infe_type(m_tile_item.get());
  if (!m_tile_decoder) {
    return {heif_error_Unsupported_feature,
            heif_suberror_Unsupported_codec,
            "'tili' image with unsupported compression format."};
  }

  if (m_preload_offset_table) {
    if (Error err = m_tild_header.read_full_offset_table(heif_file, get_id(), get_context()->get_security_limits())) {
      return err;
    }
  }


  return Error::Ok;
}


Result<std::shared_ptr<ImageItem_Tiled>>
ImageItem_Tiled::add_new_tiled_item(HeifContext* ctx, const heif_tiled_image_parameters* parameters,
                                    const heif_encoder* encoder)
{
  auto max_tild_tiles = ctx->get_security_limits()->max_number_of_tiles;
  if (max_tild_tiles && number_of_tiles(*parameters) > max_tild_tiles) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Security_limit_exceeded,
                 "Number of tiles exceeds security limit."};
  }


  // Create 'tili' Item

  auto file = ctx->get_heif_file();

  heif_item_id tild_id = ctx->get_heif_file()->add_new_image(fourcc("tili"));
  auto tild_image = std::make_shared<ImageItem_Tiled>(ctx, tild_id);
  tild_image->set_resolution(parameters->image_width, parameters->image_height);
  ctx->insert_image_item(tild_id, tild_image);

  // Create tilC box

  auto tilC_box = std::make_shared<Box_tilC>();
  tilC_box->set_parameters(*parameters);
  tilC_box->set_compression_format(encoder->plugin->compression_format);
  tild_image->add_property(tilC_box, true);

  // Create header + offset table

  TiledHeader tild_header;
  tild_header.set_parameters(*parameters);
  tild_header.set_compression_format(encoder->plugin->compression_format);

  std::vector<uint8_t> header_data = tild_header.write_offset_table();

  const int construction_method = 0; // 0=mdat 1=idat
  file->append_iloc_data(tild_id, header_data, construction_method);


  if (parameters->image_width > 0xFFFFFFFF || parameters->image_height > 0xFFFFFFFF) {
    return {Error(heif_error_Usage_error, heif_suberror_Invalid_image_size,
                  "'ispe' only supports image sized up to 4294967295 pixels per dimension")};
  }

  // Add ISPE property
  auto ispe = std::make_shared<Box_ispe>();
  ispe->set_size(static_cast<uint32_t>(parameters->image_width),
                 static_cast<uint32_t>(parameters->image_height));
  tild_image->add_property(ispe, true);

#if 0
  // TODO

  // Add PIXI property (copy from first tile)
  auto pixi = m_heif_file->get_property<Box_pixi>(tile_ids[0]);
  m_heif_file->add_property(grid_id, pixi, true);
#endif

  tild_image->set_tild_header(tild_header);
  tild_image->set_next_tild_position(header_data.size());

  // Set Brands
  //m_heif_file->set_brand(encoder->plugin->compression_format,
  //                       out_grid_image->is_miaf_compatible());

  return {tild_image};
}


Error ImageItem_Tiled::add_image_tile(uint32_t tile_x, uint32_t tile_y,
                                     const std::shared_ptr<HeifPixelImage>& image,
                                     heif_encoder* encoder)
{
  auto item = ImageItem::alloc_for_compression_format(get_context(), encoder->plugin->compression_format);

  heif_encoding_options* options = heif_encoding_options_alloc(); // TODO: should this be taken from heif_context_add_tiled_image() ?

  Result<std::shared_ptr<HeifPixelImage>> colorConversionResult;
  colorConversionResult = item->get_encoder()->convert_colorspace_for_encoding(image, encoder,
                                                                               options->output_nclx_profile,
                                                                               &options->color_conversion_options,
                                                                               get_context()->get_security_limits());
  if (!colorConversionResult) {
    heif_encoding_options_free(options);
    return colorConversionResult.error();
  }

  std::shared_ptr<HeifPixelImage> colorConvertedImage = *colorConversionResult;

  Result<Encoder::CodedImageData> encodeResult = item->encode_to_bitstream_and_boxes(colorConvertedImage, encoder, *options, heif_image_input_class_normal); // TODO (other than JPEG)
  heif_encoding_options_free(options);

  if (!encodeResult) {
    return encodeResult.error();
  }

  const int construction_method = 0; // 0=mdat 1=idat
  get_file()->append_iloc_data(get_id(), encodeResult->bitstream, construction_method);

  auto& header = m_tild_header;

  if (image->get_width() != header.get_parameters().tile_width ||
      image->get_height() != header.get_parameters().tile_height) {
    return {heif_error_Usage_error,
            heif_suberror_Unspecified,
            "Tile image size does not match the specified tile size."};
  }

  uint64_t offset = get_next_tild_position();
  size_t dataSize = encodeResult->bitstream.size();
  if (dataSize > 0xFFFFFFFF) {
    return {heif_error_Encoding_error, heif_suberror_Unspecified, "Compressed tile size exceeds maximum tile size."};
  }
  header.set_tild_tile_range(tile_x, tile_y, offset, static_cast<uint32_t>(dataSize));
  set_next_tild_position(offset + encodeResult->bitstream.size());

  auto tilC = get_property<Box_tilC>();
  assert(tilC);

  std::vector<std::shared_ptr<Box>>& tile_properties = tilC->get_tile_properties();

  for (auto& propertyBox : encodeResult->properties) {

    // we do not have to save ispe boxes in the tile properties as this is automatically synthesized

    if (propertyBox->get_short_type() == fourcc("ispe")) {
      continue;
    }

    // skip properties that exist already

    bool exists = std::any_of(tile_properties.begin(),
                              tile_properties.end(),
                              [&propertyBox](const std::shared_ptr<Box>& p) { return p->get_short_type() == propertyBox->get_short_type();});
    if (exists) {
      continue;
    }

    tile_properties.emplace_back(propertyBox);

    // some tile properties are also added to the tili image

    switch (propertyBox->get_short_type()) {
      case fourcc("pixi"):
        get_file()->add_property(get_id(), propertyBox, propertyBox->is_essential());
        break;
    }
  }

  //get_file()->set_brand(encoder->plugin->compression_format,
  //                      true); // TODO: out_grid_image->is_miaf_compatible());

  return Error::Ok;
}


void ImageItem_Tiled::process_before_write()
{
  // overwrite offsets

  const int construction_method = 0; // 0=mdat 1=idat

  std::vector<uint8_t> header_data = m_tild_header.write_offset_table();
  get_file()->replace_iloc_data(get_id(), 0, header_data, construction_method);
}


Result<std::shared_ptr<HeifPixelImage>>
ImageItem_Tiled::decode_compressed_image(const heif_decoding_options& options,
                                         bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                         std::set<heif_item_id> processed_ids) const
{
  if (decode_tile_only) {
    return decode_grid_tile(options, tile_x0, tile_y0);
  }
  else {
    return Error{heif_error_Unsupported_feature, heif_suberror_Unspecified,
                 "'tili' images can only be access per tile"};
  }
}


Error ImageItem_Tiled::append_compressed_tile_data(std::vector<uint8_t>& data, uint32_t tx, uint32_t ty) const
{
  uint32_t idx = (uint32_t) (ty * nTiles_h(m_tild_header.get_parameters()) + tx);

  if (!m_tild_header.is_tile_offset_known(idx)) {
    Error err = const_cast<ImageItem_Tiled*>(this)->load_tile_offset_entry(idx);
    if (err) {
      return err;
    }
  }

  uint64_t offset = m_tild_header.get_tile_offset(idx);
  uint64_t size = m_tild_header.get_tile_size(idx);

  Error err = get_file()->append_data_from_iloc(get_id(), data, offset, size);
  if (err.error_code) {
    return err;
  }

  return Error::Ok;
}


Result<DataExtent>
ImageItem_Tiled::get_compressed_data_for_tile(uint32_t tx, uint32_t ty) const
{
  // --- get compressed data

  Error err = m_tile_item->initialize_decoder();
  if (err) {
    return err;
  }

  Result<std::vector<uint8_t>> dataResult = m_tile_item->read_bitstream_configuration_data();
  if (!dataResult) {
    return dataResult.error();
  }

  std::vector<uint8_t> data = std::move(*dataResult);
  err = append_compressed_tile_data(data, tx, ty);
  if (err) {
    return err;
  }

  // --- decode

  DataExtent extent;
  extent.m_raw = data;

  return extent;
}


Result<std::shared_ptr<HeifPixelImage>>
ImageItem_Tiled::decode_grid_tile(const heif_decoding_options& options, uint32_t tx, uint32_t ty) const
{
  Result<DataExtent> extentResult = get_compressed_data_for_tile(tx, ty);
  if (!extentResult) {
    return extentResult.error();
  }

  m_tile_decoder->set_data_extent(std::move(*extentResult));

  return m_tile_decoder->decode_single_frame_from_compressed_data(options,
                                                                  get_context()->get_security_limits());
}


Error ImageItem_Tiled::load_tile_offset_entry(uint32_t idx)
{
  uint32_t nEntries = mReadChunkSize_bytes / m_tild_header.get_offset_table_entry_size();
  std::pair<uint32_t, uint32_t> range = m_tild_header.get_tile_offset_table_range_to_read(idx, nEntries);

  return m_tild_header.read_offset_table_range(get_file(), get_id(), range.first, range.second);
}


heif_image_tiling ImageItem_Tiled::get_heif_image_tiling() const
{
  heif_image_tiling tiling{};

  tiling.num_columns = nTiles_h(m_tild_header.get_parameters());
  tiling.num_rows = nTiles_v(m_tild_header.get_parameters());

  tiling.tile_width = m_tild_header.get_parameters().tile_width;
  tiling.tile_height = m_tild_header.get_parameters().tile_height;

  tiling.image_width = m_tild_header.get_parameters().image_width;
  tiling.image_height = m_tild_header.get_parameters().image_height;
  tiling.number_of_extra_dimensions = m_tild_header.get_parameters().number_of_extra_dimensions;
  for (int i = 0; i < std::min(tiling.number_of_extra_dimensions, uint8_t(8)); i++) {
    tiling.extra_dimension_size[i] = m_tild_header.get_parameters().extra_dimensions[i];
  }

  return tiling;
}


void ImageItem_Tiled::get_tile_size(uint32_t& w, uint32_t& h) const
{
  w = m_tild_header.get_parameters().tile_width;
  h = m_tild_header.get_parameters().tile_height;
}


Error ImageItem_Tiled::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  uint32_t tx=0, ty=0; // TODO: find a tile that is defined.

  Result<DataExtent> extentResult = get_compressed_data_for_tile(tx, ty);
  if (!extentResult) {
    return extentResult.error();
  }

  m_tile_decoder->set_data_extent(std::move(*extentResult));

  Error err = m_tile_decoder->get_coded_image_colorspace(out_colorspace, out_chroma);
  if (err) {
    return err;
  }

  postprocess_coded_image_colorspace(out_colorspace, out_chroma);

  return Error::Ok;
}


int ImageItem_Tiled::get_luma_bits_per_pixel() const
{
  DataExtent any_tile_extent;
  append_compressed_tile_data(any_tile_extent.m_raw, 0,0); // TODO: use tile that is already loaded
  m_tile_decoder->set_data_extent(std::move(any_tile_extent));

  return m_tile_decoder->get_luma_bits_per_pixel();
}

int ImageItem_Tiled::get_chroma_bits_per_pixel() const
{
  DataExtent any_tile_extent;
  append_compressed_tile_data(any_tile_extent.m_raw, 0,0); // TODO: use tile that is already loaded
  m_tile_decoder->set_data_extent(std::move(any_tile_extent));

  return m_tile_decoder->get_chroma_bits_per_pixel();
}

heif_brand2 ImageItem_Tiled::get_compatible_brand() const
{
  return 0;

  // TODO: it is not clear to me what brand to use here.

  /*
  switch (m_tild_header.get_parameters().compression_format_fourcc) {
    case heif_compression_HEVC:
      return heif_brand2_heic;
  }
   */
}
