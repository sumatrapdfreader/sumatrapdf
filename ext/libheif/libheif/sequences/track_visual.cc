/*
 * HEIF image base codec.
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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

#include "track_visual.h"

#include <memory>
#include "codecs/decoder.h"
#include "codecs/encoder.h"
#include "chunk.h"
#include "pixelimage.h"
#include "context.h"
#include "api_structs.h"
#include "codecs/hevc_boxes.h"
#include "codecs/uncompressed/unc_boxes.h"


Track_Visual::Track_Visual(HeifContext* ctx)
  : Track(ctx)
{
}


Track_Visual::~Track_Visual()
{
  for (auto& user_data : m_frame_user_data) {
    user_data.second.release();
  }
}


Error Track_Visual::load(const std::shared_ptr<Box_trak>& trak)
{
  Error parentLoadError = Track::load(trak);
  if (parentLoadError) {
    return parentLoadError;
  }

  const std::vector<uint32_t>& chunk_offsets = m_stco->get_offsets();

  // Find sequence resolution

  if (!chunk_offsets.empty()) {
    auto* s2c = m_stsc->get_chunk(1);
    if (!s2c) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Visual track has no chunk 1"
      };
    }

    Box_stsc::SampleToChunk sampleToChunk = *s2c;

    auto sample_description = m_stsd->get_sample_entry(sampleToChunk.sample_description_index - 1);
    if (!sample_description) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Visual track has sample description"
      };
    }

    auto visual_sample_description = std::dynamic_pointer_cast<const Box_VisualSampleEntry>(sample_description);
    if (!visual_sample_description) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Visual track sample description does not match visual track."
      };
    }

    m_width = visual_sample_description->get_VisualSampleEntry_const().width;
    m_height = visual_sample_description->get_VisualSampleEntry_const().height;
  }

  return {};
}


Error Track_Visual::initialize_after_parsing(HeifContext* ctx, const std::vector<std::shared_ptr<Track> >& all_tracks)
{
  // --- check whether there is an auxiliary alpha track assigned to this track

  // Only assign to image-sequence tracks (TODO: are there also alpha tracks allowed for video tracks 'heif_track_type_video'?)

  if (get_handler() == heif_track_type_image_sequence) {
    for (auto track : all_tracks) {
      // skip ourselves
      if (track->get_id() != get_id()) {
        // Is this an aux alpha track?
        auto h = fourcc_to_string(track->get_handler());
        if (track->get_handler() == heif_track_type_auxiliary &&
            track->get_auxiliary_info_type() == heif_auxiliary_track_info_type_alpha) {
          // Is it assigned to the current track
          auto tref = track->get_tref_box();
          if (!tref) {
            return {
              heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "Auxiliary track without 'tref'"
            };
          }

          auto references = tref->get_references(fourcc("auxl"));
          if (std::any_of(references.begin(), references.end(), [this](uint32_t id) { return id == get_id(); })) {
            // Assign it

            m_aux_alpha_track = std::dynamic_pointer_cast<Track_Visual>(track);
          }
        }
      }
    }
  }

  return {};
}


Track_Visual::Track_Visual(HeifContext* ctx, uint32_t track_id, uint16_t width, uint16_t height,
                           const TrackOptions* options, uint32_t handler_type)
  : Track(ctx, track_id, options, handler_type)
{
  m_tkhd->set_resolution(width, height);
  //m_hdlr->set_handler_type(handler_type);  already done in Track()

  auto vmhd = std::make_shared<Box_vmhd>();
  m_minf->append_child_box(vmhd);
}


bool Track_Visual::has_alpha_channel() const
{
  if (m_aux_alpha_track != nullptr) {
    return true;
  }

  // --- special case: 'uncv' with alpha component
#if WITH_UNCOMPRESSED_CODEC
  if (m_stsd) {
    auto sampleEntry = m_stsd->get_sample_entry(0);
    if (sampleEntry) {
      if (auto box_uncv = std::dynamic_pointer_cast<const Box_uncv>(sampleEntry)) {
        if (auto cmpd = box_uncv->get_child_box<const Box_cmpd>()) {
          if (cmpd->has_component(component_type_alpha)) {
            return true;
          }
        }
      }
    }
  }
#endif

  return false;
}


Result<std::shared_ptr<HeifPixelImage> > Track_Visual::decode_next_image_sample(const heif_decoding_options& options)
{
  // --- If we ignore the editlist, we stop when we reached the end of the original samples.

  uint64_t num_output_samples = m_num_output_samples;
  if (options.ignore_sequence_editlist) {
    num_output_samples = m_num_samples;
  }

  // --- Did we reach the end of the sequence?

  if (m_next_sample_to_be_output >= num_output_samples) {
    return Error{
      heif_error_End_of_sequence,
      heif_suberror_Unspecified,
      "End of sequence"
    };
  }


  std::shared_ptr<HeifPixelImage> image;

  uint32_t sample_idx_in_chunk;
  uintptr_t decoded_sample_idx = 0; // TODO: map this to sample idx + chunk

  for (;;) {
    const SampleTiming& sampleTiming = m_presentation_timeline[m_next_sample_to_be_decoded % m_presentation_timeline.size()];
    sample_idx_in_chunk = sampleTiming.sampleIdx;
    uint32_t chunk_idx = sampleTiming.chunkIdx;

    const std::shared_ptr<Chunk>& chunk = m_chunks[chunk_idx];

    auto decoder = chunk->get_decoder();
    assert(decoder);

    // avoid calling get_decoded_frame() before starting the decoder.
    if (m_next_sample_to_be_decoded != 0) {
      Result<std::shared_ptr<HeifPixelImage> > getFrameResult = decoder->get_decoded_frame(options,
                                                                                           &decoded_sample_idx,
                                                                                           m_heif_context->get_security_limits());
      if (getFrameResult.error()) {
        return getFrameResult.error();
      }


      // We received a decoded frame. Exit "push data" / "decode" loop.

      if (*getFrameResult != nullptr) {
        image = *getFrameResult;

        // If this was the last frame in the EditList segment, reset the 'flushed' flag
        // in case we have to restart the decoder for another repetition of the segment.
        if ((m_next_sample_to_be_output + 1) % m_presentation_timeline.size() == 0) {
          m_decoder_is_flushed = false;
        }

        break;
      }

      // If the sequence has ended and the decoder was flushed, but we still did not receive
      // the image, we are waiting for, this is an error.
      if (m_decoder_is_flushed) {
        return Error(heif_error_Decoder_plugin_error,
                     heif_suberror_Unspecified,
                     "Did not decode all frames");
      }
    }


    // --- Push more data into the decoder (or send end-of-sequence).

    if (m_next_sample_to_be_decoded < m_num_output_samples) {

      // --- Find the data extent that stores the compressed frame data.

      DataExtent extent = chunk->get_data_extent_for_sample(sample_idx_in_chunk);
      decoder->set_data_extent(extent);

      // std::cout << "PUSH chunk " << chunk_idx << " sample " << sample_idx << " (" << extent.m_size << " bytes)\n";

      // advance decoding index to next in segment
      m_next_sample_to_be_decoded++;

      // Send the decoder configuration when we send the first sample of the chunk.
      // The configuration NALs might change for each chunk.
      const bool is_first_sample = (sample_idx_in_chunk == 0);

      // --- Push data into the decoder.
      Error decodingError = decoder->decode_sequence_frame_from_compressed_data(is_first_sample,
                                                                                options,
                                                                                sample_idx_in_chunk, // user data
                                                                                m_heif_context->get_security_limits());
      if (decodingError) {
        return decodingError;
      }
    }
    else {
      // --- End of sequence (Editlist segment) reached.

      // std::cout << "FLUSH\n";
      Error flushError = decoder->flush_decoder();
      if (flushError) {
        return flushError;
      }

      m_decoder_is_flushed = true;
    }
  }


  // --- We have received a new decoded image.
  //     Postprocess decoded image, attach metadata.

  if (m_stts) {
    image->set_sample_duration(m_stts->get_sample_duration(sample_idx_in_chunk));
  }

  // --- assign alpha if we have an assigned alpha track

  if (m_aux_alpha_track) {
    auto alphaResult = m_aux_alpha_track->decode_next_image_sample(options);
    if (!alphaResult) {
      return alphaResult.error();
    }

    auto alphaImage = *alphaResult;
    image->transfer_plane_from_image_as(alphaImage, heif_channel_Y, heif_channel_Alpha);
  }


  // --- read sample auxiliary data

  if (m_aux_reader_content_ids) {
    auto readResult = m_aux_reader_content_ids->get_sample_info(get_file().get(), (uint32_t)decoded_sample_idx);
    if (!readResult) {
      return readResult.error();
    }

    Result<std::string> convResult = vector_to_string(*readResult);
    if (!convResult) {
      return convResult.error();
    }

    image->set_gimi_sample_content_id(*convResult);
  }

  if (m_aux_reader_tai_timestamps) {
    auto readResult = m_aux_reader_tai_timestamps->get_sample_info(get_file().get(), (uint32_t)decoded_sample_idx);
    if (!readResult) {
      return readResult.error();
    }

    std::vector<uint8_t>& tai_data = *readResult;
    if (!tai_data.empty()) {
      auto resultTai = Box_itai::decode_tai_from_vector(tai_data);
      if (!resultTai) {
        return resultTai.error();
      }

      image->set_tai_timestamp(&*resultTai);
    }
  }

  m_next_sample_to_be_output++;

  return image;
}


Error Track_Visual::encode_end_of_sequence(heif_encoder* h_encoder)
{
  auto encoder = m_chunks.back()->get_encoder();

  for (;;) {
    Error err = encoder->encode_sequence_flush(h_encoder);
    if (err) {
      return err;
    }

    Result<bool> processingResult = process_encoded_data(h_encoder);
    if (processingResult.is_error()) {
      return processingResult.error();
    }

    if (!*processingResult) {
      break;
    }
  }


  // --- also end alpha track

  if (m_aux_alpha_track) {
    auto err = m_aux_alpha_track->encode_end_of_sequence(m_alpha_track_encoder.get());
    if (err) {
      return err;
    }
  }

  return {};
}


Error Track_Visual::encode_image(std::shared_ptr<HeifPixelImage> image,
                                 heif_encoder* h_encoder,
                                 const heif_sequence_encoding_options* in_options,
                                 heif_image_input_class input_class)
{
  if (image->get_width() > 0xFFFF ||
      image->get_height() > 0xFFFF) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Input image resolution too high"
    };
  }

  m_image_class = input_class;


  // --- If input has an alpha channel, add an alpha auxiliary track.

  if (in_options->save_alpha_channel && image->has_alpha() && !m_aux_alpha_track &&
      h_encoder->plugin->compression_format != heif_compression_uncompressed) { // TODO: ask plugin
    if (m_active_encoder) {
      return {
        heif_error_Usage_error,
        heif_suberror_Unspecified,
        "Input images must all either have an alpha channel or none of them."
      };
    }
    else {
      // alpha track uses default options, same timescale as color track

      TrackOptions alphaOptions;
      alphaOptions.track_timescale = m_track_info.track_timescale;

      auto newAlphaTrackResult = m_heif_context->add_visual_sequence_track(&alphaOptions,
                                                                           heif_track_type_auxiliary,
                                                                           static_cast<uint16_t>(image->get_width()),
                                                                           static_cast<uint16_t>(image->get_height()));

      if (auto err = newAlphaTrackResult.error()) {
        return err;
      }

      // add a reference to the color track

      m_aux_alpha_track = *newAlphaTrackResult;
      m_aux_alpha_track->add_reference_to_track(fourcc("auxl"), m_id);

      // make a copy of the encoder from the color track for encoding the alpha track

      m_alpha_track_encoder = std::make_unique<heif_encoder>(h_encoder->plugin);
      heif_error err = m_alpha_track_encoder->alloc();
      if (err.code) {
        return {err.code, err.subcode, err.message};
      }
    }
  }


  if (!m_active_encoder) {
    m_active_encoder = h_encoder;
  }
  else if (m_active_encoder != h_encoder) {
    return {
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "You may not switch the heif_encoder while encoding a sequence."
    };
  }

  if (h_encoder->plugin->plugin_api_version < 4) {
    return Error{
      heif_error_Plugin_loading_error, heif_suberror_No_matching_decoder_installed,
      "Encoder plugin needs to be at least version 4."
    };
  }

  // === generate compressed image bitstream

  // generate new chunk for first image or when compression formats don't match

  if (m_chunks.empty() || m_chunks.back()->get_compression_format() != h_encoder->plugin->compression_format) {
    add_chunk(h_encoder->plugin->compression_format);
  }

  // --- check whether we have to convert the image color space

  // The reason for doing the color conversion here is that the input might be an RGBA image and the color conversion
  // will extract the alpha plane anyway. We can reuse that plane below instead of having to do a new conversion.

  auto encoder = m_chunks.back()->get_encoder();

  const heif_color_profile_nclx* output_nclx;
  heif_color_profile_nclx nclx;

  if (const auto* image_nclx = encoder->get_forced_output_nclx()) {
    output_nclx = const_cast<heif_color_profile_nclx*>(image_nclx);
  }
  else if (in_options) {
    output_nclx = in_options->output_nclx_profile;
  }
  else {
    if (image->has_nclx_color_profile()) {
      nclx_profile input_nclx = image->get_color_profile_nclx();

      nclx.version = 1;
      nclx.color_primaries = (enum heif_color_primaries) input_nclx.get_colour_primaries();
      nclx.transfer_characteristics = (enum heif_transfer_characteristics) input_nclx.get_transfer_characteristics();
      nclx.matrix_coefficients = (enum heif_matrix_coefficients) input_nclx.get_matrix_coefficients();
      nclx.full_range_flag = input_nclx.get_full_range_flag();

      output_nclx = &nclx;
    }
    else {
      nclx_profile undefined_nclx;
      undefined_nclx.copy_to_heif_color_profile_nclx(&nclx);

      output_nclx = &nclx;
    }
  }

  Result<std::shared_ptr<HeifPixelImage> > srcImageResult = encoder->convert_colorspace_for_encoding(image,
                                                                                                     h_encoder,
                                                                                                     output_nclx,
                                                                                                     in_options ? &in_options->color_conversion_options : nullptr,
                                                                                                     m_heif_context->get_security_limits());
  if (!srcImageResult) {
    return srcImageResult.error();
  }

  std::shared_ptr<HeifPixelImage> colorConvertedImage = *srcImageResult;

  // integer range is checked at beginning of function.
  assert(colorConvertedImage->get_width() == image->get_width());
  assert(colorConvertedImage->get_height() == image->get_height());
  m_width = static_cast<uint16_t>(colorConvertedImage->get_width());
  m_height = static_cast<uint16_t>(colorConvertedImage->get_height());

  // --- encode image

  heif_sequence_encoding_options* local_dummy_options = nullptr;
  if (!in_options) {
    local_dummy_options = heif_sequence_encoding_options_alloc();
  }

  Error encodeError = encoder->encode_sequence_frame(colorConvertedImage, h_encoder,
                                                     in_options ? *in_options : *local_dummy_options,
                                                     input_class,
                                                     colorConvertedImage->get_sample_duration(), get_timescale(),
                                                     m_current_frame_nr);
  if (local_dummy_options) {
    heif_sequence_encoding_options_release(local_dummy_options);
  }

  if (encodeError) {
    return encodeError;
  }

  m_sample_duration = colorConvertedImage->get_sample_duration();
  // TODO heif_tai_timestamp_packet* tai = image->get_tai_timestamp();
  // TODO image->has_gimi_sample_content_id() ? image->get_gimi_sample_content_id() : std::string{});


  // store frame user data

  FrameUserData userData;
  userData.sample_duration = colorConvertedImage->get_sample_duration();
  if (image->has_gimi_sample_content_id()) {
    userData.gimi_content_id = image->get_gimi_sample_content_id();
  }

  if (const auto* tai = image->get_tai_timestamp()) {
    userData.tai_timestamp = heif_tai_timestamp_packet_alloc();
    heif_tai_timestamp_packet_copy(userData.tai_timestamp, tai);
  }

  m_frame_user_data[m_current_frame_nr] = userData;

  m_current_frame_nr++;

  // --- get compressed data from encoder

  Result<bool> processingResult = process_encoded_data(h_encoder);
  if (auto err = processingResult.error()) {
    return err;
  }


  // --- encode alpha channel into auxiliary track

  if (m_aux_alpha_track) {
    auto alphaImageResult = create_alpha_image_from_image_alpha_channel(colorConvertedImage,
                                                                        m_heif_context->get_security_limits());
    if (auto err = alphaImageResult.error()) {
      return err;
    }

    (*alphaImageResult)->set_sample_duration(colorConvertedImage->get_sample_duration());

    auto err = m_aux_alpha_track->encode_image(*alphaImageResult,
                                               m_alpha_track_encoder.get(),
                                               in_options,
                                               heif_image_input_class_alpha);
    if (err) {
      return err;
    }
  }

  return {};
}


Result<bool> Track_Visual::process_encoded_data(heif_encoder* h_encoder)
{
  auto encoder = m_chunks.back()->get_encoder();

  std::optional<Encoder::CodedImageData> encodingResult = encoder->encode_sequence_get_data();
  if (!encodingResult) {
    return {};
  }

  const Encoder::CodedImageData& data = *encodingResult;


  if (data.bitstream.empty() &&
      data.properties.empty()) {
    return {false};
  }

  // --- generate SampleDescriptionBox

  if (!m_generated_sample_description_box) {
    auto sample_description_box = encoder->get_sample_description_box(data);
    if (sample_description_box) {
      VisualSampleEntry& visualSampleEntry = sample_description_box->get_VisualSampleEntry();
      visualSampleEntry.width = m_width;
      visualSampleEntry.height = m_height;

      // add Coding-Constraints box (ccst) only if we are generating an image sequence

      // TODO: does the alpha track also need a ccst box?
      //       ComplianceWarden says so (and it makes sense), but HEIF says that 'ccst' shall be present
      //       if the handler is 'pict'. However, the alpha track is 'auxv'.
      if (true) { // m_hdlr->get_handler_type() == heif_track_type_image_sequence) {
        auto ccst = std::make_shared<Box_ccst>();
        ccst->set_coding_constraints(data.codingConstraints);
        sample_description_box->append_child_box(ccst);
      }

      if (m_image_class == heif_image_input_class_alpha) {
        auto auxi_box = std::make_shared<Box_auxi>();
        auxi_box->set_aux_track_type_urn(get_track_auxiliary_info_type(h_encoder->plugin->compression_format));
        sample_description_box->append_child_box(auxi_box);
      }

      set_sample_description_box(sample_description_box);
      m_generated_sample_description_box = true;
    }
  }

  if (!data.bitstream.empty()) {
    uintptr_t frame_number = data.frame_nr;

    auto& user_data = m_frame_user_data[frame_number];

    int32_t decoding_time = static_cast<int32_t>(m_stsz->num_samples()) * m_sample_duration;
    int32_t composition_time = static_cast<int32_t>(frame_number) * m_sample_duration;

    Error err = write_sample_data(data.bitstream,
                                  user_data.sample_duration,
                                  composition_time - decoding_time,
                                  data.is_sync_frame,
                                  user_data.tai_timestamp,
                                  user_data.gimi_content_id);

    user_data.release();
    m_frame_user_data.erase(frame_number);

    if (err) {
      return err;
    }
  }

  return {true};
}


Error Track_Visual::finalize_track()
{
  if (m_active_encoder) {
    Error err = encode_end_of_sequence(m_active_encoder);
    if (err) {
      return err;
    }
  }

  return Track::finalize_track();
}


heif_brand2 Track_Visual::get_compatible_brand() const
{
  if (m_stsd->get_num_sample_entries() == 0) {
    return 0; // TODO: error ? Or can we assume at this point that there is at least one sample entry?
  }

  auto sampleEntry = m_stsd->get_sample_entry(0);

  uint32_t sample_entry_type = sampleEntry->get_short_type();
  switch (sample_entry_type) {
    case fourcc("hvc1"): {
      auto hvcC = sampleEntry->get_child_box<Box_hvcC>();
      if (!hvcC) { return 0; }

      const auto& config = hvcC->get_configuration();
      if (config.is_profile_compatibile(HEVCDecoderConfigurationRecord::Profile_Main) ||
          config.is_profile_compatibile(HEVCDecoderConfigurationRecord::Profile_MainStillPicture)) {
        return heif_brand2_hevc;
      }
      else {
        return heif_brand2_hevx;
      }
    }

    case fourcc("avc1"):
      return heif_brand2_avcs;

    case fourcc("av01"):
      return heif_brand2_avis;

    case fourcc("j2ki"):
      return heif_brand2_j2is;

    case fourcc("mjpg"):
      return heif_brand2_jpgs;

    case fourcc("vvc1"):
      return heif_brand2_vvis;

    default:
      return 0;
  }
}
