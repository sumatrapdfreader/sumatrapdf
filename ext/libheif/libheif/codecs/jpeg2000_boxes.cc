/*
 * HEIF JPEG 2000 codec.
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
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

#include "jpeg2000_boxes.h"
#include "api_structs.h"
#include <cstdint>
#include <iostream>
#include <cstdio>

static const uint16_t JPEG2000_CAP_MARKER = 0xFF50;
static const uint16_t JPEG2000_SIZ_MARKER = 0xFF51;
static const uint16_t JPEG2000_SOC_MARKER = 0xFF4F;


Error Box_cdef::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint16_t channel_count = range.read16();

  if (limits->max_components && channel_count > limits->max_components) {
    std::stringstream sstr;
    sstr << "cdef box wants to define " << channel_count << " JPEG-2000 channels, but the security limit is set to "
         << limits->max_components << " components";
    return {heif_error_Invalid_input,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};
  }

  if (channel_count > range.get_remaining_bytes() / 6) {
    std::stringstream sstr;
    sstr << "cdef box wants to define " << channel_count << " JPEG-2000 channels, but file only contains "
         << range.get_remaining_bytes() / 6 << " components";
    return {heif_error_Invalid_input,
            heif_suberror_End_of_data,
            sstr.str()};
  }

  m_channels.resize(channel_count);

  for (uint16_t i = 0; i < channel_count && !range.error() && !range.eof(); i++) {
    Channel channel;
    channel.channel_index = range.read16();
    channel.channel_type = range.read16();
    channel.channel_association = range.read16();
    m_channels[i] = channel;
  }

  return range.get_error();
}

std::string Box_cdef::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  for (const auto& channel : m_channels) {
    sstr << indent << "channel_index: " << channel.channel_index
         << ", channel_type: " << channel.channel_type
         << ", channel_association: " << channel.channel_association << "\n";
  }

  return sstr.str();
}


Error Box_cdef::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write16((uint16_t) m_channels.size());
  for (const auto& channel : m_channels) {
    writer.write16(channel.channel_index);
    writer.write16(channel.channel_type);
    writer.write16(channel.channel_association);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


void Box_cdef::set_channels(heif_colorspace colorspace)
{
  // TODO - Check for the presence of a cmap box which specifies channel indices.

  const uint16_t TYPE_COLOR = 0;
  const uint16_t ASOC_GREY = 1;
  const uint16_t ASOC_RED = 1;
  const uint16_t ASOC_GREEN = 2;
  const uint16_t ASOC_BLUE = 3;
  const uint16_t ASOC_Y = 1;
  const uint16_t ASOC_Cb = 2;
  const uint16_t ASOC_Cr = 3;

  switch (colorspace) {
    case heif_colorspace_RGB:
      m_channels.push_back({0, TYPE_COLOR, ASOC_RED});
      m_channels.push_back({1, TYPE_COLOR, ASOC_GREEN});
      m_channels.push_back({2, TYPE_COLOR, ASOC_BLUE});
      break;

    case heif_colorspace_YCbCr:
      m_channels.push_back({0, TYPE_COLOR, ASOC_Y});
      m_channels.push_back({1, TYPE_COLOR, ASOC_Cb});
      m_channels.push_back({2, TYPE_COLOR, ASOC_Cr});
      break;

    case heif_colorspace_monochrome:
      m_channels.push_back({0, TYPE_COLOR, ASOC_GREY});
      break;

    default:
      //TODO - Handle remaining cases.
      break;
  }
}

Error Box_cmap::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  while (!range.eof() && !range.error()) {
    Component component;
    component.component_index = range.read16();
    component.mapping_type = range.read8();
    component.palette_colour = range.read8();
    m_components.push_back(component);
  }

  return range.get_error();
}


std::string Box_cmap::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  for (const auto& component : m_components) {
    sstr << indent << "component_index: " << component.component_index
         << ", mapping_type: " << (int) (component.mapping_type)
         << ", palette_colour: " << (int) (component.palette_colour) << "\n";
  }

  return sstr.str();
}


Error Box_cmap::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  for (const auto& component : m_components) {
    writer.write16(component.component_index);
    writer.write8(component.mapping_type);
    writer.write8(component.palette_colour);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_pclr::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint16_t num_entries = range.read16();
  uint8_t num_palette_columns = range.read8();
  for (uint8_t i = 0; i < num_palette_columns; i++) {
    uint8_t bit_depth = range.read8();
    if (bit_depth & 0x80) {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "pclr with signed data is not supported");
    }
    if (bit_depth > 16) {
      return Error(heif_error_Unsupported_feature,
                   heif_suberror_Unsupported_data_version,
                   "pclr more than 16 bits per channel is not supported");
    }
    m_bitDepths.push_back(bit_depth);
  }
  for (uint16_t j = 0; j < num_entries; j++) {
    PaletteEntry entry;
    for (unsigned long int i = 0; i < entry.columns.size(); i++) {
      if (m_bitDepths[i] <= 8) {
        entry.columns.push_back(range.read8());
      }
      else {
        entry.columns.push_back(range.read16());
      }
    }
    m_entries.push_back(entry);
  }

  return range.get_error();
}


std::string Box_pclr::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  sstr << indent << "NE: " << m_entries.size();
  sstr << ", NPC: " << (int) get_num_columns();
  sstr << ", B: ";
  for (uint8_t b : m_bitDepths) {
    sstr << (int) b << ", ";
  }
  // TODO: maybe dump entries too?
  sstr << "\n";

  return sstr.str();
}


Error Box_pclr::write(StreamWriter& writer) const
{
  if (get_num_columns() == 0) {
    // skip
    return Error::Ok;
  }

  size_t box_start = reserve_box_header_space(writer);

  writer.write16(get_num_entries());
  writer.write8(get_num_columns());
  for (uint8_t b : m_bitDepths) {
    writer.write8(b);
  }
  for (PaletteEntry entry : m_entries) {
    for (unsigned long int i = 0; i < entry.columns.size(); i++) {
      if (m_bitDepths[i] <= 8) {
        writer.write8((uint8_t) (entry.columns[i]));
      }
      else {
        writer.write16(entry.columns[i]);
      }
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}

void Box_pclr::set_columns(uint8_t num_columns, uint8_t bit_depth)
{
  m_bitDepths.clear();
  m_entries.clear();
  for (int i = 0; i < num_columns; i++) {
    m_bitDepths.push_back(bit_depth);
  }
}

Error Box_j2kL::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  uint16_t layer_count = range.read16();

  if (layer_count > range.get_remaining_bytes() / (2+1+2)) {
    std::stringstream sstr;
    sstr << "j2kL box wants to define " << layer_count << "JPEG-2000 layers, but the box only contains "
         << range.get_remaining_bytes() / (2 + 1 + 2) << " layers entries";
    return {heif_error_Invalid_input,
            heif_suberror_End_of_data,
            sstr.str()};
  }

  m_layers.resize(layer_count);

  for (int i = 0; i < layer_count && !range.error() && !range.eof(); i++) {
    Layer layer;
    layer.layer_id = range.read16();
    layer.discard_levels = range.read8();
    layer.decode_layers = range.read16();
    m_layers[i] = layer;
  }

  if (range.get_error()) {
    m_layers.clear();
  }

  return range.get_error();
}

std::string Box_j2kL::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  for (const auto& layer : m_layers) {
    sstr << indent << "layer_id: " << layer.layer_id
         << ", discard_levels: " << (int) (layer.discard_levels)
         << ", decode_layers: " << layer.decode_layers << "\n";
  }

  return sstr.str();
}


Error Box_j2kL::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write16((uint16_t) m_layers.size());
  for (const auto& layer : m_layers) {
    writer.write16(layer.layer_id);
    writer.write8(layer.discard_levels);
    writer.write16(layer.decode_layers);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_j2kH::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  return read_children(range, READ_CHILDREN_ALL, limits);
}

std::string Box_j2kH::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  sstr << dump_children(indent);

  return sstr.str();
}


Error JPEG2000MainHeader::parseHeader(const std::vector<uint8_t>& compressedImageData)
{
  // TODO: it is very inefficient to store the whole image data when we only need the header

  headerData = compressedImageData;
  return doParse();
}

Error JPEG2000MainHeader::doParse()
{
  cursor = 0;
  Error err = parse_SOC_segment();
  if (err) {
    return err;
  }
  err = parse_SIZ_segment();
  if (err) {
    return err;
  }
  if (cursor < headerData.size() - MARKER_LEN) {
    uint16_t marker = read16();
    if (marker == JPEG2000_CAP_MARKER) {
      return parse_CAP_segment_body();
    }
    return Error::Ok;
  }
  // we should have at least COD and QCD, so this is probably broken.
  return Error(heif_error_Invalid_input,
               heif_suberror_Invalid_J2K_codestream,
               std::string("Missing required header marker(s)"));
}

Error JPEG2000MainHeader::parse_SOC_segment()
{
  const size_t REQUIRED_BYTES = MARKER_LEN;
  if ((headerData.size() < REQUIRED_BYTES) || (cursor > (headerData.size() - REQUIRED_BYTES))) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream);
  }
  uint16_t marker = read16();
  if (marker == JPEG2000_SOC_MARKER) {
    return Error::Ok;
  }
  return Error(heif_error_Invalid_input,
               heif_suberror_Invalid_J2K_codestream,
               std::string("Missing required SOC Marker"));
}

Error JPEG2000MainHeader::parse_SIZ_segment()
{
  size_t REQUIRED_BYTES = MARKER_LEN + 38 + 3 * 1;
  if ((headerData.size() < REQUIRED_BYTES) || (cursor > (headerData.size() - REQUIRED_BYTES))) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream);
  }

  uint16_t marker = read16();
  if (marker != JPEG2000_SIZ_MARKER) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream,
                 std::string("Missing required SIZ Marker"));
  }
  uint16_t lsiz = read16();
  if ((lsiz < 41) || (lsiz > 49190)) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream,
                 std::string("Out of range Lsiz value"));
  }
  siz.decoder_capabilities = read16();
  siz.reference_grid_width = read32();
  siz.reference_grid_height = read32();
  siz.image_horizontal_offset = read32();
  siz.image_vertical_offset = read32();
  siz.tile_width = read32();
  siz.tile_height = read32();
  siz.tile_offset_x = read32();
  siz.tile_offset_y = read32();
  uint16_t csiz = read16();
  if ((csiz < 1) || (csiz > 16384)) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream,
                 std::string("Out of range Csiz value"));
  }
  if (cursor > headerData.size() - (3 * csiz)) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream);
  }
  // TODO: consider checking for Lsiz consistent with Csiz
  for (uint16_t c = 0; c < csiz; c++) {
    JPEG2000_SIZ_segment::component comp;
    uint8_t ssiz = read8();
    comp.is_signed = (ssiz & 0x80);
    comp.precision = uint8_t((ssiz & 0x7F) + 1);
    comp.h_separation = read8();
    comp.v_separation = read8();
    siz.components.push_back(comp);
  }
  return Error::Ok;
}

Error JPEG2000MainHeader::parse_CAP_segment_body()
{
  if (cursor > headerData.size() - 8) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream);
  }
  uint16_t lcap = read16();
  if ((lcap < 8) || (lcap > 70)) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_J2K_codestream,
                 std::string("Out of range Lcap value"));
  }
  uint32_t pcap = read32();
  for (uint8_t i = 2; i <= 32; i++) {
    if (pcap & (1 << (32 - i))) {
      switch (i) {
        case JPEG2000_Extension_Capability_HT::IDENT:
          parse_Ccap15();
          break;
        default:
          std::cout << "unhandled extended capabilities value: " << (int)i << std::endl;
          read16();
      }
    }
  }
  return Error::Ok;
}

void JPEG2000MainHeader::parse_Ccap15()
{
  uint16_t val = read16();
  JPEG2000_Extension_Capability_HT ccap;
  // We could parse more here, but we don't need that yet.
  ccap.setValue(val);
  cap.push_back(ccap);
}

heif_chroma JPEG2000MainHeader::get_chroma_format() const
{
  // Y-plane must be full resolution
  if (siz.components[0].h_separation != 1 || siz.components[0].v_separation != 1) {
    return heif_chroma_undefined;
  }

  if (siz.components.size() == 1) {
    return heif_chroma_monochrome;
  }
  else if (siz.components.size() == 3) {
    // TODO: we should map channels through `cdef` ?

    // both chroma components must have the same sampling
    if (siz.components[1].h_separation != siz.components[2].h_separation ||
        siz.components[1].v_separation != siz.components[2].v_separation) {
      return heif_chroma_undefined;
    }

    if (siz.components[1].h_separation == 2 && siz.components[1].v_separation==2) {
      return heif_chroma_420;
    }
    if (siz.components[1].h_separation == 2 && siz.components[1].v_separation==1) {
      return heif_chroma_422;
    }
    if (siz.components[1].h_separation == 1 && siz.components[1].v_separation==1) {
      return heif_chroma_444;
    }
  }

  return heif_chroma_undefined;
}
