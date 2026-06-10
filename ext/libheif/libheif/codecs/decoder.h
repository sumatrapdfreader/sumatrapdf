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

#ifndef HEIF_DECODER_H
#define HEIF_DECODER_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"
#include "file.h"
#include "security_limits.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "image-items/hevc.h"


// Image dimensions in luma samples. Reused wherever libheif needs to pass
// a (width, height) pair around — initially for SPS-derived coded sizes,
// but designed to fit other uses (ispe, image-item dimensions) over time.
struct ImageSize
{
  uint32_t width;
  uint32_t height;
};


// Specifies the input data for decoding.
// For images, this points to the iloc extents.
// For sequences, this points to the track data.
struct DataExtent
{
  std::shared_ptr<HeifFile> m_file;
  enum class Source : uint8_t { Raw, Image, FileRange } m_source = Source::Raw;

  // --- raw data
  mutable std::vector<uint8_t> m_raw; // also for cached data

  // Holds m_raw's allocation against the file's max_total_memory budget.
  // Released when DataExtent is destroyed (or moved-from).
  mutable MemoryHandle m_raw_memory_handle;

  // --- image
  heif_item_id m_item_id = 0;

  // --- file range
  uint64_t m_offset = 0;
  uint32_t m_size = 0;

  void set_from_image_item(std::shared_ptr<HeifFile> file, heif_item_id item);

  void set_file_range(std::shared_ptr<HeifFile> file, uint64_t offset, uint32_t size);

  Result<std::vector<uint8_t>*> read_data() const;

  Result<std::vector<uint8_t>> read_data(uint64_t offset, uint64_t size) const;
};


class Decoder
{
public:
  static std::shared_ptr<Decoder> alloc_for_infe_type(const ImageItem* item);

  static std::shared_ptr<Decoder> alloc_for_sequence_sample_description_box(std::shared_ptr<const class Box_VisualSampleEntry> sample_description_box);


  virtual ~Decoder();

  virtual heif_compression_format get_compression_format() const = 0;

  void set_data_extent(DataExtent extent) { m_data_extent = std::move(extent); }

  const DataExtent& get_data_extent() const { return m_data_extent; }

  // --- information about the image format

  [[nodiscard]] virtual int get_luma_bits_per_pixel() const = 0;

  [[nodiscard]] virtual int get_chroma_bits_per_pixel() const = 0;

  [[nodiscard]] virtual Error get_coded_image_colorspace(heif_colorspace*, heif_chroma*) const = 0;

  // --- raw data access

  // Returns a stream of packets. Each packet is starts with a 4-byte size (MSB first).
  [[nodiscard]] virtual Result<std::vector<uint8_t>> read_bitstream_configuration_data() const = 0;

  // Returns the *coded* picture size from the codec configuration record (the
  // SPS for HEVC/AVC/VVC) — i.e. the buffer dimensions the decoder will
  // actually allocate, BEFORE conformance-window cropping. The cropped output
  // size is unsuitable for security checks: a malicious file can declare a
  // huge SPS picture size with a near-equal-sized conformance window, so the
  // displayed image looks small while the decoder still allocates the full
  // uncropped buffer.
  //
  // Returns nullopt when the codec does not store dimensions in its
  // configuration record (e.g. AV1's av1C) or when no SPS NAL is present.
  // Returns Error only on a structurally invalid configuration record.
  [[nodiscard]] virtual Result<std::optional<ImageSize>>
  get_coded_image_size_from_config() const
  {
    return std::optional<ImageSize>{};
  }

  Result<std::vector<uint8_t>> get_compressed_data(bool with_configuration_NALs) const;

  // --- decoding

  // Decode a stream image that contains exactly one image. Decoder input is flushed and
  // it always should return an image.
  virtual Result<std::shared_ptr<HeifPixelImage>>
  decode_single_frame_from_compressed_data(const heif_decoding_options& options,
                                           const heif_security_limits* limits);

  // Push data for one frame into decoder.
  virtual Error
  decode_sequence_frame_from_compressed_data(bool upload_configuration_NALs,
                                             const heif_decoding_options& options,
                                             uintptr_t user_data,
                                             const heif_security_limits* limits);

  virtual Error flush_decoder();

  // Get a decoded frame from the decoder.
  // It may return NULL when there is buffering in the codec.
  virtual Result<std::shared_ptr<HeifPixelImage> > get_decoded_frame(const heif_decoding_options& options,
                                                                     uintptr_t* out_user_data,
                                                                     const heif_security_limits* limits);

  // Release the codec plugin decoder context (frees worker threads).
  // Safe to call multiple times. The decoder will be re-created on next use.
  void release_decoder();

private:
  DataExtent m_data_extent;

  const heif_decoder_plugin* m_decoder_plugin = nullptr;
  void* m_decoder = nullptr;

  // get the decoder plugin if it is not set already
  Error require_decoder_plugin(const heif_decoding_options& options);
};

#endif
