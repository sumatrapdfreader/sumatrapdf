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

#ifndef HEIF_ENCODER_VVC_H
#define HEIF_ENCODER_VVC_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"
#include "file.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "codecs/encoder.h"


class Encoder_VVC : public Encoder {
public:
  Result<CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                heif_encoder* encoder,
                                const heif_encoding_options& options,
                                heif_image_input_class input_class) override;

  std::shared_ptr<Box_VisualSampleEntry> get_sample_description_box(const CodedImageData&) const override;


  bool encode_sequence_started() const override { return m_encoder_active; }

  Error encode_sequence_frame(const std::shared_ptr<HeifPixelImage>& image,
                              heif_encoder* encoder,
                              const heif_sequence_encoding_options& options,
                              heif_image_input_class input_class,
                              uint32_t framerate_num, uint32_t framerate_denom,
                              uintptr_t frame_number) override;

  Error encode_sequence_flush(heif_encoder* encoder) override;

  std::optional<CodedImageData> encode_sequence_get_data() override;

private:
  bool m_encoder_active = false;
  bool m_end_of_sequence_reached = false;

  // Whether the vvcC is complete and was returned in an encode_sequence_get_data() call.
  bool m_vvcC_has_VPS = false;
  bool m_vvcC_has_SPS = false;
  bool m_vvcC_has_PPS = false;
  std::shared_ptr<class Box_vvcC> m_vvcC;
  bool m_vvcC_sent = false;

  int m_encoded_image_width = 0;
  int m_encoded_image_height = 0;

  std::optional<CodedImageData> m_current_output_data;

  Error get_data(heif_encoder*);
};


#endif
