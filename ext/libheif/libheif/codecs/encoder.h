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

#ifndef HEIF_ENCODER_H
#define HEIF_ENCODER_H

#include "libheif/heif.h"
#include "error.h"
#include "libheif/heif_plugin.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "sequences/seq_boxes.h"

class HeifPixelImage;

class Box;


class Encoder {
public:
  virtual ~Encoder() = default;

  struct CodedImageData {
    std::vector<std::shared_ptr<Box>> properties;
    std::vector<uint8_t> bitstream;
    CodingConstraints codingConstraints;

    // If 0, the encoded size is unknown.
    uint32_t encoded_image_width = 0;
    uint32_t encoded_image_height = 0;

    bool is_sync_frame = true; // TODO: set in encoder
    uintptr_t frame_nr = 0;

    void append(const uint8_t* data, size_t size);

    void append_with_4bytes_size(const uint8_t* data, size_t size);
  };

  // If the output format requires a specific nclx (like JPEG), return this. Otherwise, return NULL.
  virtual const heif_color_profile_nclx* get_forced_output_nclx() const { return nullptr; }

  Result<std::shared_ptr<HeifPixelImage>> convert_colorspace_for_encoding(const std::shared_ptr<HeifPixelImage>& image,
                                                                          heif_encoder* encoder,
                                                                          const heif_color_profile_nclx* user_requested_output_nclx,
                                                                          const heif_color_conversion_options* color_conversion_options,
                                                                          const heif_security_limits* security_limits);

  virtual Result<CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                        heif_encoder* encoder,
                                        const heif_encoding_options& options,
                                        heif_image_input_class input_class) { return {}; }

  // --- encode sequence

  virtual bool encode_sequence_started() const { return false; }

  virtual Error encode_sequence_frame(const std::shared_ptr<HeifPixelImage>& image,
                                      heif_encoder* encoder,
                                      const heif_sequence_encoding_options& options,
                                      heif_image_input_class input_class,
                                      uint32_t framerate_num, uint32_t framerate_denom,
                                      uintptr_t frame_number) { return {}; }

  virtual Error encode_sequence_flush(heif_encoder* encoder) { return {}; }

  virtual std::optional<CodedImageData> encode_sequence_get_data() { return std::nullopt; }

  virtual std::shared_ptr<Box_VisualSampleEntry> get_sample_description_box(const CodedImageData&) const { return {}; }
};


#endif
