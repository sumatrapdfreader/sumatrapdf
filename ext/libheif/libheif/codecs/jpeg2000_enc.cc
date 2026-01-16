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

#include "jpeg2000_enc.h"
#include "jpeg2000_boxes.h"
#include "error.h"
#include "context.h"
#include "api_structs.h"

#include <string>


Result<Encoder::CodedImageData> Encoder_JPEG2000::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                         struct heif_encoder* encoder,
                                                         const struct heif_encoding_options& options,
                                                         enum heif_image_input_class input_class)
{
  Encoder::CodedImageData codedImageData;

  heif_image c_api_image;
  c_api_image.image = image;

  encoder->plugin->encode_image(encoder->encoder, &c_api_image, input_class);

  // get compressed data
  for (;;) {
    uint8_t* data;
    int size;

    encoder->plugin->get_compressed_data(encoder->encoder, &data, &size, nullptr);

    if (data == nullptr) {
      break;
    }

    codedImageData.append(data, size);
  }

  // add 'j2kH' property
  auto j2kH = std::make_shared<Box_j2kH>();

  // add 'cdef' to 'j2kH'
  auto cdef = std::make_shared<Box_cdef>();
  cdef->set_channels(image->get_colorspace());
  j2kH->append_child_box(cdef);

  codedImageData.properties.push_back(j2kH);

  codedImageData.codingConstraints.intra_pred_used = false;
  codedImageData.codingConstraints.all_ref_pics_intra = true;
  codedImageData.codingConstraints.max_ref_per_pic = 0;

  return codedImageData;
}


std::shared_ptr<class Box_VisualSampleEntry> Encoder_JPEG2000::get_sample_description_box(const CodedImageData& data) const
{
  auto j2ki = std::make_shared<Box_j2ki>();
  j2ki->get_VisualSampleEntry().compressorname = "JPEG2000";

  for (auto prop : data.properties) {
    if (prop->get_short_type() == fourcc("j2kH")) {
      j2ki->append_child_box(prop);
      return j2ki;
    }
  }

  assert(false); // no hvcC generated
  return nullptr;
}


std::shared_ptr<class Box_VisualSampleEntry> Encoder_HTJ2K::get_sample_description_box(const CodedImageData& data) const
{
  auto j2ki = std::make_shared<Box_j2ki>();
  j2ki->get_VisualSampleEntry().compressorname = "HTJ2K";

  for (auto prop : data.properties) {
    if (prop->get_short_type() == fourcc("j2kH")) {
      j2ki->append_child_box(prop);
      return j2ki;
    }
  }

  assert(false); // no hvcC generated
  return nullptr;
}
