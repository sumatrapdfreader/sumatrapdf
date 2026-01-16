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

#ifndef HEIF_ENCODER_JPEG_H
#define HEIF_ENCODER_JPEG_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"
#include "file.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "codecs/encoder.h"


class Encoder_JPEG : public Encoder {
public:
  const heif_color_profile_nclx* get_forced_output_nclx() const override;

  Result<CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                heif_encoder* encoder,
                                const heif_encoding_options& options,
                                enum heif_image_input_class input_class) override;

  std::shared_ptr<Box_VisualSampleEntry> get_sample_description_box(const CodedImageData&) const override;


  bool encode_sequence_started() const override { return m_codedImageData.has_value(); }

  Error encode_sequence_frame(const std::shared_ptr<HeifPixelImage>& image,
                                      heif_encoder* encoder,
                                      const heif_sequence_encoding_options& options,
                                      heif_image_input_class input_class,
                                      uint32_t framerate_num, uint32_t framerate_denom,
                                      uintptr_t frame_number) override
  {
    heif_encoding_options dummy_options{};

    auto encodeResult = encode(image, encoder, dummy_options, input_class);
    if (encodeResult.error()) {
      return encodeResult.error();
    }

    m_codedImageData = std::move(*encodeResult);

    m_codedImageData->frame_nr = frame_number;
    m_codedImageData->is_sync_frame = true;

    return {};
  }

  Error encode_sequence_flush(heif_encoder* encoder) override
  {
    return {};
  }

  std::optional<CodedImageData> encode_sequence_get_data() override
  {
    return std::move(m_codedImageData);
  }

private:
  std::optional<CodedImageData> m_codedImageData;
};


#endif
