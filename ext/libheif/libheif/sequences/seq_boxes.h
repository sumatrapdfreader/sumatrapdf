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

#ifndef SEQ_BOXES_H
#define SEQ_BOXES_H

#include "box.h"
#include "security_limits.h"

#include <string>
#include <memory>
#include <vector>


class Box_container : public Box {
public:
  Box_container(const char* type)
  {
    set_short_type(fourcc(type));
  }

  std::string dump(Indent&) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


// Movie Box
class Box_moov : public Box_container {
public:
  Box_moov() : Box_container("moov") {}

  const char* debug_box_name() const override { return "Movie"; }
};


// Movie Header Box
class Box_mvhd : public FullBox {
public:
  Box_mvhd()
  {
    set_short_type(fourcc("mvhd"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Movie Header"; }

  Error write(StreamWriter& writer) const override;

  void derive_box_version() override;

  double get_rate() const { return m_rate / double(0x10000); }

  float get_volume() const { return float(m_volume) / float(0x100); }

  double get_matrix_element(int idx) const;

  uint32_t get_time_scale() const { return m_timescale; }

  uint64_t get_duration() const { return m_duration; }

  void set_duration(uint64_t duration) { m_duration = duration; }

  void set_time_scale(uint32_t timescale) { m_timescale = timescale; }

  void set_next_track_id(uint32_t next_id) { m_next_track_ID = next_id; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint64_t m_creation_time = 0;
  uint64_t m_modification_time = 0;
  uint32_t m_timescale = 0;
  uint64_t m_duration = 0;

  uint32_t m_rate = 0x00010000; // typically 1.0
  uint16_t m_volume = 0x0100; // typically, full volume

  uint32_t m_matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};
  uint32_t m_next_track_ID = 0;
};


// Track Box
class Box_trak : public Box_container {
public:
  Box_trak() : Box_container("trak") {}

  const char* debug_box_name() const override { return "Track"; }
};


// Track Header Box
class Box_tkhd : public FullBox {
public:
  Box_tkhd()
  {
    set_short_type(fourcc("tkhd"));

    // set default flags according to ISO 14496-12
    set_flags(Track_enabled | Track_in_movie | Track_in_preview);
  }

  enum Flags : uint32_t {
    Track_enabled = 0x01,
    Track_in_movie = 0x02,
    Track_in_preview = 0x04,
    Track_size_is_aspect_ratio = 0x08
  };

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Track Header"; }

  Error write(StreamWriter& writer) const override;

  void derive_box_version() override;

  float get_volume() const { return float(m_volume) / float(0x100); }

  double get_matrix_element(int idx) const;

  double get_width() const { return float(m_width) / double(0x10000); }

  double get_height() const { return float(m_height) / double(0x10000); }

  uint32_t get_track_id() const { return m_track_id; }

  void set_track_id(uint32_t track_id) { m_track_id = track_id; }

  void set_resolution(double width, double height)
  {
    m_width = (uint32_t) (width * 0x10000);
    m_height = (uint32_t) (height * 0x10000);
  }

  uint64_t get_duration() const { return m_duration; }

  void set_duration(uint64_t duration) { m_duration = duration; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint64_t m_creation_time = 0;
  uint64_t m_modification_time = 0;
  uint32_t m_track_id = 0;
  uint64_t m_duration = 0;

  uint16_t m_layer = 0;
  uint16_t m_alternate_group = 0;
  uint16_t m_volume = 0x0100; // typically, full volume

  uint32_t m_matrix[9] = {0x00010000, 0, 0, 0, 0x00010000, 0, 0, 0, 0x40000000};

  uint32_t m_width = 0;
  uint32_t m_height = 0;
};


// Media Box
class Box_mdia : public Box_container {
public:
  Box_mdia() : Box_container("mdia") {}

  const char* debug_box_name() const override { return "Media"; }
};


// Media Header Box
class Box_mdhd : public FullBox {
public:
  Box_mdhd()
  {
    set_short_type(fourcc("mdhd"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Media Header"; }

  Error write(StreamWriter& writer) const override;

  void derive_box_version() override;

  double get_matrix_element(int idx) const;

  uint32_t get_timescale() const { return m_timescale; }

  void set_timescale(uint32_t timescale) { m_timescale = timescale; }

  uint64_t get_duration() const { return m_duration; }

  void set_duration(uint64_t duration) { m_duration = duration; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint64_t m_creation_time = 0;
  uint64_t m_modification_time = 0;
  uint32_t m_timescale = 0;
  uint64_t m_duration = 0;

  char m_language[4] = {'u', 'n', 'k', 0};
};


// Media Information Box (container)
class Box_minf : public Box_container {
public:
  Box_minf() : Box_container("minf") {}

  const char* debug_box_name() const override { return "Media Information"; }
};


// Video Media Header
class Box_vmhd : public FullBox {
public:
  Box_vmhd()
  {
    set_short_type(fourcc("vmhd"));
    set_flags(1);
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Video Media Header"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint16_t m_graphics_mode = 0;
  uint16_t m_op_color[3] = {0, 0, 0};
};


// Null Media Header
class Box_nmhd : public FullBox {
public:
  Box_nmhd()
  {
    set_short_type(fourcc("nmhd"));
    set_flags(1);
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Null Media Header"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;
};


// Sample Table Box (container)
class Box_stbl : public Box_container {
public:
  Box_stbl() : Box_container("stbl") {}

  const char* debug_box_name() const override { return "Sample Table"; }
};


// Sample Description Box
class Box_stsd : public FullBox {
public:
  Box_stsd()
  {
    set_short_type(fourcc("stsd"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample Description"; }

  Error write(StreamWriter& writer) const override;

  std::shared_ptr<const class Box> get_sample_entry(size_t idx) const
  {
    if (idx >= m_sample_entries.size()) {
      return nullptr;
    } else {
      return m_sample_entries[idx];
    }
  }

  void add_sample_entry(std::shared_ptr<class Box> entry)
  {
    m_sample_entries.push_back(entry);
  }

  size_t get_num_sample_entries() const { return m_sample_entries.size(); }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<std::shared_ptr<class Box>> m_sample_entries;
};


// Decoding Time to Sample Box
class Box_stts : public FullBox {
public:
  Box_stts()
  {
    set_short_type(fourcc("stts"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Decoding Time to Sample"; }

  Error write(StreamWriter& writer) const override;

  struct TimeToSample {
    uint32_t sample_count;
    uint32_t sample_delta;
  };

  uint32_t get_sample_duration(uint32_t sample_idx);

  void append_sample_duration(uint32_t duration);

  uint64_t get_total_duration(bool include_last_frame_duration);

  uint32_t get_number_of_samples() const;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<TimeToSample> m_entries;
  MemoryHandle m_memory_handle;
};


// Composition Time to Sample Box
class Box_ctts : public FullBox {
public:
  Box_ctts()
  {
    set_short_type(fourcc("ctts"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Composition Time to Sample"; }

  Error write(StreamWriter& writer) const override;

  struct OffsetToSample {
    uint32_t sample_count;
    int32_t sample_offset;   // either uint32_t or int32_t, we assume that all uint32_t values will also fit into int32_t
  };

  int32_t get_sample_offset(uint32_t sample_idx);

  void append_sample_offset(int32_t offset);

  bool is_constant_offset() const;

  void derive_box_version() override;

  int32_t compute_min_offset() const;

  uint32_t get_number_of_samples() const;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<OffsetToSample> m_entries;
  MemoryHandle m_memory_handle;
};


// Sample to Chunk Box
class Box_stsc : public FullBox {
public:
  Box_stsc()
  {
    set_short_type(fourcc("stsc"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample to Chunk"; }

  Error write(StreamWriter& writer) const override;

  struct SampleToChunk {
    uint32_t first_chunk;
    uint32_t samples_per_chunk;
    uint32_t sample_description_index;
  };

  const std::vector<SampleToChunk>& get_chunks() const { return m_entries; }

  // idx counting starts at 1
  const SampleToChunk* get_chunk(uint32_t idx) const;

  void add_chunk(uint32_t description_index);

  void increase_samples_in_chunk(uint32_t nFrames);

  bool last_chunk_empty() const {
    assert(!m_entries.empty());

    return m_entries.back().samples_per_chunk == 0;
  }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<SampleToChunk> m_entries;
  MemoryHandle m_memory_handle;
};


// Chunk Offset Box
class Box_stco : public FullBox {
public:
  Box_stco()
  {
    set_short_type(fourcc("stco"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample Offset"; }

  Error write(StreamWriter& writer) const override;

  void add_chunk_offset(uint32_t offset) { m_offsets.push_back(offset); }

  const std::vector<uint32_t>& get_offsets() const { return m_offsets; }

  size_t get_number_of_chunks() const { return m_offsets.size(); }

  void patch_file_pointers(StreamWriter&, size_t offset) override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<uint32_t> m_offsets;
  MemoryHandle m_memory_handle;

  mutable size_t m_offset_start_pos = 0;
};


// Sample Size Box
class Box_stsz : public FullBox {
public:
  Box_stsz()
  {
    set_short_type(fourcc("stsz"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample Size"; }

  Error write(StreamWriter& writer) const override;

  bool has_fixed_sample_size() const { return m_fixed_sample_size != 0; }

  uint32_t get_fixed_sample_size() const { return m_fixed_sample_size; }

  uint32_t num_samples() const { return m_sample_count; }

  const std::vector<uint32_t>& get_sample_sizes() const { return m_sample_sizes; }

  void append_sample_size(uint32_t size);

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_fixed_sample_size = 0;
  uint32_t m_sample_count = 0;
  std::vector<uint32_t> m_sample_sizes;
  MemoryHandle m_memory_handle;
};


// Sync Sample Box
class Box_stss : public FullBox {
public:
  Box_stss()
  {
    set_short_type(fourcc("stss"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sync Sample"; }

  Error write(StreamWriter& writer) const override;

  void add_sync_sample(uint32_t sample_idx) { m_sync_samples.push_back(sample_idx); }

  // when this is set, the Box will compute whether it can be skipped
  void set_total_number_of_samples(uint32_t num_samples);

  // bool skip_box() const override { return m_all_samples_are_sync_samples; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<uint32_t> m_sync_samples;
  MemoryHandle m_memory_handle;

  bool m_all_samples_are_sync_samples = false;
};


struct CodingConstraints {
  bool all_ref_pics_intra = false;
  bool intra_pred_used = false;
  uint8_t max_ref_per_pic = 0; // 4 bit
};


// Coding Constraints Box
class Box_ccst : public FullBox {
public:
  Box_ccst()
  {
    set_short_type(fourcc("ccst"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Coding Constraints"; }

  Error write(StreamWriter& writer) const override;

  void set_coding_constraints(const CodingConstraints& c) { m_codingConstraints = c; }

  const CodingConstraints& get_coding_constraints() const { return m_codingConstraints; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  CodingConstraints m_codingConstraints;
};


class Box_auxi : public FullBox {
public:
  Box_auxi()
  {
    set_short_type(fourcc("auxi"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Auxiliary Info Type"; }

  Error write(StreamWriter& writer) const override;

  void set_aux_track_type_urn(const std::string& t) { m_aux_track_type = t; }

  std::string get_aux_track_type_urn() const { return m_aux_track_type; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::string m_aux_track_type;
};


struct VisualSampleEntry {
  // from SampleEntry
  //const unsigned int(8)[6] reserved = 0;
  uint16_t data_reference_index = 1;

  // VisualSampleEntry

  uint16_t pre_defined = 0;
  //uint16_t reserved = 0;
  uint32_t pre_defined2[3] = {0, 0, 0};
  uint16_t width = 0;
  uint16_t height = 0;
  uint32_t horizresolution = 0x00480000; // 72 dpi
  uint32_t vertresolution = 0x00480000; // 72 dpi
  //uint32_t reserved = 0;
  uint16_t frame_count = 1;
  std::string compressorname; // max 32 characters
  uint16_t depth = 0x0018;
  int16_t pre_defined3 = -1;
  // other boxes from derived specifications
  //std::shared_ptr<Box_clap> clap; // optional
  //std::shared_ptr<Box_pixi> pixi; // optional

  double get_horizontal_resolution() const { return horizresolution / double(0x10000); }

  double get_vertical_resolution() const { return vertresolution / double(0x10000); }

  Error parse(BitstreamRange& range, const heif_security_limits*);

  Error write(StreamWriter& writer) const;

  std::string dump(Indent&) const;
};


class Box_VisualSampleEntry : public Box {
public:
  Error write(StreamWriter& writer) const override;

  std::string dump(Indent&) const override;

  const VisualSampleEntry& get_VisualSampleEntry_const() const { return m_visualSampleEntry; }

  VisualSampleEntry& get_VisualSampleEntry() { return m_visualSampleEntry; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  VisualSampleEntry m_visualSampleEntry;
};


class Box_URIMetaSampleEntry : public Box {
public:
  Box_URIMetaSampleEntry()
  {
    set_short_type(fourcc("urim"));
  }

  Error write(StreamWriter& writer) const override;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "URI Meta Sample Entry"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  // from SampleEntry
  //const unsigned int(8)[6] reserved = 0;
  uint16_t data_reference_index;
};


class Box_uri : public FullBox {
public:
  Box_uri()
  {
    set_short_type(fourcc("uri "));
  }

  void set_uri(std::string uri) { m_uri = uri; }

  std::string get_uri() const { return m_uri; }

  Error write(StreamWriter& writer) const override;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "URI"; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  std::string m_uri;
};


// Sample to Group
class Box_sbgp : public FullBox {
public:
  Box_sbgp()
  {
    set_short_type(fourcc("sbgp"));
  }

  void derive_box_version() override;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample to Group"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_grouping_type = 0; // 4cc
  std::optional<uint32_t> m_grouping_type_parameter;

  struct Entry {
    uint32_t sample_count;
    uint32_t group_description_index;
  };

  std::vector<Entry> m_entries;
  MemoryHandle m_memory_handle;
};


class SampleGroupEntry
{
public:
  virtual ~SampleGroupEntry() = default;

  virtual std::string dump() const = 0;

  virtual Error write(StreamWriter& writer) const = 0;

  virtual Error parse(BitstreamRange& range, const heif_security_limits*) = 0;
};


class SampleGroupEntry_refs : public SampleGroupEntry
{
public:
  std::string dump() const override;

  Error write(StreamWriter& writer) const override;

  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_sample_id;
  std::vector<uint32_t> m_direct_reference_sample_id;
};


// Sample Group Description
class Box_sgpd : public FullBox {
public:
  Box_sgpd()
  {
    set_short_type(fourcc("sgpd"));
  }

  void derive_box_version() override;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample Group Description"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_grouping_type = 0; // 4cc
  std::optional<uint32_t> m_default_length; // version 1  (0 -> variable length)
  std::optional<uint32_t> m_default_sample_description_index;; // version >= 2

  struct Entry {
    uint32_t description_length = 0; // if version==1 && m_default_length == 0
    std::shared_ptr<SampleGroupEntry> sample_group_entry;
  };

  std::vector<Entry> m_entries;
};

// Bitrate
class Box_btrt : public FullBox {
public:
  Box_btrt()
  {
    set_short_type(fourcc("btrt"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Bitrate"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_bufferSizeDB;
  uint32_t m_maxBitrate;
  uint32_t m_avgBitrate;
};


class Box_saiz : public FullBox {
public:
  Box_saiz()
  {
    set_short_type(fourcc("saiz"));
  }

  void set_aux_info_type(uint32_t aux_info_type, uint32_t aux_info_type_parameter = 0);

  uint32_t get_aux_info_type() const { return m_aux_info_type; }

  uint32_t get_aux_info_type_parameter() const { return m_aux_info_type_parameter; }

  void add_sample_size(uint8_t s);

  void add_nonpresent_sample() { add_sample_size(0); }

  uint8_t get_sample_size(uint32_t idx);

  bool have_samples_constant_size() const { return m_default_sample_info_size != 0; }

  uint32_t get_num_samples() const { return m_num_samples; }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample Auxiliary Information Sizes"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  uint32_t m_aux_info_type = 0;
  uint32_t m_aux_info_type_parameter = 0;
  uint8_t  m_default_sample_info_size = 0;  // 0 -> variable length
  uint32_t m_num_samples = 0; // needed in case we are using the default sample size

  std::vector<uint8_t> m_sample_sizes;
  MemoryHandle m_memory_handle;
};


class Box_saio : public FullBox {
public:
  Box_saio()
  {
    set_short_type(fourcc("saio"));
  }

  void set_aux_info_type(uint32_t aux_info_type, uint32_t aux_info_type_parameter = 0);

  uint32_t get_aux_info_type() const { return m_aux_info_type; }

  uint32_t get_aux_info_type_parameter() const { return m_aux_info_type_parameter; }

  void add_chunk_offset(uint64_t offset);

  // If this is 1, the SAI data of all samples is written contiguously in the file.
  size_t get_num_chunks() const { return m_chunk_offset.size(); }

  uint64_t get_chunk_offset(uint32_t idx) const;

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Sample Auxiliary Information Offsets"; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  void patch_file_pointers(StreamWriter& writer, size_t offset) override;

private:
  uint32_t m_aux_info_type = 0;
  uint32_t m_aux_info_type_parameter = 0;

  bool m_need_64bit = false;
  mutable uint64_t m_offset_start_pos;

  // If |chunk_offset|==1, the SAI data of all samples is stored contiguously in the file
  std::vector<uint64_t> m_chunk_offset;

  MemoryHandle m_memory_handle;
};


class Box_sdtp : public FullBox {
public:
  Box_sdtp()
  {
    set_short_type(fourcc("sdtp"));
  }

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Independent and Disposable Samples"; }

  // Error write(StreamWriter& writer) const override;

  uint8_t get_is_leading(uint32_t sampleIdx) const { return (m_sample_information[sampleIdx] >> 6) & 3; }

  uint8_t get_depends_on(uint32_t sampleIdx) const { return (m_sample_information[sampleIdx] >> 4) & 3; }

  uint8_t get_is_depended_on(uint32_t sampleIdx) const { return (m_sample_information[sampleIdx] >> 2) & 3; }

  uint8_t get_has_redundancy(uint32_t sampleIdx) const { return (m_sample_information[sampleIdx]) & 3; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  std::vector<uint8_t> m_sample_information;
};


class Box_tref : public Box
{
public:
  Box_tref()
  {
    set_short_type(fourcc("tref"));
  }

  struct Reference {
    uint32_t reference_type;
    std::vector<uint32_t> to_track_id;
  };

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Track Reference"; }

  std::vector<uint32_t> get_references(uint32_t ref_type) const;

  size_t get_number_of_references_of_type(uint32_t ref_type) const;

  size_t get_number_of_reference_types() const { return m_references.size(); }

  std::vector<uint32_t> get_reference_types() const;

  void add_references(uint32_t to_track_id, uint32_t type);

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

  Error check_for_double_references() const;

private:
  std::vector<Reference> m_references;
};


// Edit List container
class Box_edts : public Box_container {
public:
  Box_edts() : Box_container("edts") {}

  const char* debug_box_name() const override { return "Edit List Container"; }
};


class Box_elst : public FullBox
{
public:
  Box_elst()
  {
    set_short_type(fourcc("elst"));
  }

  enum Flags {
    Repeat_EditList = 0x01
  };

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Edit List"; }

  void enable_repeat_mode(bool enable);

  bool is_repeat_mode() const { return get_flags() & Flags::Repeat_EditList; }

  struct Entry {
    uint64_t segment_duration = 0;
    int64_t media_time = 0;
    int16_t media_rate_integer = 1;
    int16_t media_rate_fraction = 0;
  };

  void add_entry(const Entry&);

  size_t num_entries() const { return m_entries.size(); }

  Entry get_entry(uint32_t entry) const { return m_entries[entry]; }

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

  Error write(StreamWriter& writer) const override;

public:
  void derive_box_version() override;

private:
  std::vector<Entry> m_entries;
};

#endif //SEQ_BOXES_H
