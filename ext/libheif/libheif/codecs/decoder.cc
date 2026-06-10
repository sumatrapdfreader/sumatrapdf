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

#include "codecs/decoder.h"

#include <utility>
#include "error.h"
#include "context.h"
#include "plugin_registry.h"
#include "api_structs.h"
#include "security_limits.h"

#include "codecs/hevc_dec.h"
#include "codecs/avif_dec.h"
#include "codecs/avc_dec.h"
#include "codecs/vvc_dec.h"
#include "codecs/jpeg_dec.h"
#include "codecs/jpeg2000_dec.h"
#include "avc_boxes.h"
#include "avif_boxes.h"
#include "hevc_boxes.h"
#include "vvc_boxes.h"
#include "jpeg_boxes.h"
#include "jpeg2000_boxes.h"

#if WITH_UNCOMPRESSED_CODEC
#include "codecs/uncompressed/unc_dec.h"
#include "codecs/uncompressed/unc_boxes.h"
#endif

void DataExtent::set_from_image_item(std::shared_ptr<HeifFile> file, heif_item_id item)
{
  m_file = std::move(file);
  m_item_id = item;
  m_source = Source::Image;
}


void DataExtent::set_file_range(std::shared_ptr<HeifFile> file, uint64_t offset, uint32_t size)
{
  m_file = std::move(file);
  m_source = Source::FileRange;
  m_offset = offset;
  m_size = size;
}


Result<std::vector<uint8_t>*> DataExtent::read_data() const
{
  if (!m_raw.empty()) {
    return &m_raw;
  }
  else if (m_source == Source::Image) {
    assert(m_file);

    // image
    Error err = m_file->append_data_from_iloc(m_item_id, m_raw);
    if (err) {
      return err;
    }

    // Account the (now-known) buffer size against the file's total-memory budget.
    // append_data_from_iloc has already enforced max_memory_block_size per extent.
    if (auto memErr = m_raw_memory_handle.alloc(m_raw.size(), m_file->get_security_limits(),
                                                "decoder input buffer (iloc)")) {
      m_raw.clear();
      m_raw.shrink_to_fit();
      return memErr;
    }
  }
  else {
    assert(m_file);

    // Reserve the buffer in the total-memory tracker before allocating it.
    // This also enforces max_memory_block_size and rejects sizes that would
    // exceed max_total_memory across all concurrently-live DataExtents.
    if (auto memErr = m_raw_memory_handle.alloc(m_size, m_file->get_security_limits(),
                                                "decoder input buffer (sample)")) {
      return memErr;
    }

    // file range
    Error err = m_file->append_data_from_file_range(m_raw, m_offset, m_size);
    if (err) {
      m_raw_memory_handle.free();
      return err;
    }
  }

  return &m_raw;
}


Result<std::vector<uint8_t>> DataExtent::read_data(uint64_t offset, uint64_t size) const
{
  std::vector<uint8_t> data;

  if (!m_raw.empty()) {
    // No caller currently reaches this cached path with an out-of-range request, so
    // hitting it indicates an internal logic error rather than malformed input. Guard
    // it defensively anyway. The subtraction form avoids a uint64_t wrap in
    // 'offset + size' that would otherwise allow an out-of-bounds read below.
    // TODO: this would be better reported as an internal error; change it once we have
    //       a dedicated error code for that.
    if (offset > m_raw.size() || size > m_raw.size() - offset) {
      return Error{heif_error_Invalid_input,
                   heif_suberror_End_of_data,
                   "Requested data range exceeds the cached extent buffer"};
    }
    data.insert(data.begin(), m_raw.begin() + offset, m_raw.begin() + offset + size);
    return data;
  }
  else if (m_source == Source::Image) {
    // TODO: cache data

    // image
    Error err = m_file->append_data_from_iloc(m_item_id, data, offset, size);
    if (err) {
      return err;
    }
    return data;
  }
  else {
    // file range
    Error err = m_file->append_data_from_file_range(data, m_offset, m_size);
    if (err) {
      return err;
    }

    return data;
  }
}


std::shared_ptr<Decoder> Decoder::alloc_for_infe_type(const ImageItem* item)
{
  uint32_t format_4cc = item->get_infe_type();

  switch (format_4cc) {
    case fourcc("hvc1"): {
      auto hvcC = item->get_property<Box_hvcC>();
      if (!hvcC) return nullptr;
      return std::make_shared<Decoder_HEVC>(hvcC);
    }
    case fourcc("av01"): {
      auto av1C = item->get_property<Box_av1C>();
      if (!av1C) return nullptr;
      return std::make_shared<Decoder_AVIF>(av1C);
    }
    case fourcc("avc1"): {
      auto avcC = item->get_property<Box_avcC>();
      if (!avcC) return nullptr;
      return std::make_shared<Decoder_AVC>(avcC);
    }
    case fourcc("j2k1"): {
      auto j2kH = item->get_property<Box_j2kH>();
      return std::make_shared<Decoder_JPEG2000>(j2kH);
    }
    case fourcc("vvc1"): {
      auto vvcC = item->get_property<Box_vvcC>();
      if (!vvcC) return nullptr;
      return std::make_shared<Decoder_VVC>(vvcC);
    }
    case fourcc("jpeg"): {
      auto jpgC = item->get_property<Box_jpgC>();
      return std::make_shared<Decoder_JPEG>(jpgC);
    }
#if WITH_UNCOMPRESSED_CODEC
    case fourcc("unci"): {
      auto uncC = item->get_property<Box_uncC>();
      auto cmpd = item->get_property<Box_cmpd>();
      auto ispe = item->get_property<Box_ispe>();
      auto decoder = std::make_shared<Decoder_uncompressed>(uncC, cmpd, ispe);
      decoder->set_cpat(item->get_property<Box_cpat>());
      decoder->set_cmpC(item->get_property<Box_cmpC>());
      decoder->set_icef(item->get_property<Box_icef>());
      decoder->set_cloc(item->get_property<Box_cloc>());
      decoder->set_splz(item->get_all_properties<Box_splz>());
      decoder->set_sbpm(item->get_all_properties<Box_sbpm>());
      decoder->set_snuc(item->get_all_properties<Box_snuc>());
      return decoder;
    }
#endif
    case fourcc("mski"): {
      return nullptr; // do we need a decoder for this?
    }
    default:
      return nullptr;
  }
}


std::shared_ptr<Decoder> Decoder::alloc_for_sequence_sample_description_box(std::shared_ptr<const Box_VisualSampleEntry> sample_description_box)
{
  std::string compressor = sample_description_box->get_VisualSampleEntry_const().compressorname;
  uint32_t sampleType = sample_description_box->get_short_type();

  switch (sampleType) {
    case fourcc("hvc1"): {
      auto hvcC = sample_description_box->get_child_box<Box_hvcC>();
      if (!hvcC) return nullptr;
      return std::make_shared<Decoder_HEVC>(hvcC);
    }

    case fourcc("av01"): {
      auto av1C = sample_description_box->get_child_box<Box_av1C>();
      if (!av1C) return nullptr;
      return std::make_shared<Decoder_AVIF>(av1C);
    }

    case fourcc("vvc1"): {
      auto vvcC = sample_description_box->get_child_box<Box_vvcC>();
      if (!vvcC) return nullptr;
      return std::make_shared<Decoder_VVC>(vvcC);
    }

    case fourcc("avc1"): {
      auto avcC = sample_description_box->get_child_box<Box_avcC>();
      if (!avcC) return nullptr;
      return std::make_shared<Decoder_AVC>(avcC);
    }

#if WITH_UNCOMPRESSED_CODEC
    case fourcc("uncv"): {
      auto uncC = sample_description_box->get_child_box<Box_uncC>();
      auto cmpd = sample_description_box->get_child_box<Box_cmpd>();
      auto ispe = std::make_shared<Box_ispe>();
      ispe->set_size(sample_description_box->get_VisualSampleEntry_const().width,
                     sample_description_box->get_VisualSampleEntry_const().height);
      auto decoder = std::make_shared<Decoder_uncompressed>(uncC, cmpd, ispe);
      decoder->set_cpat(sample_description_box->get_child_box<Box_cpat>());
      decoder->set_cmpC(sample_description_box->get_child_box<Box_cmpC>());
      decoder->set_icef(sample_description_box->get_child_box<Box_icef>());
      decoder->set_cloc(sample_description_box->get_child_box<Box_cloc>());
      decoder->set_splz(sample_description_box->get_child_boxes<Box_splz>());
      decoder->set_sbpm(sample_description_box->get_child_boxes<Box_sbpm>());
      decoder->set_snuc(sample_description_box->get_child_boxes<Box_snuc>());
      return decoder;
    }
#endif

    case fourcc("j2ki"): {
      auto j2kH = sample_description_box->get_child_box<Box_j2kH>();
      return std::make_shared<Decoder_JPEG2000>(j2kH);
    }

    case fourcc("mjpg"): {
      auto jpgC = sample_description_box->get_child_box<Box_jpgC>();
      return std::make_shared<Decoder_JPEG>(jpgC);
    }

    default:
      return nullptr;
  }
}


Result<std::vector<uint8_t>> Decoder::get_compressed_data(bool with_configuration_NALs) const
{
  // --- get the compressed image data

  if (with_configuration_NALs) {
    // data from configuration blocks

    Result<std::vector<uint8_t>> confData = read_bitstream_configuration_data();
    if (!confData) {
      return confData.error();
    }

    std::vector<uint8_t> data = *confData;

    // append image data

    auto dataResult = m_data_extent.read_data();
    if (!dataResult) {
      return dataResult.error();
    }

    data.insert(data.end(), (*dataResult)->begin(), (*dataResult)->end());

    return data;
  }
  else {
    auto dataResult = m_data_extent.read_data();
    if (!dataResult) {
      return dataResult.error();
    }

    return {*(*dataResult)};
  }
}


Decoder::~Decoder()
{
  release_decoder();
}


void Decoder::release_decoder()
{
  if (m_decoder) {
    assert(m_decoder_plugin);
    m_decoder_plugin->free_decoder(m_decoder);
    m_decoder = nullptr;
  }
}


Error Decoder::require_decoder_plugin(const heif_decoding_options& options)
{
  if (!m_decoder_plugin) {
    if (options.decoder_id && !has_decoder(get_compression_format(), options.decoder_id)) {
      return {
        heif_error_Plugin_loading_error,
        heif_suberror_Unspecified,
        "No decoder with that ID found."
      };
    }

    m_decoder_plugin = get_decoder(get_compression_format(), options.decoder_id);
    if (!m_decoder_plugin) {
      return Error(heif_error_Plugin_loading_error, heif_suberror_No_matching_decoder_installed);
    }

    if (m_decoder_plugin->plugin_api_version < 5) {
      return Error{
        heif_error_Plugin_loading_error, heif_suberror_No_matching_decoder_installed,
        "Decoder plugin needs to be at least version 5."
      };
    }
  }

  return {};
}


Error Decoder::decode_sequence_frame_from_compressed_data(bool upload_configuration_NALs,
                                                          const heif_decoding_options& options,
                                                          uintptr_t user_data,
                                                          const heif_security_limits* limits)
{
  auto pluginErr = require_decoder_plugin(options);
  if (pluginErr) {
    return pluginErr;
  }

  // Reject memory-bomb inputs whose codec configuration record (SPS) declares
  // a coded picture size beyond libheif's security limits, before handing any
  // bytes to the decoder plugin. Codecs whose configuration record does not
  // carry dimensions (e.g. AV1's av1C) return nullopt and skip the check.
  //
  // TODO: check this also in the decoder plugin since SPS packets may be
  //       found within the actual image bitstream.
  auto codedSize = get_coded_image_size_from_config();
  if (codedSize.is_error()) {
    return codedSize.error();
  }

  if (codedSize->has_value()) {
    Error sizeErr = check_for_valid_image_size(limits, (*codedSize)->width, (*codedSize)->height);
    if (sizeErr) {
      return sizeErr;
    }
  }

  // --- decode image with the plugin

  heif_error err;

  if (!m_decoder) {
    if (m_decoder_plugin->new_decoder == nullptr) {
      return Error(heif_error_Plugin_loading_error, heif_suberror_No_matching_decoder_installed,
                   "Cannot decode with a dummy decoder plugin.");
    }

    if (m_decoder_plugin->plugin_api_version >= 5) {
      heif_decoder_plugin_options plugin_options;
      plugin_options.format = get_compression_format();
      plugin_options.num_threads = options.num_codec_threads;
      plugin_options.strict_decoding = options.strict_decoding;
      plugin_options.limits = limits;

      err = m_decoder_plugin->new_decoder2(&m_decoder, &plugin_options);
      if (err.code != heif_error_Ok) {
        return Error(err.code, err.subcode, err.message);
      }
    }
    else {
      err = m_decoder_plugin->new_decoder(&m_decoder);
      if (err.code != heif_error_Ok) {
        return Error(err.code, err.subcode, err.message);
      }

      // automatically delete decoder plugin when we leave the scope
      //std::unique_ptr<void, void (*)(void*)> decoderSmartPtr(m_decoder, m_decoder_plugin->free_decoder);

      if (m_decoder_plugin->plugin_api_version >= 2) {
        if (m_decoder_plugin->set_strict_decoding) {
          m_decoder_plugin->set_strict_decoding(m_decoder, options.strict_decoding);
        }
      }
    }
  }

  auto dataResult = get_compressed_data(upload_configuration_NALs);
  if (!dataResult) {
    return dataResult.error();
  }

  // Check that we are pushing at least some data into the decoder.
  // Some decoders (e.g. aom) do not complain when the input data is empty and we might
  // get stuck in an endless decoding loop, waiting for the decompressed image.

  if (dataResult->size() == 0) {
    return Error{
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Input with empty data extent."
    };
  }

  //std::cout << "Decoder::decode_sequence_frame_from_compressed_data push " << dataResult->size() << "\n";
  if (m_decoder_plugin->plugin_api_version >= 5 && m_decoder_plugin->push_data2) {
    err = m_decoder_plugin->push_data2(m_decoder, dataResult->data(), dataResult->size(), user_data);
  }
  else {
    err = m_decoder_plugin->push_data(m_decoder, dataResult->data(), dataResult->size());
  }
  if (err.code != heif_error_Ok) {
    return Error(err.code, err.subcode, err.message);
  }

  return {};
}

Error Decoder::flush_decoder()
{
  assert(m_decoder_plugin);

  if (m_decoder_plugin->plugin_api_version >= 5) {
    heif_error err = m_decoder_plugin->flush_data(m_decoder);
    return Error::from_heif_error(err);
  }

  return {};
}

Result<std::shared_ptr<HeifPixelImage> > Decoder::get_decoded_frame(const heif_decoding_options& options,
                                                                    uintptr_t* out_user_data,
                                                                    const heif_security_limits* limits)
{
  auto pluginErr = require_decoder_plugin(options);
  if (pluginErr) {
    return pluginErr;
  }

  // The plugin's per-decoder context is created lazily on the first push of
  // compressed data. If a caller polls for a frame before any data was pushed
  // (e.g. when a sequence advances into a new chunk that uses a freshly-
  // allocated decoder), there is nothing buffered yet — return nullptr.
  if (!m_decoder) {
    return {nullptr};
  }

  heif_image* decoded_img = nullptr;

  heif_error err;

  if (m_decoder_plugin->plugin_api_version >= 5 &&
      m_decoder_plugin->decode_next_image2 != nullptr) {

    err = m_decoder_plugin->decode_next_image2(m_decoder, &decoded_img, out_user_data, limits);
    if (err.code != heif_error_Ok) {
      return Error::from_heif_error(err);
    }
  }
  else if (m_decoder_plugin->plugin_api_version >= 4 &&
           m_decoder_plugin->decode_next_image != nullptr) {

    err = m_decoder_plugin->decode_next_image(m_decoder, &decoded_img, limits);
    if (err.code != heif_error_Ok) {
      return Error::from_heif_error(err);
    }
  }
  else {
    err = m_decoder_plugin->decode_image(m_decoder, &decoded_img);
    if (err.code != heif_error_Ok) {
      return Error::from_heif_error(err);
    }
  }

  if (!decoded_img) {
    return {nullptr};
  }

  // -- cleanup

  std::shared_ptr<HeifPixelImage> img = std::move(decoded_img->image);
  heif_image_release(decoded_img);

  return img;
}


Result<std::shared_ptr<HeifPixelImage>>
Decoder::decode_single_frame_from_compressed_data(const heif_decoding_options& options,
                                                  const heif_security_limits* limits)
{
  Error decodeError = decode_sequence_frame_from_compressed_data(true, options, 0, limits);
  if (decodeError) {
    release_decoder();
    return decodeError;
  }

  flush_decoder();

  // We might have to try several times to get an image out of the decoder.
  // However, we stop after a maximum number of tries because the decoder might not
  // give any image when the input data is incomplete.
  const int max_decoding_tries = 50; // hardcoded value, should be large enough

  for (int i = 0; i < max_decoding_tries; i++) {
    Result<std::shared_ptr<HeifPixelImage>> imgResult;
    imgResult = get_decoded_frame(options, nullptr, limits);
    if (imgResult.error()) {
      release_decoder();
      return imgResult.error();
    }

    if (*imgResult != nullptr) {
      release_decoder();
      return imgResult;
    }
  }

  // We did not receive an image from the decoder. We give up.

  release_decoder();

  return Error{
    heif_error_Decoder_plugin_error,
    heif_suberror_Unspecified,
    "Decoding the input data did not give a decompressed image."
  };
}
