/*
 * HEIF image base codec.
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

#ifndef LIBHEIF_TRACK_H
#define LIBHEIF_TRACK_H

#include "error.h"
#include "api_structs.h"
#include "libheif/heif_plugin.h"
#include "libheif/heif_sequences.h"
#include <string>
#include <memory>
#include <vector>

class HeifContext;

class HeifPixelImage;

class Chunk;

class Box_trak;


class SampleAuxInfoHelper
{
public:
  SampleAuxInfoHelper(bool interleaved = false);

  void set_aux_info_type(uint32_t aux_info_type, uint32_t aux_info_type_parameter = 0);

  Error add_sample_info(const std::vector<uint8_t>& data);

  void add_nonpresent_sample();

  void write_interleaved(const std::shared_ptr<HeifFile>& file);

  void write_all(const std::shared_ptr<Box>& parent, const std::shared_ptr<HeifFile>& file);

private:
  std::shared_ptr<Box_saiz> m_saiz;
  std::shared_ptr<Box_saio> m_saio;

  std::vector<uint8_t> m_data;

  bool m_interleaved;
};


class SampleAuxInfoReader
{
public:
  SampleAuxInfoReader(std::shared_ptr<Box_saiz>,
                      std::shared_ptr<Box_saio>,
                      const std::vector<std::shared_ptr<Chunk>>& chunks);

  heif_sample_aux_info_type get_type() const;

  Result<std::vector<uint8_t>> get_sample_info(const HeifFile* file, uint32_t sample_idx);

private:
  std::shared_ptr<Box_saiz> m_saiz;
  std::shared_ptr<Box_saio> m_saio;

  // If there is only one chunk and the SAI data sizes are constant, we do not need an offset table.
  // We just store the base offset and can directly calculate the sample offset from that.
  bool m_contiguous_and_constant_size=false;
  uint64_t m_singleChunk_offset=0;

  // For chunked data or non-constant sample sizes, we use a table with the offsets for all SAI samples.
  std::vector<uint64_t> m_sample_offsets;
};


/**
 * This structure specifies what will be written in a track and how it will be laid out in the file.
 */
struct TrackOptions
{
  ~TrackOptions()
  {
    heif_tai_clock_info_release(tai_clock_info);
  }

  // Timescale (clock ticks per second) for this track.
  uint32_t track_timescale = 90000;

  // If 'true', the aux_info data blocks will be interleaved with the compressed image.
  // This has the advantage that the aux_info is localized near the image data.
  //
  // If 'false', all aux_info will be written as one block after the compressed image data.
  // This has the advantage that no aux_info offsets have to be written.
  bool write_sample_aux_infos_interleaved = false;


  // --- TAI timestamps for samples
  heif_sample_aux_info_presence with_sample_tai_timestamps = heif_sample_aux_info_presence_none;
  heif_tai_clock_info* tai_clock_info = nullptr;

  // --- GIMI content IDs for samples

  heif_sample_aux_info_presence with_sample_content_ids = heif_sample_aux_info_presence_none;

  // --- GIMI content ID for the track

  std::string gimi_track_content_id;

  TrackOptions& operator=(const TrackOptions&);
};


const char* get_track_auxiliary_info_type(heif_compression_format format);


class Track : public ErrorBuffer {
public:
  //Track(HeifContext* ctx);

  Track(HeifContext* ctx, uint32_t track_id, const TrackOptions* info, uint32_t handler_type);

  Track(HeifContext* ctx);

  virtual ~Track() = default;

  // Allocate a Track of the correct sub-class (visual or metadata).
  // For tracks with an unsupported handler type, heif_error_Unsupported_feature/heif_suberror_Unsupported_track_type is returned.
  static Result<std::shared_ptr<Track>> alloc_track(HeifContext*, const std::shared_ptr<Box_trak>&);

  // load track from file
  virtual Error load(const std::shared_ptr<Box_trak>&);

  // This is called after creating all Track objects when reading a HEIF file.
  // We can now do initializations that require access to all tracks.
  [[nodiscard]] virtual Error initialize_after_parsing(HeifContext*, const std::vector<std::shared_ptr<Track>>& all_tracks) { return {}; }

  heif_item_id get_id() const { return m_id; }

  std::shared_ptr<HeifFile> get_file() const;

  uint32_t get_handler() const { return m_handler_type; }

  heif_auxiliary_track_info_type get_auxiliary_info_type() const;

  std::string get_auxiliary_info_type_urn() const { return m_auxiliary_info_type; }

  void set_auxiliary_info_type(heif_auxiliary_track_info_type);

  void set_auxiliary_info_type_urn(std::string t) { m_auxiliary_info_type = t; }

  bool is_visual_track() const;

  virtual bool has_alpha_channel() const { return false; }

  uint32_t get_first_cluster_sample_entry_type() const;

  Result<std::string> get_first_cluster_urim_uri() const;

  uint64_t get_duration_in_media_units() const;

  uint32_t get_timescale() const;

  // The context will compute the duration in global movie units and set this.
  void set_track_duration_in_movie_units(uint64_t total_duration, uint64_t segment_duration);

  void enable_edit_list_repeat_mode(bool enable);

  std::shared_ptr<Box_taic> get_first_cluster_taic() { return m_first_taic; }

  bool end_of_sequence_reached() const;

  // Compute some parameters after all frames have been encoded (for example: track duration).
  virtual Error finalize_track();

  const TrackOptions& get_track_info() const { return m_track_info; }

  void add_reference_to_track(uint32_t referenceType, uint32_t to_track_id);

  std::shared_ptr<const Box_tref> get_tref_box() const { return m_tref; }

  Result<heif_raw_sequence_sample*> get_next_sample_raw_data(const heif_decoding_options* options);

  std::vector<heif_sample_aux_info_type> get_sample_aux_info_types() const;

protected:
  HeifContext* m_heif_context = nullptr;
  uint32_t m_id = 0;
  uint32_t m_handler_type = 0;

  TrackOptions m_track_info;

  uint32_t m_num_samples = 0;

  struct SampleTiming {
    uint32_t sampleIdx = 0;
    uint32_t sampleInChunkIdx = 0;
    uint32_t chunkIdx = 0;
    uint64_t presentation_time = 0; // TODO
    uint64_t media_composition_time = 0; // TODO
    uint64_t media_decoding_time = 0;
    uint32_t sample_duration_media_time = 0;
    uint32_t sample_duration_presentation_time = 0; // TODO
  };
  std::vector<SampleTiming> m_presentation_timeline;
  uint64_t m_num_output_samples = 0; // Can be larger than the vector. It then repeats the playback.

  // Continuous counting through all repetitions. You have to take the modulo operation to get the
  // index into m_presentation_timeline SampleTiming table.
  // (At 30 fps, this 32 bit integer will overflow in >4 years. I think this is acceptable.)
  uint32_t m_next_sample_to_be_decoded = 0;

  // Total sequence output index.
  uint32_t m_next_sample_to_be_output = 0;
  bool     m_decoder_is_flushed = false;

  Error init_sample_timing_table();

  std::vector<std::shared_ptr<Chunk>> m_chunks;
  std::vector<uint8_t> m_chunk_data;

  std::shared_ptr<Box_moov> m_moov;
  std::shared_ptr<Box_trak> m_trak;
  std::shared_ptr<Box_tkhd> m_tkhd;
  std::shared_ptr<Box_minf> m_minf;
  std::shared_ptr<Box_mdhd> m_mdhd;
  std::shared_ptr<Box_hdlr> m_hdlr;
  std::shared_ptr<Box_stbl> m_stbl;
  std::shared_ptr<Box_stsd> m_stsd;
  std::shared_ptr<Box_stsc> m_stsc;
  std::shared_ptr<Box_stco> m_stco;
  std::shared_ptr<Box_stts> m_stts;
  std::shared_ptr<Box_ctts> m_ctts; // optional box, TODO: add only if needed
  std::shared_ptr<Box_stss> m_stss;
  std::shared_ptr<Box_stsz> m_stsz;
  std::shared_ptr<Box_elst> m_elst;

  std::shared_ptr<class Box_tref> m_tref; // optional

  std::string m_auxiliary_info_type; // only for auxiliary tracks

  // --- sample auxiliary information

  std::unique_ptr<SampleAuxInfoHelper> m_aux_helper_tai_timestamps;
  std::unique_ptr<SampleAuxInfoHelper> m_aux_helper_content_ids;

  std::unique_ptr<SampleAuxInfoReader> m_aux_reader_tai_timestamps;
  std::unique_ptr<SampleAuxInfoReader> m_aux_reader_content_ids;

  std::shared_ptr<class Box_taic> m_first_taic; // the TAIC of the first chunk


  // --- Helper functions for writing samples.

  // Call when we begin a new chunk of samples, e.g. because the compression format changed
  void add_chunk(heif_compression_format format);

  // Call to set the sample_description_box for the last added chunk.
  // Has to be called when we call add_chunk().
  // It is not merged with add_chunk() because the sample_description_box may need information from the
  // first encoded frame.
  void set_sample_description_box(std::shared_ptr<Box> sample_description_box);

  // Write the actual sample data. `tai` may be null and `gimi_contentID` may be empty.
  // In these cases, no timestamp or no contentID will be written, respectively.
  Error write_sample_data(const std::vector<uint8_t>& raw_data,
                          uint32_t sample_duration,
                          int32_t composition_time_offset,
                          bool is_sync_sample,
                          const heif_tai_timestamp_packet* tai,
                          const std::optional<std::string>& gimi_contentID);
};


#endif //LIBHEIF_TRACK_H
