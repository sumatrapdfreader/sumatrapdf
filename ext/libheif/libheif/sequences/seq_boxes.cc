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

#include "sequences/seq_boxes.h"
#include <iomanip>
#include <set>
#include <limits>
#include <utility>
#include <algorithm>


Error Box_container::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  return read_children(range, READ_CHILDREN_ALL, limits);
}


std::string Box_container::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);
  sstr << dump_children(indent);

  return sstr.str();
}


double Box_mvhd::get_matrix_element(int idx) const
{
  if (idx == 8) {
    return 1.0;
  }

  return m_matrix[idx] / double(0x10000);
}


Error Box_mvhd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 1) {
    return unsupported_version_error("mvhd");
  }

  if (get_version() == 1) {
    m_creation_time = range.read64();
    m_modification_time = range.read64();
    m_timescale = range.read32();
    m_duration = range.read64();
  }
  else {
    // version==0
    m_creation_time = range.read32();
    m_modification_time = range.read32();
    m_timescale = range.read32();
    m_duration = range.read32();
  }

  m_rate = range.read32();
  m_volume = range.read16();
  range.skip(2);
  range.skip(8);
  for (uint32_t& m : m_matrix) {
    m = range.read32();
  }
  for (int i = 0; i < 6; i++) {
    range.skip(4);
  }

  m_next_track_ID = range.read32();

  return range.get_error();
}


std::string Box_mvhd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "creation time:     " << m_creation_time << "\n"
      << indent << "modification time: " << m_modification_time << "\n"
      << indent << "timescale: " << m_timescale << "\n"
      << indent << "duration: " << m_duration << "\n";
  sstr << indent << "rate: " << get_rate() << "\n"
      << indent << "volume: " << get_volume() << "\n"
      << indent << "matrix:\n";
  for (int y = 0; y < 3; y++) {
    sstr << indent << "  ";
    for (int i = 0; i < 3; i++) {
      sstr << get_matrix_element(i + 3 * y) << " ";
    }
    sstr << "\n";
  }
  sstr << indent << "next_track_ID: " << m_next_track_ID << "\n";

  return sstr.str();
}


void Box_mvhd::derive_box_version()
{
  if (m_creation_time > 0xFFFFFFFF ||
      m_modification_time > 0xFFFFFFFF ||
      m_timescale > 0xFFFFFFFF ||
      m_duration > 0xFFFFFFFF) {
    set_version(1);
  }
  else {
    set_version(0);
  }
}


Error Box_mvhd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (get_version() == 1) {
    writer.write64(m_creation_time);
    writer.write64(m_modification_time);
    writer.write32(m_timescale);
    writer.write64(m_duration);
  }
  else {
    // version==0
    writer.write32(static_cast<uint32_t>(m_creation_time));
    writer.write32(static_cast<uint32_t>(m_modification_time));
    writer.write32(static_cast<uint32_t>(m_timescale));
    writer.write32(static_cast<uint32_t>(m_duration));
  }

  writer.write32(m_rate);
  writer.write16(m_volume);
  writer.write16(0);
  writer.write64(0);
  for (uint32_t m : m_matrix) {
    writer.write32(m);
  }
  for (int i = 0; i < 6; i++) {
    writer.write32(0);
  }

  writer.write32(m_next_track_ID);

  prepend_header(writer, box_start);

  return Error::Ok;
}


double Box_tkhd::get_matrix_element(int idx) const
{
  if (idx == 8) {
    return 1.0;
  }

  return m_matrix[idx] / double(0x10000);
}


Error Box_tkhd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 1) {
    return unsupported_version_error("tkhd");
  }

  if (get_version() == 1) {
    m_creation_time = range.read64();
    m_modification_time = range.read64();
    m_track_id = range.read32();
    range.skip(4);
    m_duration = range.read64();
  }
  else {
    // version==0
    m_creation_time = range.read32();
    m_modification_time = range.read32();
    m_track_id = range.read32();
    range.skip(4);
    m_duration = range.read32();
  }

  range.skip(8);
  m_layer = range.read16();
  m_alternate_group = range.read16();
  m_volume = range.read16();
  range.skip(2);
  for (uint32_t& m : m_matrix) {
    m = range.read32();
  }

  m_width = range.read32();
  m_height = range.read32();

  return range.get_error();
}


std::string Box_tkhd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "track enabled: " << ((get_flags() & Track_enabled) ? "yes" : "no") << "\n"
       << indent << "track in movie: " << ((get_flags() & Track_in_movie) ? "yes" : "no") << "\n"
       << indent << "track in preview: " << ((get_flags() & Track_in_preview) ? "yes" : "no") << "\n"
       << indent << "track size is aspect ratio: " << ((get_flags() & Track_size_is_aspect_ratio) ? "yes" : "no") << "\n";
  sstr << indent << "creation time:     " << m_creation_time << "\n"
      << indent << "modification time: " << m_modification_time << "\n"
      << indent << "track ID: " << m_track_id << "\n"
      << indent << "duration: " << m_duration << "\n";
  sstr << indent << "layer: " << m_layer << "\n"
      << indent << "alternate_group: " << m_alternate_group << "\n"
      << indent << "volume: " << get_volume() << "\n"
      << indent << "matrix:\n";
  for (int y = 0; y < 3; y++) {
    sstr << indent << "  ";
    for (int i = 0; i < 3; i++) {
      sstr << get_matrix_element(i + 3 * y) << " ";
    }
    sstr << "\n";
  }

  sstr << indent << "width: " << get_width() << "\n"
      << indent << "height: " << get_height() << "\n";

  return sstr.str();
}


void Box_tkhd::derive_box_version()
{
  if (m_creation_time > 0xFFFFFFFF ||
      m_modification_time > 0xFFFFFFFF ||
      m_duration > 0xFFFFFFFF) {
    set_version(1);
  }
  else {
    set_version(0);
  }
}


Error Box_tkhd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (get_version() == 1) {
    writer.write64(m_creation_time);
    writer.write64(m_modification_time);
    writer.write32(m_track_id);
    writer.write32(0);
    writer.write64(m_duration);
  }
  else {
    // version==0
    writer.write32(static_cast<uint32_t>(m_creation_time));
    writer.write32(static_cast<uint32_t>(m_modification_time));
    writer.write32(m_track_id);
    writer.write32(0);
    writer.write32(static_cast<uint32_t>(m_duration));
  }

  writer.write64(0);
  writer.write16(m_layer);
  writer.write16(m_alternate_group);
  writer.write16(m_volume);
  writer.write16(0);
  for (uint32_t m : m_matrix) {
    writer.write32(m);
  }

  writer.write32(m_width);
  writer.write32(m_height);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_mdhd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 1) {
    return unsupported_version_error("mdhd");
  }

  if (get_version() == 1) {
    m_creation_time = range.read64();
    m_modification_time = range.read64();
    m_timescale = range.read32();
    m_duration = range.read64();
  }
  else {
    // version==0
    m_creation_time = range.read32();
    m_modification_time = range.read32();
    m_timescale = range.read32();
    m_duration = range.read32();
  }

  uint16_t language_packed = range.read16();
  m_language[0] = ((language_packed >> 10) & 0x1F) + 0x60;
  m_language[1] = ((language_packed >> 5) & 0x1F) + 0x60;
  m_language[2] = ((language_packed >> 0) & 0x1F) + 0x60;
  m_language[3] = 0;

  range.skip(2);

  return range.get_error();
}


std::string Box_mdhd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "creation time:     " << m_creation_time << "\n"
      << indent << "modification time: " << m_modification_time << "\n"
      << indent << "timescale: " << m_timescale << "\n"
      << indent << "duration: " << m_duration << "\n";
  sstr << indent << "language: " << m_language << "\n";

  return sstr.str();
}


void Box_mdhd::derive_box_version()
{
  if (m_creation_time > 0xFFFFFFFF ||
      m_modification_time > 0xFFFFFFFF ||
      m_duration > 0xFFFFFFFF) {
    set_version(1);
  }
  else {
    set_version(0);
  }
}


Error Box_mdhd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (get_version() == 1) {
    writer.write64(m_creation_time);
    writer.write64(m_modification_time);
    writer.write32(m_timescale);
    writer.write64(m_duration);
  }
  else {
    // version==0
    writer.write32(static_cast<uint32_t>(m_creation_time));
    writer.write32(static_cast<uint32_t>(m_modification_time));
    writer.write32(m_timescale);
    writer.write32(static_cast<uint32_t>(m_duration));
  }

  auto language_packed = static_cast<uint16_t>((((m_language[0] - 0x60) & 0x1F) << 10) |
                                               (((m_language[1] - 0x60) & 0x1F) << 5) |
                                               (((m_language[2] - 0x60) & 0x1F) << 0));
  writer.write16(language_packed);
  writer.write16(0);

  prepend_header(writer, box_start);

  return Error::Ok;
}



Error Box_vmhd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("vmhd");
  }

  m_graphics_mode = range.read16();
  for (uint16_t& c : m_op_color) {
    c = range.read16();
  }

  return range.get_error();
}


std::string Box_vmhd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "graphics mode: " << m_graphics_mode;
  if (m_graphics_mode == 0) {
    sstr << " (copy)";
  }
  sstr << "\n"
       << indent << "op color: " << m_op_color[0] << "; " << m_op_color[1] << "; " << m_op_color[2] << "\n";

  return sstr.str();
}


Error Box_vmhd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write16(m_graphics_mode);
  for (uint16_t c : m_op_color) {
    writer.write16(c);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_nmhd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("nmhd");
  }

  return range.get_error();
}


std::string Box_nmhd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);

  return sstr.str();
}


Error Box_nmhd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_stsd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("stsd");
  }

  uint32_t entry_count = range.read32();

  if (limits->max_sample_description_box_entries &&
      entry_count > limits->max_sample_description_box_entries) {
    std::stringstream sstr;
    sstr << "Allocating " << static_cast<uint64_t>(entry_count) << " sample description items exceeds the security limit of "
         << limits->max_sample_description_box_entries << " items";

    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};

  }

  for (uint32_t i = 0; i < entry_count; i++) {
    std::shared_ptr<Box> entrybox;
    Error err = Box::read(range, &entrybox, limits);
    if (err) {
      return err;
    }

#if 0
    auto visualSampleEntry_box = std::dynamic_pointer_cast<Box_VisualSampleEntry>(entrybox);
    if (!visualSampleEntry_box) {
      return Error{heif_error_Invalid_input,
                   heif_suberror_Unspecified,
                   "Invalid or unknown VisualSampleEntry in stsd box."};
    }
#endif

    m_sample_entries.push_back(entrybox);
  }

  return range.get_error();
}


std::string Box_stsd::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  for (size_t i = 0; i < m_sample_entries.size(); i++) {
    sstr << indent << "[" << i << "]\n";
    indent++;
    sstr << m_sample_entries[i]->dump(indent);
    indent--;
  }

  return sstr.str();
}


Error Box_stsd::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_sample_entries.size()));
  for (const auto& sample : m_sample_entries) {
    sample->write(writer);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_stts::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("stts");
  }

  uint32_t entry_count = range.read32();

  if (entry_count > limits->max_sequence_frames) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "Security limit for maximum number of sequence frames exceeded"
    };
  }

  if (auto err = m_memory_handle.alloc(entry_count * sizeof(TimeToSample),
                                       limits, "the 'stts' table")) {
    return err;
  }

  m_entries.resize(entry_count);

  for (uint32_t i = 0; i < entry_count; i++) {
    if (range.eof()) {
      std::stringstream sstr;
      sstr << "stts box should contain " << entry_count << " entries, but box only contained "
          << i << " entries";

      return {
        heif_error_Invalid_input,
        heif_suberror_End_of_data,
        sstr.str()
      };
    }

    TimeToSample entry{};
    entry.sample_count = range.read32();
    entry.sample_delta = range.read32();
    m_entries[i] = entry;
  }

  return range.get_error();
}


std::string Box_stts::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  for (size_t i = 0; i < m_entries.size(); i++) {
    sstr << indent << "[" << i << "] : cnt=" << m_entries[i].sample_count << ", delta=" << m_entries[i].sample_delta << "\n";
  }

  return sstr.str();
}


Error Box_stts::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_entries.size()));
  for (const auto& sample : m_entries) {
    writer.write32(sample.sample_count);
    writer.write32(sample.sample_delta);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


uint32_t Box_stts::get_sample_duration(uint32_t sample_idx)
{
  size_t i = 0;
  while (i < m_entries.size()) {
    if (sample_idx < m_entries[i].sample_count) {
      return m_entries[i].sample_delta;
    }
    else {
      sample_idx -= m_entries[i].sample_count;
    }
  }

  return 0;
}


void Box_stts::append_sample_duration(uint32_t duration)
{
  if (m_entries.empty() || m_entries.back().sample_delta != duration) {
    TimeToSample entry{};
    entry.sample_delta = duration;
    entry.sample_count = 1;
    m_entries.push_back(entry);
    return;
  }

  m_entries.back().sample_count++;
}


uint64_t Box_stts::get_total_duration(bool include_last_frame_duration)
{
  uint64_t total = 0;

  for (const auto& entry : m_entries) {
    total += entry.sample_count * uint64_t(entry.sample_delta);
  }

  if (!include_last_frame_duration && !m_entries.empty()) {
    total -= m_entries.back().sample_delta;
  }

  return total;
}


uint32_t Box_stts::get_number_of_samples() const
{
  uint32_t total = 0;
  for (const auto& entry : m_entries) {
    total += entry.sample_count;
  }

  return total;
}


Error Box_ctts::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  uint8_t version = get_version();

  if (version > 1) {
    return unsupported_version_error("ctts");
  }

  uint32_t entry_count = range.read32();

  if (entry_count > limits->max_sequence_frames) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "Security limit for maximum number of sequence frames exceeded"
    };
  }

  if (auto err = m_memory_handle.alloc(entry_count * sizeof(OffsetToSample),
                                       limits, "the 'ctts' table")) {
    return err;
  }

  m_entries.resize(entry_count);

  for (uint32_t i = 0; i < entry_count; i++) {
    if (range.eof()) {
      std::stringstream sstr;
      sstr << "ctts box should contain " << entry_count << " entries, but box only contained "
          << i << " entries";

      return {
        heif_error_Invalid_input,
        heif_suberror_End_of_data,
        sstr.str()
      };
    }

    OffsetToSample entry{};
    entry.sample_count = range.read32();
    if (version == 0) {
      uint32_t offset = range.read32();
#if 0
      // TODO: I disabled this because I found several files that seem to
      //       have wrong data. Since we are not using the 'ctts' data anyway,
      //       let's not care about it now.

      if (offset > INT32_MAX) {
        return {
          heif_error_Unsupported_feature,
          heif_suberror_Unsupported_parameter,
          "We don't support offsets > 0x7fff in 'ctts' box."
        };
      }
#endif

      entry.sample_offset = static_cast<int32_t>(offset);
    }
    else if (version == 1) {
      entry.sample_offset = range.read32s();
    }
    else {
      assert(false);
    }

    m_entries[i] = entry;
  }

  return range.get_error();
}


std::string Box_ctts::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  for (size_t i = 0; i < m_entries.size(); i++) {
    sstr << indent << "[" << i << "] : cnt=" << m_entries[i].sample_count << ", offset=" << m_entries[i].sample_offset << "\n";
  }

  return sstr.str();
}


int32_t Box_ctts::compute_min_offset() const
{
  int32_t min_offset = INT32_MAX;
  for (const auto& entry : m_entries) {
    min_offset = std::min(min_offset, entry.sample_offset);
  }

  return min_offset;
}


uint32_t Box_ctts::get_number_of_samples() const
{
  uint32_t total = 0;
  for (const auto& entry : m_entries) {
    total += entry.sample_count;
  }

  return total;
}


Error Box_ctts::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  int32_t min_offset;

  if (get_version() == 0) {
    // shift such that all offsets are >= 0
    min_offset = compute_min_offset();
  }
  else {
    // do not modify offsets
    min_offset = 0;
  }

  writer.write32(static_cast<uint32_t>(m_entries.size()));
  for (const auto& sample : m_entries) {
    writer.write32(sample.sample_count);
    writer.write32s(sample.sample_offset - min_offset);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


int32_t Box_ctts::get_sample_offset(uint32_t sample_idx)
{
  size_t i = 0;
  while (i < m_entries.size()) {
    if (sample_idx < m_entries[i].sample_count) {
      return m_entries[i].sample_offset;
    }
    else {
      sample_idx -= m_entries[i].sample_count;
    }
  }

  return 0;
}


void Box_ctts::append_sample_offset(int32_t offset)
{
  if (m_entries.empty() || m_entries.back().sample_offset != offset) {
    OffsetToSample entry{};
    entry.sample_offset = offset;
    entry.sample_count = 1;
    m_entries.push_back(entry);
    return;
  }

  m_entries.back().sample_count++;
}


bool Box_ctts::is_constant_offset() const
{
  return m_entries.empty() || m_entries.size() == 1;
}

void Box_ctts::derive_box_version()
{
  set_version(1);
}


Error Box_stsc::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("stsc");
  }

  uint32_t entry_count = range.read32();

  if (auto err = m_memory_handle.alloc(entry_count * sizeof(SampleToChunk),
                                       limits, "the 'stsc' table")) {
    return err;
  }

  m_entries.resize(entry_count);

  for (uint32_t i = 0; i < entry_count; i++) {
    SampleToChunk entry{};
    entry.first_chunk = range.read32();
    entry.samples_per_chunk = range.read32();
    entry.sample_description_index = range.read32();

    if (entry.sample_description_index == 0) {
      return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "'sample_description_index' in 'stsc' must not be 0."};
    }

    if (entry.samples_per_chunk > limits->max_sequence_frames) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Number of chunk samples in `stsc` box exceeds security limits of maximum number of frames."};
    }

    m_entries[i] = entry;
  }

  return range.get_error();
}


std::string Box_stsc::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  for (size_t i = 0; i < m_entries.size(); i++) {
    sstr << indent << "[" << i << "]\n"
        << indent << "  first chunk: " << m_entries[i].first_chunk << "\n"
        << indent << "  samples per chunk: " << m_entries[i].samples_per_chunk << "\n"
        << indent << "  sample description index: " << m_entries[i].sample_description_index << "\n";
  }

  return sstr.str();
}


Error Box_stsc::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_entries.size()));
  for (const auto& sample : m_entries) {
    writer.write32(sample.first_chunk);
    writer.write32(sample.samples_per_chunk);
    writer.write32(sample.sample_description_index);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


const Box_stsc::SampleToChunk* Box_stsc::get_chunk(uint32_t idx) const
{
  assert(idx>=1);
  for (size_t i = 0 ; i < m_entries.size();i++) {
    if (idx >= m_entries[i].first_chunk && (i==m_entries.size()-1 || idx < m_entries[i+1].first_chunk)) {
      return &m_entries[i];
    }
  }

  return nullptr;
}


void Box_stsc::add_chunk(uint32_t description_index)
{
  SampleToChunk entry{};
  entry.first_chunk = 1; // TODO
  entry.samples_per_chunk = 0;
  entry.sample_description_index = description_index;
  m_entries.push_back(entry);
}


void Box_stsc::increase_samples_in_chunk(uint32_t nFrames)
{
  assert(!m_entries.empty());

  m_entries.back().samples_per_chunk += nFrames;
}


Error Box_stco::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("stco");
  }

  uint32_t entry_count = range.read32();

  // check required memory

  uint64_t mem_size = entry_count * sizeof(uint32_t);
  if (auto err = m_memory_handle.alloc(mem_size,
                                       limits, "the 'stco' table")) {
    return err;
  }

  for (uint32_t i = 0; i < entry_count; i++) {
    m_offsets.push_back(range.read32());

    if (range.error()) {
      return range.get_error();
    }
  }

  return range.get_error();
}


std::string Box_stco::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  for (size_t i = 0; i < m_offsets.size(); i++) {
    sstr << indent << "[" << i << "] : 0x" << std::hex << m_offsets[i] << std::dec << "\n";
  }

  return sstr.str();
}


Error Box_stco::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_offsets.size()));

  m_offset_start_pos = writer.get_position();

  for (uint32_t offset : m_offsets) {
    writer.write32(offset);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


void Box_stco::patch_file_pointers(StreamWriter& writer, size_t offset)
{
  size_t oldPosition = writer.get_position();

  writer.set_position(m_offset_start_pos);

  for (uint32_t chunk_offset : m_offsets) {
    if (chunk_offset + offset > std::numeric_limits<uint32_t>::max()) {
      writer.write32(0); // TODO: error
    }
    else {
      writer.write32(static_cast<uint32_t>(chunk_offset + offset));
    }
  }

  writer.set_position(oldPosition);
}



Error Box_stsz::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("stsz");
  }

  m_fixed_sample_size = range.read32();
  m_sample_count = range.read32();

  if (m_fixed_sample_size == 0) {
    // check required memory

    if (m_sample_count > limits->max_sequence_frames) {
      return {
        heif_error_Memory_allocation_error,
        heif_suberror_Security_limit_exceeded,
        "Security limit for maximum number of sequence frames exceeded"
      };
    }

    uint64_t mem_size = m_sample_count * sizeof(uint32_t);
    if (auto err = m_memory_handle.alloc(mem_size, limits, "the 'stsz' table")) {
      return err;
    }

    for (uint32_t i = 0; i < m_sample_count; i++) {
      if (range.eof()) {
        std::stringstream sstr;
        sstr << "stsz box should contain " << m_sample_count << " entries, but box only contained "
            << i << " entries";

        return {
          heif_error_Invalid_input,
          heif_suberror_End_of_data,
          sstr.str()
        };
      }

      m_sample_sizes.push_back(range.read32());

      if (range.error()) {
        return range.get_error();
      }
    }
  }

  return range.get_error();
}


std::string Box_stsz::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "sample count: " << m_sample_count << "\n";
  if (m_fixed_sample_size == 0) {
    for (size_t i = 0; i < m_sample_sizes.size(); i++) {
      sstr << indent << "[" << i << "] : " << m_sample_sizes[i] << "\n";
    }
  }
  else {
    sstr << indent << "fixed sample size: " << m_fixed_sample_size << "\n";
  }

  return sstr.str();
}


Error Box_stsz::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(m_fixed_sample_size);
  writer.write32(m_sample_count);
  if (m_fixed_sample_size == 0) {
    assert(m_sample_count == m_sample_sizes.size());

    for (uint32_t size : m_sample_sizes) {
      writer.write32(size);
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


void Box_stsz::append_sample_size(uint32_t size)
{
  if (m_sample_count == 0 && size != 0) {
    m_fixed_sample_size = size;
    m_sample_count = 1;
    return;
  }

  if (m_fixed_sample_size == size && size != 0) {
    m_sample_count++;
    return;
  }

  if (m_fixed_sample_size != 0) {
    for (uint32_t i = 0; i < m_sample_count; i++) {
      m_sample_sizes.push_back(m_fixed_sample_size);
    }

    m_fixed_sample_size = 0;
  }

  m_sample_sizes.push_back(size);
  m_sample_count++;

  assert(m_sample_count == m_sample_sizes.size());
}


Error Box_stss::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("stss");
  }

  uint32_t sample_count = range.read32();

  // check required memory

  uint64_t mem_size = sample_count * sizeof(uint32_t);
  if (auto err = m_memory_handle.alloc(mem_size, limits, "the 'stss' table")) {
    return err;
  }

  for (uint32_t i = 0; i < sample_count; i++) {
    m_sync_samples.push_back(range.read32());

    if (range.error()) {
      return range.get_error();
    }
  }

  return range.get_error();
}


std::string Box_stss::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  for (size_t i = 0; i < m_sync_samples.size(); i++) {
    sstr << indent << "[" << i << "] : " << m_sync_samples[i] << "\n";
  }

  return sstr.str();
}


void Box_stss::set_total_number_of_samples(uint32_t num_samples)
{
  m_all_samples_are_sync_samples = (m_sync_samples.size() == num_samples);
}


Error Box_stss::write(StreamWriter& writer) const
{
  // If we don't need this box, skip it.
  if (m_all_samples_are_sync_samples) {
    return Error::Ok;
  }

  size_t box_start = reserve_box_header_space(writer);

  writer.write32(static_cast<uint32_t>(m_sync_samples.size()));
  for (uint32_t sample : m_sync_samples) {
    writer.write32(sample);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error VisualSampleEntry::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  (void)limits;

  range.skip(6);
  data_reference_index = range.read16();

  pre_defined = range.read16();
  range.skip(2);
  for (uint32_t& p : pre_defined2) {
    p = range.read32();
  }
  width = range.read16();
  height = range.read16();
  horizresolution = range.read32();
  vertresolution = range.read32();
  range.skip(4);
  frame_count = range.read16();
  compressorname = range.read_fixed_string(32);
  depth = range.read16();
  pre_defined3 = range.read16s();

  // other boxes from derived specifications
  //std::shared_ptr<Box_clap> clap; // optional // TODO
  //std::shared_ptr<Box_pixi> pixi; // optional // TODO

  return Error::Ok;
}


Error VisualSampleEntry::write(StreamWriter& writer) const
{
  writer.write32(0);
  writer.write16(0);
  writer.write16(data_reference_index);

  writer.write16(pre_defined);
  writer.write16(0);
  for (uint32_t p : pre_defined2) {
    writer.write32(p);
  }

  writer.write16(width);
  writer.write16(height);
  writer.write32(horizresolution);
  writer.write32(vertresolution);
  writer.write32(0);
  writer.write16(frame_count);
  writer.write_fixed_string(compressorname, 32);
  writer.write16(depth);
  writer.write16(pre_defined3);

  return Error::Ok;
}


std::string VisualSampleEntry::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << indent << "data reference index: " << data_reference_index << "\n"
       << indent << "width: " << width << "\n"
       << indent << "height: " << height << "\n"
       << indent << "horiz. resolution: " << get_horizontal_resolution() << "\n"
       << indent << "vert. resolution: " << get_vertical_resolution() << "\n"
       << indent << "frame count: " << frame_count << "\n"
       << indent << "compressorname: " << compressorname << "\n"
       << indent << "depth: " << depth << "\n";

  return sstr.str();
}


Error Box_URIMetaSampleEntry::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(0);
  writer.write16(0);
  writer.write16(data_reference_index);

  write_children(writer);

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_URIMetaSampleEntry::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << Box::dump(indent);
  sstr << indent << "data reference index: " << data_reference_index << "\n";
  sstr << dump_children(indent);
  return sstr.str();
}


Error Box_URIMetaSampleEntry::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  range.skip(6);
  data_reference_index = range.read16();

  Error err = read_children(range, READ_CHILDREN_ALL, limits);
  if (err) {
    return err;
  }

  return Error::Ok;
}


Error Box_uri::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("uri ");
  }

  m_uri = range.read_string();

  return range.get_error();
}


std::string Box_uri::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "uri: " << m_uri << "\n";

  return sstr.str();
}


Error Box_uri::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write(m_uri);

  prepend_header(writer, box_start);

  return Error::Ok;
}



Error Box_ccst::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("ccst");
  }

  uint32_t bits = range.read32();

  auto& constraints = m_codingConstraints;

  constraints.all_ref_pics_intra = (bits & 0x80000000) != 0;
  constraints.intra_pred_used = (bits & 0x40000000) != 0;
  constraints.max_ref_per_pic = (bits >> 26) & 0x0F;

  return range.get_error();
}


std::string Box_ccst::dump(Indent& indent) const
{
  const auto& constraints = m_codingConstraints;

  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "all ref pics intra: " << std::boolalpha <<constraints.all_ref_pics_intra << "\n"
       << indent << "intra pred used: " << constraints.intra_pred_used << "\n"
       << indent << "max ref per pic: " << ((int) constraints.max_ref_per_pic) << "\n";

  return sstr.str();
}


Error Box_ccst::write(StreamWriter& writer) const
{
  const auto& constraints = m_codingConstraints;

  size_t box_start = reserve_box_header_space(writer);

  uint32_t bits = 0;

  if (constraints.all_ref_pics_intra) {
    bits |= 0x80000000;
  }

  if (constraints.intra_pred_used) {
    bits |= 0x40000000;
  }

  bits |= constraints.max_ref_per_pic << 26;

  writer.write32(bits);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_auxi::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 0) {
    return unsupported_version_error("auxi");
  }

  m_aux_track_type = range.read_string();

  return range.get_error();
}


std::string Box_auxi::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "aux track info type: " << m_aux_track_type << "\n";

  return sstr.str();
}


Error Box_auxi::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write(m_aux_track_type);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_VisualSampleEntry::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  Error err = get_VisualSampleEntry_const().write(writer);
  if (err) {
    return err;
  }

  write_children(writer);

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_VisualSampleEntry::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << Box::dump(indent);
  sstr << m_visualSampleEntry.dump(indent);
  sstr << dump_children(indent);
  return sstr.str();
}


Error Box_VisualSampleEntry::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  auto err = m_visualSampleEntry.parse(range, limits);
  if (err) {
    return err;
  }

  err = read_children(range, READ_CHILDREN_ALL, limits);
  if (err) {
    return err;
  }

  return Error::Ok;
}


std::string Box_sbgp::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << FullBox::dump(indent);
  sstr << indent << "grouping_type: " << fourcc_to_string(m_grouping_type) << "\n";

  if (m_grouping_type_parameter) {
    sstr << indent << "grouping_type_parameter: " << *m_grouping_type_parameter << "\n";
  }

  uint32_t total_samples = 0;
  for (size_t i = 0; i < m_entries.size(); i++) {
    sstr << indent << "[" << std::setw(2) << (i + 1) << "] : " << std::setw(3) << m_entries[i].sample_count << "x " << m_entries[i].group_description_index << "\n";
    total_samples += m_entries[i].sample_count;
  }
  sstr << indent << "total samples: " << total_samples << "\n";

  return sstr.str();
}


void Box_sbgp::derive_box_version()
{
  if (m_grouping_type_parameter) {
    set_version(1);
  }
  else {
    set_version(0);
  }
}


Error Box_sbgp::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(m_grouping_type);
  if (m_grouping_type_parameter) {
    writer.write32(*m_grouping_type_parameter);
  }

  if (m_entries.size() > 0xFFFFFFFF) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Too many sbgp entries."};
  }

  writer.write32(static_cast<uint32_t>(m_entries.size()));
  for (const auto& entry : m_entries) {
    writer.write32(entry.sample_count);
    writer.write32(entry.group_description_index);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_sbgp::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_version() > 1) {
    return unsupported_version_error("sbgp");
  }

  m_grouping_type = range.read32();

  if (get_version() == 1) {
    m_grouping_type_parameter = range.read32();
  }

  uint32_t count = range.read32();
  if (auto err = m_memory_handle.alloc(count * sizeof(Entry),
                                       limits, "the 'sample to group' table")) {
    return err;
  }

  for (uint32_t i = 0; i < count; i++) {
    Entry e{};
    e.sample_count = range.read32();
    e.group_description_index = range.read32();
    m_entries.push_back(e);
    if (range.error()) {
      return range.get_error();
    }
  }

  return range.get_error();
}


std::string SampleGroupEntry_refs::dump() const
{
  std::stringstream sstr;
  if (m_sample_id==0) {
    sstr << "0 (non-ref) refs =";
  }
  else {
    sstr << m_sample_id << " refs =";
  }
  for (uint32_t ref : m_direct_reference_sample_id) {
    sstr << ' ' << ref;
  }

  return sstr.str();
}

Error SampleGroupEntry_refs::write(StreamWriter& writer) const
{
  return {};
}

Error SampleGroupEntry_refs::parse(BitstreamRange& range, const heif_security_limits*)
{
  m_sample_id = range.read32();
  uint8_t cnt = range.read8();
  for (uint8_t i = 0; i < cnt; i++) {
    m_direct_reference_sample_id.push_back(range.read32());
  }

  return Error::Ok;
}


void Box_sgpd::derive_box_version()
{
  if (m_default_length) {
    set_version(1);
    assert(!m_default_sample_description_index);
    return;
  }

  if (m_default_sample_description_index) {
    set_version(2);
    return;
  }

  set_version(0);
}


std::string Box_sgpd::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << FullBox::dump(indent);

  sstr << indent << "grouping_type: " << fourcc_to_string(m_grouping_type) << "\n";
  if (m_default_length) {
    sstr << indent << "default_length: " << *m_default_length << "\n";
  }
  if (m_default_sample_description_index) {
    sstr << indent << "default_sample_description_index: " << *m_default_sample_description_index << "\n";
  }

  for (size_t i=0; i<m_entries.size(); i++) {
    sstr << indent << "[" << (i+1) << "] : ";
    if (m_entries[i].sample_group_entry) {
      sstr << m_entries[i].sample_group_entry->dump() << "\n";
    }
    else {
      sstr << "empty (description_length=" << m_entries[i].description_length << ")\n";
    }
  }

  return sstr.str();
}


Error Box_sgpd::write(StreamWriter& writer) const
{
return {};
}


Error Box_sgpd::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  m_grouping_type = range.read32();

  if (get_version() == 1) {
    m_default_length = range.read32();
  }

  if (get_version() >= 2) {
    m_default_sample_description_index = range.read32();
  }

  uint32_t entry_count = range.read32();

  if (limits->max_sample_group_description_box_entries &&
      entry_count > limits->max_sample_group_description_box_entries) {
    std::stringstream sstr;
    sstr << "Allocating " << static_cast<uint64_t>(entry_count) << " sample group description items exceeds the security limit of "
         << limits->max_sample_group_description_box_entries << " items";

    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};

  }

  for (uint32_t i = 0; i < entry_count; i++) {
    Entry entry;

    if (get_version() == 1) {
      if (*m_default_length == 0) {
        entry.description_length = range.read32();
      }
    }

    switch (m_grouping_type) {
      case fourcc("refs"): {
        entry.sample_group_entry = std::make_shared<SampleGroupEntry_refs>();
        Error err = entry.sample_group_entry->parse(range, limits);
        if (err) {
          return err;
        }

        break;
      }

      default:
        break;
    }

    m_entries.emplace_back(std::move(entry));
  }

  return Error::Ok;
}


std::string Box_btrt::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << FullBox::dump(indent);

  sstr << indent << "bufferSizeDB: " << m_bufferSizeDB << " bytes\n";
  sstr << indent << "max bitrate: " << m_maxBitrate << " bits/sec\n";
  sstr << indent << "avg bitrate: " << m_avgBitrate << " bits/sec\n";

  return sstr.str();
}


Error Box_btrt::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write32(m_bufferSizeDB);
  writer.write32(m_maxBitrate);
  writer.write32(m_avgBitrate);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_btrt::parse(BitstreamRange& range, const heif_security_limits*)
{
  m_bufferSizeDB = range.read32();
  m_maxBitrate = range.read32();
  m_avgBitrate = range.read32();

  return Error::Ok;
}



void Box_saiz::set_aux_info_type(uint32_t aux_info_type, uint32_t aux_info_type_parameter)
{
  m_aux_info_type = aux_info_type;
  m_aux_info_type_parameter = aux_info_type_parameter;

  bool nonnull = (m_aux_info_type != 0 || m_aux_info_type_parameter != 0);
  set_flags(nonnull ? 1 : 0);
}


void Box_saiz::add_sample_size(uint8_t s)
{
  // --- it is the first sample -> put into default size (except if it is a zero size = no sample aux info)

  if (s != 0 && m_num_samples == 0) {
    m_default_sample_info_size = s;
    m_num_samples = 1;
    return;
  }

  // --- if it's the default size, just add more to the number of default sizes

  if (s != 0 && s == m_default_sample_info_size) {
    m_num_samples++;
    return;
  }

  // --- it is different from the default size -> add the list

  // first copy samples with the default size into the list

  if (m_default_sample_info_size != 0) {
    for (uint32_t i = 0; i < m_num_samples; i++) {
      m_sample_sizes.push_back(m_default_sample_info_size);
    }

    m_default_sample_info_size = 0;
  }

  // add the new sample size

  m_num_samples++;
  m_sample_sizes.push_back(s);
}


uint8_t Box_saiz::get_sample_size(uint32_t idx)
{
  if (m_default_sample_info_size != 0) {
    return m_default_sample_info_size;
  }

  if (idx >= m_sample_sizes.size()) {
    return 0;
  }

  return m_sample_sizes[idx];
}


std::string Box_saiz::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << FullBox::dump(indent);

  sstr << indent << "aux_info_type: ";
  if (m_aux_info_type == 0) {
    sstr << "0\n";
  }
  else {
    sstr << fourcc_to_string(m_aux_info_type) << "\n";
  }

  sstr << indent << "aux_info_type_parameter: ";
  if (m_aux_info_type_parameter == 0) {
    sstr << "0\n";
  }
  else {
    sstr << fourcc_to_string(m_aux_info_type_parameter) << "\n";
  }

  sstr << indent << "default sample size: ";
  if (m_default_sample_info_size == 0) {
    sstr << "0 (variable)\n";
  }
  else {
    sstr << ((int)m_default_sample_info_size) << "\n";
  }

  if (m_default_sample_info_size == 0) {
    for (size_t i = 0; i < m_sample_sizes.size(); i++) {
      sstr << indent << "[" << i << "] : " << ((int) m_sample_sizes[i]) << "\n";
    }
  }

  return sstr.str();
}


Error Box_saiz::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (get_flags() & 1) {
    writer.write32(m_aux_info_type);
    writer.write32(m_aux_info_type_parameter);
  }

  writer.write8(m_default_sample_info_size);

  if (m_default_sample_info_size == 0) {
    assert(m_num_samples == m_sample_sizes.size());

    uint32_t num_nonnull_samples = static_cast<uint32_t>(m_sample_sizes.size());
    while (num_nonnull_samples > 0 && m_sample_sizes[num_nonnull_samples-1] == 0) {
      num_nonnull_samples--;
    }

    writer.write32(num_nonnull_samples);

    for (size_t i = 0; i < num_nonnull_samples; i++) {
      writer.write8(m_sample_sizes[i]);
    }
  }
  else {
    writer.write32(m_num_samples);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_saiz::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_flags() & 1) {
    m_aux_info_type = range.read32();
    m_aux_info_type_parameter = range.read32();
  }

  m_default_sample_info_size = range.read8();
  m_num_samples = range.read32();

  if (limits && m_num_samples > limits->max_sequence_frames) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "Number of 'saiz' samples exceeds the maximum number of sequence frames."
    };
  }

  if (m_default_sample_info_size == 0) {
    // check required memory

    uint64_t mem_size = m_num_samples;
    if (auto err = m_memory_handle.alloc(mem_size, limits, "the 'sample aux info sizes' (saiz) table")) {
      return err;
    }

    // read whole table at once

    m_sample_sizes.resize(m_num_samples);
    range.read(m_sample_sizes.data(), m_num_samples);
  }

  return range.get_error();
}



void Box_saio::set_aux_info_type(uint32_t aux_info_type, uint32_t aux_info_type_parameter)
{
  m_aux_info_type = aux_info_type;
  m_aux_info_type_parameter = aux_info_type_parameter;

  bool nonnull = (m_aux_info_type != 0 || m_aux_info_type_parameter != 0);
  set_flags(nonnull ? 1 : 0);
}


void Box_saio::add_chunk_offset(uint64_t s)
{
  if (s > 0xFFFFFFFF) {
    m_need_64bit = true;
    set_version(1);
  }

  m_chunk_offset.push_back(s);
}


uint64_t Box_saio::get_chunk_offset(uint32_t idx) const
{
  if (idx >= m_chunk_offset.size()) {
    return 0;
  }
  else {
    return m_chunk_offset[idx];
  }
}


std::string Box_saio::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << FullBox::dump(indent);

  sstr << indent << "aux_info_type: ";
  if (m_aux_info_type == 0) {
    sstr << "0\n";
  }
  else {
    sstr << fourcc_to_string(m_aux_info_type) << "\n";
  }

  sstr << indent << "aux_info_type_parameter: ";
  if (m_aux_info_type_parameter == 0) {
    sstr << "0\n";
  }
  else {
    sstr << fourcc_to_string(m_aux_info_type_parameter) << "\n";
  }

  for (size_t i = 0; i < m_chunk_offset.size(); i++) {
    sstr << indent << "[" << i << "] : 0x" << std::hex << m_chunk_offset[i] << "\n";
  }

  return sstr.str();
}


void Box_saio::patch_file_pointers(StreamWriter& writer, size_t offset)
{
  size_t oldPosition = writer.get_position();

  writer.set_position(m_offset_start_pos);

  for (uint64_t ptr : m_chunk_offset) {
    if (get_version() == 0 && ptr + offset > std::numeric_limits<uint32_t>::max()) {
      writer.write32(0); // TODO: error
    } else if (get_version() == 0) {
      writer.write32(static_cast<uint32_t>(ptr + offset));
    } else {
      writer.write64(ptr + offset);
    }
  }

  writer.set_position(oldPosition);
}


Error Box_saio::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (get_flags() & 1) {
    writer.write32(m_aux_info_type);
    writer.write32(m_aux_info_type_parameter);
  }

  if (m_chunk_offset.size() > std::numeric_limits<uint32_t>::max()) {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unspecified,
                 "Maximum number of chunks exceeded"};
  }
  writer.write32(static_cast<uint32_t>(m_chunk_offset.size()));

  m_offset_start_pos = writer.get_position();

  for (uint64_t size : m_chunk_offset) {
    if (m_need_64bit) {
      writer.write64(size);
    } else {
      writer.write32(static_cast<uint32_t>(size));
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_saio::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  if (get_flags() & 1) {
    m_aux_info_type = range.read32();
    m_aux_info_type_parameter = range.read32();
  }

  uint32_t num_chunks = range.read32();

  // We have no explicit maximum on the number of chunks.
  // Use the maximum number of frames as an upper limit.
  if (limits && num_chunks > limits->max_sequence_frames) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "Number of 'saio' chunks exceeds the maximum number of sequence frames."
    };
  }

  // check required memory
  uint64_t mem_size = num_chunks * sizeof(uint64_t);

  if (auto err = m_memory_handle.alloc(mem_size, limits, "the 'saio' table")) {
    return err;
  }

  m_chunk_offset.resize(num_chunks);

  for (uint32_t i = 0; i < num_chunks; i++) {
    uint64_t offset;
    if (get_version() == 1) {
      offset = range.read64();
    }
    else {
      offset = range.read32();
    }

    m_chunk_offset[i] = offset;

    if (range.error()) {
      return range.get_error();
    }
  }

  return Error::Ok;
}


std::string Box_sdtp::dump(Indent& indent) const
{
  std::stringstream sstr;
  sstr << FullBox::dump(indent);

  assert(m_sample_information.size() <= UINT32_MAX);

  for (uint32_t i = 0; i < static_cast<uint32_t>(m_sample_information.size()); i++) {
    const char* spaces = "            ";
    int nSpaces = 6;
    int k = i;
    while (k >= 10 && nSpaces < 12) {
      k /= 10;
      nSpaces++;
    }

    spaces = spaces + 12 - nSpaces;

    sstr << indent << "[" << i << "] : is_leading=" << (int) get_is_leading(i) << "\n"
        << indent << spaces << "depends_on=" << (int) get_depends_on(i) << "\n"
        << indent << spaces << "is_depended_on=" << (int) get_is_depended_on(i) << "\n"
        << indent << spaces << "has_redundancy=" << (int) get_has_redundancy(i) << "\n";
  }

  return sstr.str();
}


Error Box_sdtp::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);

  // We have no easy way to get the number of samples from 'saiz' or 'stz2' as specified
  // in the standard. Instead, we read until the end of the box.
  size_t nSamples = range.get_remaining_bytes();

  m_sample_information.resize(nSamples);
  range.read(m_sample_information.data(), nSamples);

  return Error::Ok;
}


Error Box_tref::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  while (!range.eof()) {
    BoxHeader header;
    Error err = header.parse_header(range);
    if (err != Error::Ok) {
      return err;
    }

    if (header.get_box_size() < header.get_header_size()) {
      return {heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "Invalid box size (smaller than header)"};
    }

    uint64_t dataSize = (header.get_box_size() - header.get_header_size());

    if (dataSize % 4 != 0 || dataSize < 4) {
      return {heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "Input file has a 'tref' TrackReferenceTypeBox with invalid size."};
    }

    uint64_t nRefs = dataSize / 4;

    if (limits->max_items && nRefs > limits->max_items) {
      std::stringstream sstr;
      sstr << "Number of references in tref box (" << nRefs << ") exceeds the security limits of " << limits->max_items << " references.";

      return {heif_error_Invalid_input,
              heif_suberror_Security_limit_exceeded,
              sstr.str()};
    }

    Reference ref;
    ref.reference_type = header.get_short_type();

    for (uint64_t i = 0; i < nRefs; i++) {
      if (range.eof()) {
        std::stringstream sstr;
        sstr << "tref box should contain " << nRefs << " references, but we can only read " << i << " references.";

        return {heif_error_Invalid_input,
                heif_suberror_End_of_data,
                sstr.str()};
      }

      ref.to_track_id.push_back(static_cast<uint32_t>(range.read32()));
    }

    m_references.push_back(ref);
  }


  // --- check for duplicate references

  if (auto error = check_for_double_references()) {
    return error;
  }

  return range.get_error();
}


Error Box_tref::check_for_double_references() const
{
  for (const auto& ref : m_references) {
    std::set<uint32_t> to_ids;
    for (const auto to_id : ref.to_track_id) {
      if (to_ids.find(to_id) == to_ids.end()) {
        to_ids.insert(to_id);
      }
      else {
        return {heif_error_Invalid_input,
                heif_suberror_Unspecified,
                "'tref' has double references"};
      }
    }
  }

  return Error::Ok;
}


Error Box_tref::write(StreamWriter& writer) const
{
  if (auto error = check_for_double_references()) {
    return error;
  }

  size_t box_start = reserve_box_header_space(writer);

  for (const auto& ref : m_references) {
    uint32_t box_size = 8 + uint32_t(ref.to_track_id.size() * 4);

    // we write the BoxHeader ourselves since it is very simple
    writer.write32(box_size);
    writer.write32(ref.reference_type);

    for (uint32_t r : ref.to_track_id) {
      writer.write32(r);
    }
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_tref::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  for (const auto& ref : m_references) {
    sstr << indent << "reference with type '" << fourcc_to_string(ref.reference_type) << "'"
         << " to track IDs: ";
    for (uint32_t id : ref.to_track_id) {
      sstr << id << " ";
    }
    sstr << "\n";
  }

  return sstr.str();
}


std::vector<uint32_t> Box_tref::get_references(uint32_t ref_type) const
{
  for (const Reference& ref : m_references) {
    if (ref.reference_type == ref_type) {
      return ref.to_track_id;
    }
  }

  return {};
}


size_t Box_tref::get_number_of_references_of_type(uint32_t ref_type) const
{
  for (const Reference& ref : m_references) {
    if (ref.reference_type == ref_type) {
      return ref.to_track_id.size();
    }
  }

  return 0;
}


std::vector<uint32_t> Box_tref::get_reference_types() const
{
  std::vector<uint32_t> types;
  types.reserve(m_references.size());
  for (const auto& ref : m_references) {
    types.push_back(ref.reference_type);
  }

  return types;
}


void Box_tref::add_references(uint32_t to_track_id, uint32_t type)
{
  for (auto& ref : m_references) {
    if (ref.reference_type == type) {
      ref.to_track_id.push_back(to_track_id);
      return;
    }
  }

  Reference ref;
  ref.reference_type = type;
  ref.to_track_id = {to_track_id};

  m_references.push_back(ref);
}


Error Box_elst::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  Error err = parse_full_box_header(range);
  if (err != Error::Ok) {
    return err;
  }

  if (get_version() > 1) {
    return unsupported_version_error("edts");
  }

  uint32_t nEntries = range.read32();
  m_entries.clear();

  for (uint64_t i = 0; i < nEntries; i++) {
    if (range.eof()) {
      std::stringstream sstr;
      sstr << "edts box should contain " << nEntries << " entries, but we can only read " << i << " entries.";

      return {heif_error_Invalid_input,
              heif_suberror_End_of_data,
              sstr.str()};
    }

    Entry entry{};
    if (get_version() == 1) {
      entry.segment_duration = range.read64();
      entry.media_time = range.read64s();
    }
    else {
      entry.segment_duration = range.read32();
      entry.media_time = range.read32s();
    }

    entry.media_rate_integer = range.read16s();
    entry.media_rate_fraction = range.read16s();

    m_entries.push_back(entry);
  }

  return range.get_error();
}


Error Box_elst::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  if (m_entries.size() > std::numeric_limits<uint32_t>::max()) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "Too many entries in edit list"};
  }

  writer.write32(static_cast<uint32_t>(m_entries.size()));


  for (const auto& entry : m_entries) {
    if (get_version() == 1) {
      writer.write64(entry.segment_duration);
      writer.write64s(entry.media_time);
    }
    else {
      // The cast is valid because we check in derive_box_version() whether everything
      // fits into 32bit. If not, version 1 is used.

      writer.write32(static_cast<uint32_t>(entry.segment_duration));
      writer.write32s(static_cast<int32_t>(entry.media_time));
    }

    writer.write16s(entry.media_rate_integer);
    writer.write16s(entry.media_rate_fraction);
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


std::string Box_elst::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << FullBox::dump(indent);

  sstr << indent << "repeat list: " << ((get_flags() & Flags::Repeat_EditList) ? "yes" : "no") << "\n";

  for (const auto& entry : m_entries) {
    sstr << indent << "segment duration: " << entry.segment_duration << "\n";
    sstr << indent << "media time: " << entry.media_time << "\n";
    sstr << indent << "media rate integer: " << entry.media_rate_integer << "\n";
    sstr << indent << "media rate fraction: " << entry.media_rate_fraction << "\n";
  }

  return sstr.str();
}

void Box_elst::derive_box_version()
{
  // check whether we need 64bit values

  bool need_64bit = std::any_of(m_entries.begin(),
                                m_entries.end(),
                                [](const Entry& entry) {
                                  return (entry.segment_duration > std::numeric_limits<uint32_t>::max() ||
                                          entry.media_time > std::numeric_limits<int32_t>::max());
                                });

  if (need_64bit) {
    set_version(1);
  }
  else {
    set_version(0);
  }
}


void Box_elst::enable_repeat_mode(bool enable)
{
  uint32_t flags = get_flags();
  if (enable) {
    flags |= Flags::Repeat_EditList;
  }
  else {
    flags &= ~Flags::Repeat_EditList;
  }

  set_flags(flags);
}


void Box_elst::add_entry(const Entry& entry)
{
  m_entries.push_back(entry);
}
