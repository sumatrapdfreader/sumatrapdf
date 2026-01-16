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

#include <cstring>
#include "track.h"
#include "context.h"
#include "sequences/seq_boxes.h"
#include "sequences/chunk.h"
#include "sequences/track_visual.h"
#include "sequences/track_metadata.h"
#include "api_structs.h"
#include <limits>


TrackOptions& TrackOptions::operator=(const TrackOptions& src)
{
  if (&src == this) {
    return *this;
  }

  this->track_timescale = src.track_timescale;
  this->write_sample_aux_infos_interleaved = src.write_sample_aux_infos_interleaved;
  this->with_sample_tai_timestamps = src.with_sample_tai_timestamps;

  if (src.tai_clock_info) {
    this->tai_clock_info = heif_tai_clock_info_alloc();
    heif_tai_clock_info_copy(this->tai_clock_info, src.tai_clock_info);
  }
  else {
    this->tai_clock_info = nullptr;
  }

  this->with_sample_content_ids = src.with_sample_content_ids;
  this->gimi_track_content_id = src.gimi_track_content_id;

  return *this;
}


SampleAuxInfoHelper::SampleAuxInfoHelper(bool interleaved)
  : m_interleaved(interleaved)
{
  m_saiz = std::make_shared<Box_saiz>();
  m_saio = std::make_shared<Box_saio>();
}


void SampleAuxInfoHelper::set_aux_info_type(uint32_t aux_info_type, uint32_t aux_info_type_parameter)
{
  m_saiz->set_aux_info_type(aux_info_type, aux_info_type_parameter);
  m_saio->set_aux_info_type(aux_info_type, aux_info_type_parameter);
}

Error SampleAuxInfoHelper::add_sample_info(const std::vector<uint8_t>& data)
{
  if (data.size() > 0xFF) {
    return {
      heif_error_Encoding_error,
      heif_suberror_Unspecified,
      "Encoded sample auxiliary information exceeds maximum size"
    };
  }

  m_saiz->add_sample_size(static_cast<uint8_t>(data.size()));

  m_data.insert(m_data.end(), data.begin(), data.end());

  return Error::Ok;
}

void SampleAuxInfoHelper::add_nonpresent_sample()
{
  m_saiz->add_nonpresent_sample();
}


void SampleAuxInfoHelper::write_interleaved(const std::shared_ptr<HeifFile>& file)
{
  if (m_interleaved && !m_data.empty()) {
    // TODO: I think this does not work because the image data does not know that there is SAI in-between
    uint64_t pos = file->append_mdat_data(m_data);
    m_saio->add_chunk_offset(pos);

    m_data.clear();
  }
}

void SampleAuxInfoHelper::write_all(const std::shared_ptr<Box>& parent, const std::shared_ptr<HeifFile>& file)
{
  parent->append_child_box(m_saiz);
  parent->append_child_box(m_saio);

  if (!m_data.empty()) {
    uint64_t pos = file->append_mdat_data(m_data);
    m_saio->add_chunk_offset(pos);
  }
}


SampleAuxInfoReader::SampleAuxInfoReader(std::shared_ptr<Box_saiz> saiz,
                                         std::shared_ptr<Box_saio> saio,
                                         const std::vector<std::shared_ptr<Chunk>>& chunks)
{
  m_saiz = saiz;
  m_saio = saio;

  bool oneChunk = (saio->get_num_chunks() == 1);

  uint32_t current_chunk = 0;
  uint64_t offset = saio->get_chunk_offset(0);
  uint32_t nSamples = saiz->get_num_samples();

  m_contiguous_and_constant_size = (oneChunk && m_saiz->have_samples_constant_size());

  if (m_contiguous_and_constant_size) {
    m_singleChunk_offset = offset;
  }
  else {
    m_sample_offsets.resize(nSamples);

    for (uint32_t i = 0; i < nSamples; i++) {
      if (!oneChunk && i > chunks[current_chunk]->last_sample_number()) {
        current_chunk++;
        assert(current_chunk < chunks.size());
        offset = saio->get_chunk_offset(current_chunk);
      }

      m_sample_offsets[i] = offset;
      offset += saiz->get_sample_size(i);
    }
  }
}


heif_sample_aux_info_type SampleAuxInfoReader::get_type() const
{
  heif_sample_aux_info_type type;
  type.type = m_saiz->get_aux_info_type();
  type.parameter = m_saiz->get_aux_info_type_parameter();
  return type;
}


Result<std::vector<uint8_t> > SampleAuxInfoReader::get_sample_info(const HeifFile* file, uint32_t sample_idx)
{
  uint64_t offset;
  uint8_t size;

  if (m_contiguous_and_constant_size) {
    size = m_saiz->get_sample_size(0);
    offset = m_singleChunk_offset + sample_idx * size;
  }
  else {
    size = m_saiz->get_sample_size(sample_idx);
    if (size > 0) {
      if (sample_idx >= m_sample_offsets.size()) {
        return {};
      }

      offset = m_sample_offsets[sample_idx];
    }
  }

  std::vector<uint8_t> data;

  if (size > 0) {
    Error err = file->append_data_from_file_range(data, offset, size);
    if (err) {
      return err;
    }
  }

  return data;
}


std::shared_ptr<HeifFile> Track::get_file() const
{
  return m_heif_context->get_heif_file();
}


Track::Track(HeifContext* ctx)
{
  m_heif_context = ctx;
}


Error Track::load(const std::shared_ptr<Box_trak>& trak_box)
{
  m_trak = trak_box;

  auto tkhd = trak_box->get_child_box<Box_tkhd>();
  if (!tkhd) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'tkhd' box."
    };
  }

  m_tkhd = tkhd;

  m_id = tkhd->get_track_id();

  auto edts = trak_box->get_child_box<Box_edts>();
  if (edts) {
    m_elst = edts->get_child_box<Box_elst>();
  }

  auto mdia = trak_box->get_child_box<Box_mdia>();
  if (!mdia) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'mdia' box."
    };
  }

  m_tref = trak_box->get_child_box<Box_tref>();

  auto hdlr = mdia->get_child_box<Box_hdlr>();
  if (!hdlr) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'hdlr' box."
    };
  }

  m_handler_type = hdlr->get_handler_type();

  m_minf = mdia->get_child_box<Box_minf>();
  if (!m_minf) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'minf' box."
    };
  }

  m_mdhd = mdia->get_child_box<Box_mdhd>();
  if (!m_mdhd) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'mdhd' box."
    };
  }

  auto stbl = m_minf->get_child_box<Box_stbl>();
  if (!stbl) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'stbl' box."
    };
  }

  m_stsd = stbl->get_child_box<Box_stsd>();
  if (!m_stsd) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'stsd' box."
    };
  }

  m_stsc = stbl->get_child_box<Box_stsc>();
  if (!m_stsc) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'stsc' box."
    };
  }

  m_stco = stbl->get_child_box<Box_stco>();
  if (!m_stco) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'stco' box."
    };
  }

  m_stsz = stbl->get_child_box<Box_stsz>();
  if (!m_stsz) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'stsz' box."
    };
  }

  m_stts = stbl->get_child_box<Box_stts>();
  if (!m_stts) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'stts' box."
    };
  }

  // --- check that number of samples in various boxes are consistent

  if (m_stts->get_number_of_samples() != m_stsz->num_samples()) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Number of samples in 'stts' and 'stsz' is inconsistent."
    };
  }

  if (m_ctts && m_ctts->get_number_of_samples() != m_stsz->num_samples()) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Number of samples in 'ctts' and 'stsz' is inconsistent."
    };
  }

  const std::vector<uint32_t>& chunk_offsets = m_stco->get_offsets();
  assert(chunk_offsets.size() <= (size_t) std::numeric_limits<uint32_t>::max()); // There cannot be more than uint32_t chunks.

  uint32_t current_sample_idx = 0;
  int32_t previous_sample_description_index = -1;

  for (size_t chunk_idx = 0; chunk_idx < chunk_offsets.size(); chunk_idx++) {
    auto* s2c = m_stsc->get_chunk(static_cast<uint32_t>(chunk_idx + 1));
    if (!s2c) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "'stco' box references a non-existing chunk."
      };
    }

    Box_stsc::SampleToChunk sampleToChunk = *s2c;

    auto sample_description = m_stsd->get_sample_entry(sampleToChunk.sample_description_index - 1);
    if (!sample_description) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Track references a non-existing sample description."
      };
    }

    if (auto auxi = sample_description->get_child_box<Box_auxi>()) {
      m_auxiliary_info_type = auxi->get_aux_track_type_urn();
    }

    if (m_first_taic == nullptr) {
      auto taic = sample_description->get_child_box<Box_taic>();
      if (taic) {
        m_first_taic = taic;
      }
    }

    if (current_sample_idx + sampleToChunk.samples_per_chunk > m_stsz->num_samples()) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Number of samples in 'stsc' box exceeds sample sizes in 'stsz' box."
      };
    }

    auto chunk = std::make_shared<Chunk>(m_heif_context, m_id,
                                         current_sample_idx, sampleToChunk.samples_per_chunk,
                                         m_stco->get_offsets()[chunk_idx],
                                         m_stsz);

    if (auto visualSampleDescription = std::dynamic_pointer_cast<const Box_VisualSampleEntry>(sample_description)) {
      if (chunk_idx > 0 && (int32_t) sampleToChunk.sample_description_index == previous_sample_description_index) {
        // reuse decoder from previous chunk if it uses the sample sample_description_index
        chunk->set_decoder(m_chunks[chunk_idx - 1]->get_decoder());
      }
      else {
        // use a new decoder
        chunk->set_decoder(Decoder::alloc_for_sequence_sample_description_box(visualSampleDescription));
      }
    }

    m_chunks.push_back(chunk);

    current_sample_idx += sampleToChunk.samples_per_chunk;
    previous_sample_description_index = sampleToChunk.sample_description_index;
  }

  // --- read sample auxiliary information boxes

  std::vector<std::shared_ptr<Box_saiz> > saiz_boxes = stbl->get_child_boxes<Box_saiz>();
  std::vector<std::shared_ptr<Box_saio> > saio_boxes = stbl->get_child_boxes<Box_saio>();

  if (saio_boxes.size() != saiz_boxes.size()) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Boxes 'saiz' and `saio` must come in pairs."
    };
  }

  for (const auto& saiz : saiz_boxes) {
    uint32_t aux_info_type = saiz->get_aux_info_type();
    uint32_t aux_info_type_parameter = saiz->get_aux_info_type_parameter();

    // find the corresponding saio box

    std::shared_ptr<Box_saio> saio;
    for (const auto& candidate : saio_boxes) {
      if (candidate->get_aux_info_type() == aux_info_type &&
          candidate->get_aux_info_type_parameter() == aux_info_type_parameter) {
        saio = candidate;
        break;
      }
    }

    if (saio) {
      if (saio->get_num_chunks() != 1 &&
          saio->get_num_chunks() != m_stco->get_number_of_chunks()) {
        return Error{
          heif_error_Invalid_input,
          heif_suberror_Unspecified,
          "Invalid number of chunks in 'saio' box."
        };
      }

      if (aux_info_type == fourcc("suid")) {
        m_aux_reader_content_ids = std::make_unique<SampleAuxInfoReader>(saiz, saio, m_chunks);
      }

      if (aux_info_type == fourcc("stai")) {
        m_aux_reader_tai_timestamps = std::make_unique<SampleAuxInfoReader>(saiz, saio, m_chunks);
      }
    }
    else {
      return Error{
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "'saiz' box without matching 'saio' box."
      };
    }
  }

  // --- read track properties

  if (auto meta = trak_box->get_child_box<Box_meta>()) {
    auto iloc = meta->get_child_box<Box_iloc>();
    auto idat = meta->get_child_box<Box_idat>();

    auto iinf = meta->get_child_box<Box_iinf>();
    if (iinf) {
      auto infe_boxes = iinf->get_child_boxes<Box_infe>();
      for (const auto& box : infe_boxes) {
        if (box->get_item_type_4cc() == fourcc("uri ") &&
            box->get_item_uri_type() == "urn:uuid:15beb8e4-944d-5fc6-a3dd-cb5a7e655c73") {
          heif_item_id id = box->get_item_ID();

          std::vector<uint8_t> data;
          Error err = iloc->read_data(id, m_heif_context->get_heif_file()->get_reader(), idat, &data, m_heif_context->get_security_limits());
          if (err) {
            // TODO
          }

          Result contentIdResult = vector_to_string(data);
          if (!contentIdResult) {
            // TODO
          }

          m_track_info.gimi_track_content_id = *contentIdResult;
        }
      }
    }
  }


  // --- security checks

  if (m_stsz->num_samples() > m_heif_context->get_security_limits()->max_sequence_frames) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "Security limit for maximum number of sequence frames exceeded"
    };
  }


  // --- initialize track tables

  Error err = init_sample_timing_table();
  if (err) {
    return err;
  }

  return {};
}


Track::Track(HeifContext* ctx, uint32_t track_id, const TrackOptions* options, uint32_t handler_type)
{
  m_heif_context = ctx;

  m_moov = ctx->get_heif_file()->get_moov_box();
  assert(m_moov);

  // --- find next free track ID

  if (track_id == 0) {
    track_id = 1; // minimum track ID

    for (const auto& track : m_moov->get_child_boxes<Box_trak>()) {
      auto tkhd = track->get_child_box<Box_tkhd>();

      if (tkhd->get_track_id() >= track_id) {
        track_id = tkhd->get_track_id() + 1;
      }
    }

    auto mvhd = m_moov->get_child_box<Box_mvhd>();
    mvhd->set_next_track_id(track_id + 1);

    m_id = track_id;
  }

  m_trak = std::make_shared<Box_trak>();
  m_moov->append_child_box(m_trak);

  m_tkhd = std::make_shared<Box_tkhd>();
  m_trak->append_child_box(m_tkhd);
  m_tkhd->set_track_id(track_id);

  auto mdia = std::make_shared<Box_mdia>();
  m_trak->append_child_box(mdia);

  m_mdhd = std::make_shared<Box_mdhd>();
  m_mdhd->set_timescale(options ? options->track_timescale : 90000);
  mdia->append_child_box(m_mdhd);

  m_hdlr = std::make_shared<Box_hdlr>();
  mdia->append_child_box(m_hdlr);
  m_hdlr->set_handler_type(handler_type);

  m_minf = std::make_shared<Box_minf>();
  mdia->append_child_box(m_minf);

  // add (unused) 'dinf'

  auto dinf = std::make_shared<Box_dinf>();
  auto dref = std::make_shared<Box_dref>();
  auto url = std::make_shared<Box_url>();
  m_minf->append_child_box(dinf);
  dinf->append_child_box(dref);
  dref->append_child_box(url);

  // vmhd is added in Track_Visual

  m_stbl = std::make_shared<Box_stbl>();
  m_minf->append_child_box(m_stbl);

  m_stsd = std::make_shared<Box_stsd>();
  m_stbl->append_child_box(m_stsd);

  m_stts = std::make_shared<Box_stts>();
  m_stbl->append_child_box(m_stts);

  m_ctts = std::make_shared<Box_ctts>();
  // The ctts box will be added in finalize_track(), but only there is frame-reordering.

  m_stsc = std::make_shared<Box_stsc>();
  m_stbl->append_child_box(m_stsc);

  m_stsz = std::make_shared<Box_stsz>();
  m_stbl->append_child_box(m_stsz);

  m_stco = std::make_shared<Box_stco>();
  m_stbl->append_child_box(m_stco);

  m_stss = std::make_shared<Box_stss>();
  m_stbl->append_child_box(m_stss);

  if (options) {
    m_track_info = *options;

    if (m_track_info.with_sample_tai_timestamps != heif_sample_aux_info_presence_none) {
      m_aux_helper_tai_timestamps = std::make_unique<SampleAuxInfoHelper>(m_track_info.write_sample_aux_infos_interleaved);
      m_aux_helper_tai_timestamps->set_aux_info_type(fourcc("stai"));
    }

    if (m_track_info.with_sample_content_ids != heif_sample_aux_info_presence_none) {
      m_aux_helper_content_ids = std::make_unique<SampleAuxInfoHelper>(m_track_info.write_sample_aux_infos_interleaved);
      m_aux_helper_content_ids->set_aux_info_type(fourcc("suid"));
    }

    if (!options->gimi_track_content_id.empty()) {
      auto hdlr_box = std::make_shared<Box_hdlr>();
      hdlr_box->set_handler_type(fourcc("meta"));

      auto uuid_box = std::make_shared<Box_infe>();
      uuid_box->set_item_type_4cc(fourcc("uri "));
      uuid_box->set_item_uri_type("urn:uuid:15beb8e4-944d-5fc6-a3dd-cb5a7e655c73");
      uuid_box->set_item_ID(1);

      auto iinf_box = std::make_shared<Box_iinf>();
      iinf_box->append_child_box(uuid_box);

      std::vector<uint8_t> track_uuid_vector;
      track_uuid_vector.insert(track_uuid_vector.begin(),
                               options->gimi_track_content_id.c_str(),
                               options->gimi_track_content_id.c_str() + options->gimi_track_content_id.length() + 1);

      auto iloc_box = std::make_shared<Box_iloc>();
      iloc_box->append_data(1, track_uuid_vector, 1);

      auto meta_box = std::make_shared<Box_meta>();
      meta_box->append_child_box(hdlr_box);
      meta_box->append_child_box(iinf_box);
      meta_box->append_child_box(iloc_box);

      m_trak->append_child_box(meta_box);
    }
  }
}


Result<std::shared_ptr<Track> > Track::alloc_track(HeifContext* ctx, const std::shared_ptr<Box_trak>& trak)
{
  auto mdia = trak->get_child_box<Box_mdia>();
  if (!mdia) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'mdia' box."
    };
  }

  auto hdlr = mdia->get_child_box<Box_hdlr>();
  if (!hdlr) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Track has no 'hdlr' box."
    };
  }

  std::shared_ptr<Track> track;

  switch (hdlr->get_handler_type()) {
    case fourcc("pict"):
    case fourcc("vide"):
    case fourcc("auxv"):
      track = std::make_shared<Track_Visual>(ctx);
      break;
    case fourcc("meta"):
      track = std::make_shared<Track_Metadata>(ctx);
      break;
    default: {
      std::stringstream sstr;
      sstr << "Track with unsupported handler type '" << fourcc_to_string(hdlr->get_handler_type()) << "'.";
      return Error{
        heif_error_Unsupported_feature,
        heif_suberror_Unsupported_track_type,
        sstr.str()
      };
    }
  }

  assert(track);
  Error loadError = track->load(trak);
  if (loadError) {
    return loadError;
  }

  return {track};
}


bool Track::is_visual_track() const
{
  return (m_handler_type == fourcc("pict") ||
          m_handler_type == fourcc("vide"));
}


static const char* cAuxType_alpha_miaf = "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha";
static const char* cAuxType_alpha_hevc = "urn:mpeg:hevc:2015:auxid:1";
static const char* cAuxType_alpha_avc = "urn:mpeg:avc:2015:auxid:1";

heif_auxiliary_track_info_type Track::get_auxiliary_info_type() const
{
  if (m_auxiliary_info_type == cAuxType_alpha_miaf ||
      m_auxiliary_info_type == cAuxType_alpha_hevc ||
      m_auxiliary_info_type == cAuxType_alpha_avc) {
    return heif_auxiliary_track_info_type_alpha;
  }
  else {
    return heif_auxiliary_track_info_type_unknown;
  }
}

const char* get_track_auxiliary_info_type(heif_compression_format format)
{
  switch (format) {
    case heif_compression_HEVC:
      return cAuxType_alpha_hevc;
    case heif_compression_AVC:
      return cAuxType_alpha_avc;
    case heif_compression_AV1:
      return cAuxType_alpha_miaf;
    default:
      return cAuxType_alpha_miaf; // TODO: is this correct for all remaining compression types ?
  }
}

// TODO: is this correct or should we set the aux_info_type depending on the compression format?
void Track::set_auxiliary_info_type(heif_auxiliary_track_info_type type)
{
  switch (type) {
    case heif_auxiliary_track_info_type_alpha:
      m_auxiliary_info_type = cAuxType_alpha_miaf;
      break;
    default:
      m_auxiliary_info_type.clear();
      break;
  }
}


uint32_t Track::get_first_cluster_sample_entry_type() const
{
  if (m_stsd->get_num_sample_entries() == 0) {
    return 0; // TODO: error ? Or can we assume at this point that there is at least one sample entry?
  }

  return m_stsd->get_sample_entry(0)->get_short_type();
}


Result<std::string> Track::get_first_cluster_urim_uri() const
{
  if (m_stsd->get_num_sample_entries() == 0) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "This track has no sample entries."
    };
  }

  std::shared_ptr<const Box> sampleEntry = m_stsd->get_sample_entry(0);
  auto urim = std::dynamic_pointer_cast<const Box_URIMetaSampleEntry>(sampleEntry);
  if (!urim) {
    return Error{
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "This cluster is no 'urim' sample entry."
    };
  }

  std::shared_ptr<const Box_uri> uri = urim->get_child_box<const Box_uri>();
  if (!uri) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "The 'urim' box has no 'uri' child box."
    };
  }

  return uri->get_uri();
}


bool Track::end_of_sequence_reached() const
{
  //return (m_next_sample_to_be_processed > m_chunks.back()->last_sample_number());
  return m_next_sample_to_be_output >= m_num_output_samples;
}


Error Track::finalize_track()
{
  // --- write active chunk data

  size_t data_start = m_heif_context->get_heif_file()->append_mdat_data(m_chunk_data);
  m_chunk_data.clear();

  // first sample in chunk? -> write chunk offset

  if (true) {
    // m_stsc->last_chunk_empty()) {
    // TODO: we will have to call this at the end of a chunk to dump the current SAI queue

    // if auxiliary data is interleaved, write it between the chunks
    if (m_aux_helper_tai_timestamps) m_aux_helper_tai_timestamps->write_interleaved(get_file());
    if (m_aux_helper_content_ids) m_aux_helper_content_ids->write_interleaved(get_file());

    // TODO
    assert(data_start < 0xFF000000); // add some headroom for header data
    m_stco->add_chunk_offset(static_cast<uint32_t>(data_start));
  }


  // --- write rest of data

  if (m_aux_helper_tai_timestamps) m_aux_helper_tai_timestamps->write_all(m_stbl, get_file());
  if (m_aux_helper_content_ids) m_aux_helper_content_ids->write_all(m_stbl, get_file());

  uint64_t duration = m_stts->get_total_duration(true);
  m_mdhd->set_duration(duration);

  m_stss->set_total_number_of_samples(m_stsz->num_samples());

  // only add ctts box if we use frame-reordering
  if (!m_ctts->is_constant_offset()) {
    m_stbl->append_child_box(m_ctts);
  }

  return {};
}


uint64_t Track::get_duration_in_media_units() const
{
  return m_mdhd->get_duration();
}


uint32_t Track::get_timescale() const
{
  return m_mdhd->get_timescale();
}


void Track::set_track_duration_in_movie_units(uint64_t total_duration, uint64_t segment_duration)
{
  m_tkhd->set_duration(total_duration);

  if (m_elst) {
    Box_elst::Entry entry;
    entry.segment_duration = segment_duration;

    m_elst->add_entry(entry);
  }
}


void Track::enable_edit_list_repeat_mode(bool enable)
{
  if (!m_elst) {
    if (!enable) {
      return;
    }

    auto edts = std::make_shared<Box_edts>();
    m_trak->append_child_box(edts);

    m_elst = std::make_shared<Box_elst>();
    edts->append_child_box(m_elst);

    m_elst->enable_repeat_mode(enable);
  }
}


void Track::add_chunk(heif_compression_format format)
{
  auto chunk = std::make_shared<Chunk>(m_heif_context, m_id, format);
  m_chunks.push_back(chunk);

  int chunkIdx = (uint32_t) m_chunks.size();
  m_stsc->add_chunk(chunkIdx);
}

void Track::set_sample_description_box(std::shared_ptr<Box> sample_description_box)
{
  // --- add 'taic' when we store timestamps as sample auxiliary information

  if (m_track_info.with_sample_tai_timestamps != heif_sample_aux_info_presence_none) {
    auto taic = std::make_shared<Box_taic>();
    taic->set_from_tai_clock_info(m_track_info.tai_clock_info);
    sample_description_box->append_child_box(taic);
  }

  m_stsd->add_sample_entry(sample_description_box);
}


Error Track::write_sample_data(const std::vector<uint8_t>& raw_data, uint32_t sample_duration,
                               int32_t composition_time_offset,
                               bool is_sync_sample,
                               const heif_tai_timestamp_packet* tai, const std::optional<std::string>& gimi_contentID)
{
  m_chunk_data.insert(m_chunk_data.end(), raw_data.begin(), raw_data.end());

  m_stsc->increase_samples_in_chunk(1);

  m_stsz->append_sample_size((uint32_t) raw_data.size());

  if (is_sync_sample) {
    m_stss->add_sync_sample(m_next_sample_to_be_output + 1);
  }

  if (sample_duration == 0) {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "Sample duration may not be 0"
    };
  }

  m_stts->append_sample_duration(sample_duration);
  m_ctts->append_sample_offset(composition_time_offset);


  // --- sample timestamp

  if (m_track_info.with_sample_tai_timestamps != heif_sample_aux_info_presence_none) {
    if (tai) {
      std::vector<uint8_t> tai_data = Box_itai::encode_tai_to_bitstream(tai);
      auto err = m_aux_helper_tai_timestamps->add_sample_info(tai_data);
      if (err) {
        return err;
      }
    }
    else if (m_track_info.with_sample_tai_timestamps == heif_sample_aux_info_presence_optional) {
      m_aux_helper_tai_timestamps->add_nonpresent_sample();
    }
    else {
      return {
        heif_error_Encoding_error,
        heif_suberror_Unspecified,
        "Mandatory TAI timestamp missing"
      };
    }
  }

  // --- sample content id

  if (m_track_info.with_sample_content_ids != heif_sample_aux_info_presence_none) {
    if (gimi_contentID) {
      auto id = *gimi_contentID;
      const char* id_str = id.c_str();
      std::vector<uint8_t> id_vector;
      id_vector.insert(id_vector.begin(), id_str, id_str + id.length() + 1);
      auto err = m_aux_helper_content_ids->add_sample_info(id_vector);
      if (err) {
        return err;
      }
    }
    else if (m_track_info.with_sample_content_ids == heif_sample_aux_info_presence_optional) {
      m_aux_helper_content_ids->add_nonpresent_sample();
    }
    else {
      return {
        heif_error_Encoding_error,
        heif_suberror_Unspecified,
        "Mandatory ContentID missing"
      };
    }
  }

  m_next_sample_to_be_output++;

  return Error::Ok;
}


void Track::add_reference_to_track(uint32_t referenceType, uint32_t to_track_id)
{
  if (!m_tref) {
    m_tref = std::make_shared<Box_tref>();
    m_trak->append_child_box(m_tref);
  }

  m_tref->add_references(to_track_id, referenceType);
}


Error Track::init_sample_timing_table()
{
  m_num_samples = m_stsz->num_samples();

  // --- build media timeline

  std::vector<SampleTiming> media_timeline;

  uint64_t current_decoding_time = 0;
  uint32_t current_chunk = 0;
  uint32_t current_sample_in_chunk_idx = 0;

  for (uint32_t i = 0; i < m_num_samples; i++) {
    SampleTiming timing;
    timing.sampleIdx = i;
    timing.sampleInChunkIdx = current_sample_in_chunk_idx;
    timing.media_decoding_time = current_decoding_time;
    timing.sample_duration_media_time = m_stts->get_sample_duration(i);
    current_decoding_time += timing.sample_duration_media_time;
    current_sample_in_chunk_idx++;

    while (current_chunk < m_chunks.size() &&
           i > m_chunks[current_chunk]->last_sample_number()) {
      current_chunk++;
      current_sample_in_chunk_idx=0;

      if (current_chunk > m_chunks.size()) {
        timing.chunkIdx = 0; // TODO: error
      }
    }

    timing.chunkIdx = current_chunk;

    media_timeline.push_back(timing);
  }

  // --- build presentation timeline from editlist

  bool fallback = false;

  if (m_heif_context->get_sequence_timescale() != get_timescale()) {
    fallback = true;
  }
  else if (m_elst &&
           m_elst->num_entries() == 1 &&
           m_elst->get_entry(0).media_time == 0 &&
           m_elst->get_entry(0).segment_duration == m_mdhd->get_duration() &&
           m_elst->is_repeat_mode()) {
    m_presentation_timeline = media_timeline;

    uint64_t duration_media_units = get_duration_in_media_units();
    if (duration_media_units == 0) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Track duration is zero."
      };
    }

    m_num_output_samples = m_heif_context->get_sequence_duration() / get_duration_in_media_units() * media_timeline.size();
  }
  else {
    fallback = true;
  }

  // Fallback: just play the media timeline
  if (fallback) {
    m_presentation_timeline = media_timeline;
    m_num_output_samples = media_timeline.size();
  }

  return {};
}


Result<heif_raw_sequence_sample*> Track::get_next_sample_raw_data(const heif_decoding_options* options)
{
  uint64_t num_output_samples = m_num_output_samples;
  if (options && options->ignore_sequence_editlist) {
    num_output_samples = m_num_samples;
  }

  if (m_next_sample_to_be_output >= num_output_samples) {
    return Error{
      heif_error_End_of_sequence,
      heif_suberror_Unspecified,
      "End of sequence"
    };
  }

  const auto& sampleTiming = m_presentation_timeline[m_next_sample_to_be_output % m_presentation_timeline.size()];
  uint32_t sample_idx = sampleTiming.sampleIdx;
  uint32_t chunk_idx = sampleTiming.chunkIdx;

  const std::shared_ptr<Chunk>& chunk = m_chunks[chunk_idx];

  DataExtent extent = chunk->get_data_extent_for_sample(sample_idx);
  auto readResult = extent.read_data();
  if (!readResult) {
    return readResult.error();
  }

  heif_raw_sequence_sample* sample = new heif_raw_sequence_sample();
  sample->data = **readResult;

  // read sample duration

  if (m_stts) {
    sample->duration = m_stts->get_sample_duration(sample_idx);
  }

  // --- read sample auxiliary data

  if (m_aux_reader_content_ids) {
    auto readResult = m_aux_reader_content_ids->get_sample_info(get_file().get(), sample_idx);
    if (!readResult) {
      return readResult.error();
    }

    if (!readResult->empty()) {
      Result<std::string> convResult = vector_to_string(*readResult);
      if (!convResult) {
        return convResult.error();
      }

      sample->gimi_sample_content_id = *convResult;
    }
  }

  if (m_aux_reader_tai_timestamps) {
    auto readResult = m_aux_reader_tai_timestamps->get_sample_info(get_file().get(), sample_idx);
    if (!readResult) {
      return readResult.error();
    }

    if (!readResult->empty()) {
      auto resultTai = Box_itai::decode_tai_from_vector(*readResult);
      if (!resultTai) {
        return resultTai.error();
      }

      sample->timestamp = heif_tai_timestamp_packet_alloc();
      heif_tai_timestamp_packet_copy(sample->timestamp, &*resultTai);
    }
  }

  m_next_sample_to_be_output++;

  return sample;
}


std::vector<heif_sample_aux_info_type> Track::get_sample_aux_info_types() const
{
  std::vector<heif_sample_aux_info_type> types;

  if (m_aux_reader_tai_timestamps) types.emplace_back(m_aux_reader_tai_timestamps->get_type());
  if (m_aux_reader_content_ids) types.emplace_back(m_aux_reader_content_ids->get_type());

  return types;
}
