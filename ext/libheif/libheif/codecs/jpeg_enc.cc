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

#include "jpeg_enc.h"
#include "jpeg_boxes.h"
#include "error.h"
#include "context.h"
#include "api_structs.h"
#include <cstring>

#include <string>


static uint8_t JPEG_SOS = 0xDA;


const heif_color_profile_nclx* Encoder_JPEG::get_forced_output_nclx() const
{
  // JPEG always uses CCIR-601

  static heif_color_profile_nclx target_heif_nclx;
  target_heif_nclx.version = 1;
  target_heif_nclx.matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
  target_heif_nclx.color_primaries = heif_color_primaries_ITU_R_BT_601_6;
  target_heif_nclx.transfer_characteristics = heif_transfer_characteristic_ITU_R_BT_601_6;
  target_heif_nclx.full_range_flag = true;

  return &target_heif_nclx;
}


Result<Encoder::CodedImageData> Encoder_JPEG::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                     struct heif_encoder* encoder,
                                                     const struct heif_encoding_options& options,
                                                     enum heif_image_input_class input_class)
{
  Encoder::CodedImageData codedImage;


  heif_image c_api_image;
  c_api_image.image = image;

  struct heif_error err = encoder->plugin->encode_image(encoder->encoder, &c_api_image, input_class);
  if (err.code) {
    return Error(err.code,
                 err.subcode,
                 err.message);
  }

  std::vector<uint8_t> vec;

  for (;;) {
    uint8_t* data;
    int size;

    encoder->plugin->get_compressed_data(encoder->encoder, &data, &size, nullptr);

    if (data == nullptr) {
      break;
    }

    size_t oldsize = vec.size();
    vec.resize(oldsize + size);
    memcpy(vec.data() + oldsize, data, size);
  }

#if 0
  // Optional: split the JPEG data into a jpgC box and the actual image data.
  // Currently disabled because not supported yet in other decoders.
  if (false) {
    size_t pos = find_jpeg_marker_start(vec, JPEG_SOS);
    if (pos > 0) {
      std::vector<uint8_t> jpgC_data(vec.begin(), vec.begin() + pos);
      auto jpgC = std::make_shared<Box_jpgC>();
      jpgC->set_data(jpgC_data);

      auto ipma_box = m_heif_file->get_ipma_box();
      int index = m_heif_file->get_ipco_box()->find_or_append_child_box(jpgC);
      ipma_box->add_property_for_item_ID(image_id, Box_ipma::PropertyAssociation{true, uint16_t(index + 1)});

      std::vector<uint8_t> image_data(vec.begin() + pos, vec.end());
      vec = std::mo ve(image_data);
    }
  }
#endif
  (void) JPEG_SOS;

  codedImage.bitstream = std::move(vec);

#if 0
  // TODO: extract 'jpgC' header data
#endif

  codedImage.codingConstraints.intra_pred_used = false;
  codedImage.codingConstraints.all_ref_pics_intra = true;
  codedImage.codingConstraints.max_ref_per_pic = 0;

  return {codedImage};
}


std::shared_ptr<class Box_VisualSampleEntry> Encoder_JPEG::get_sample_description_box(const CodedImageData& data) const
{
  auto mjpg = std::make_shared<Box_mjpg>();
  mjpg->get_VisualSampleEntry().compressorname = "JPEG";

  for (auto prop : data.properties) {
    if (prop->get_short_type() == fourcc("jpgC")) {
      mjpg->append_child_box(prop);
    }
  }

  return mjpg;
}
