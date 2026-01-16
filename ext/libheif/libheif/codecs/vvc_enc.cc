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

#include "vvc_enc.h"
#include "vvc_boxes.h"
#include "error.h"
#include "context.h"
#include "api_structs.h"

#include <string>

#include "plugins/nalu_utils.h"


Result<Encoder::CodedImageData> Encoder_VVC::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                    struct heif_encoder* encoder,
                                                    const struct heif_encoding_options& options,
                                                    enum heif_image_input_class input_class)
{
  Encoder::CodedImageData codedImage;

  auto vvcC = std::make_shared<Box_vvcC>();
  codedImage.properties.push_back(vvcC);


  heif_image c_api_image;
  c_api_image.image = image;

  struct heif_error err = encoder->plugin->encode_image(encoder->encoder, &c_api_image, input_class);
  if (err.code) {
    return Error(err.code,
                 err.subcode,
                 err.message);
  }

  int encoded_width = 0;
  int encoded_height = 0;

  for (;;) {
    uint8_t* data;
    int size;

    encoder->plugin->get_compressed_data(encoder->encoder, &data, &size, NULL);

    if (data == NULL) {
      break;
    }


    const uint8_t NAL_SPS = 15;

    uint8_t nal_type = 0;
    if (size>=2) {
      nal_type = (data[1] >> 3) & 0x1F;
    }

    if (nal_type == NAL_SPS) {
      Box_vvcC::configuration config;

      parse_sps_for_vvcC_configuration(data, size, &config, &encoded_width, &encoded_height);

      vvcC->set_configuration(config);
    }

    switch (nal_type) {
      case 14: // VPS
      case 15: // SPS
      case 16: // PPS
        vvcC->append_nal_data(data, size);
        break;

      default:
        codedImage.append_with_4bytes_size(data, size);
    }
  }

  codedImage.codingConstraints.intra_pred_used = true;
  codedImage.codingConstraints.all_ref_pics_intra = true; // TODO: change when we use predicted frames
  codedImage.codingConstraints.max_ref_per_pic = 0;

  return codedImage;
}


std::shared_ptr<class Box_VisualSampleEntry> Encoder_VVC::get_sample_description_box(const CodedImageData& data) const
{
  auto vvc1 = std::make_shared<Box_vvc1>();
  vvc1->get_VisualSampleEntry().compressorname = "VVC";

  for (auto prop : data.properties) {
    if (prop->get_short_type() == fourcc("vvcC")) {
      vvc1->append_child_box(prop);
      return vvc1;
    }
  }

  // box not yet available
  return nullptr;
}


Error Encoder_VVC::encode_sequence_frame(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_sequence_encoding_options& options,
                                         heif_image_input_class input_class,
                                         uint32_t framerate_num, uint32_t framerate_denom,
                                         uintptr_t frame_number)
{
  heif_image c_api_image;
  c_api_image.image = image;

  if (!m_encoder_active) {
    heif_error err = encoder->plugin->start_sequence_encoding(encoder->encoder, &c_api_image, input_class,
                                                              framerate_num, framerate_denom,
                                                              &options);
    if (err.code) {
      return {
        err.code,
        err.subcode,
        err.message
      };
    }

    m_vvcC = std::make_shared<Box_vvcC>();
    m_encoder_active = true;
  }

  Error dataErr = get_data(encoder);
  if (dataErr) {
    return dataErr;
  }

  heif_error err = encoder->plugin->encode_sequence_frame(encoder->encoder, &c_api_image, frame_number);
  if (err.code) {
    return {
      err.code,
      err.subcode,
      err.message
    };
  }

  return get_data(encoder);
}


Error Encoder_VVC::encode_sequence_flush(heif_encoder* encoder)
{
  encoder->plugin->end_sequence_encoding(encoder->encoder);
  m_encoder_active = false;
  m_end_of_sequence_reached = true;

  return get_data(encoder);
}


std::optional<Encoder::CodedImageData> Encoder_VVC::encode_sequence_get_data()
{
  return std::move(m_current_output_data);
}

Error Encoder_VVC::get_data(heif_encoder* encoder)
{
  //CodedImageData codedImage;

  bool got_some_data = false;

  for (;;) {
    uint8_t* data;
    int size;

    uintptr_t frameNr=0;
    int more_frame_packets = 1;
    encoder->plugin->get_compressed_data2(encoder->encoder, &data, &size, &frameNr, nullptr, &more_frame_packets);

    if (data == nullptr) {
      break;
    }

    got_some_data = true;

    const uint8_t nal_type = (data[1] >> 3);
    const bool is_sync = (nal_type == 7 || nal_type == 8 || nal_type == 9);
    const bool is_image_data = (nal_type >= 0 && nal_type <= VVC_NAL_UNIT_MAX_VCL);

    // std::cout << "received frameNr=" << frameNr << " nal_type:" << ((int)nal_type) << " size: " << size << "\n";

    if (nal_type == VVC_NAL_UNIT_SPS_NUT && m_vvcC) {
      parse_sps_for_vvcC_configuration(data, size,
                                       &m_vvcC->get_configuration(),
                                       &m_encoded_image_width, &m_encoded_image_height);
    }

    switch (nal_type) {
      case VVC_NAL_UNIT_VPS_NUT:
        if (m_vvcC && !m_vvcC_has_VPS) m_vvcC->append_nal_data(data, size);
        m_vvcC_has_VPS = true;
        break;

      case VVC_NAL_UNIT_SPS_NUT:
        if (m_vvcC && !m_vvcC_has_SPS) m_vvcC->append_nal_data(data, size);
        m_vvcC_has_SPS = true;
        break;

      case VVC_NAL_UNIT_PPS_NUT:
        if (m_vvcC && !m_vvcC_has_PPS) m_vvcC->append_nal_data(data, size);
        m_vvcC_has_PPS = true;
        break;

      default:
        if (!m_current_output_data) {
          m_current_output_data = CodedImageData{};
        }
        m_current_output_data->append_with_4bytes_size(data, size);

        if (is_image_data) {
          m_current_output_data->is_sync_frame = is_sync;
          m_current_output_data->frame_nr = frameNr;
        }
    }

    if (!more_frame_packets) {
      break;
    }
  }

  if (!got_some_data) {
    return {};
  }

  if (!m_encoded_image_width || !m_encoded_image_height) {
    return Error(heif_error_Encoder_plugin_error,
                 heif_suberror_Invalid_image_size);
  }


  // --- return hvcC when all headers are included and it was not returned yet
  //     TODO: it's maybe better to return this at the end so that we are sure to have all headers
  //           and also complete codingConstraints.

  //if (hvcC_has_VPS && m_hvcC_has_SPS && m_hvcC_has_PPS && !m_hvcC_returned) {
  if (m_end_of_sequence_reached && m_vvcC && !m_vvcC_sent) {
    m_current_output_data->properties.push_back(m_vvcC);
    m_vvcC = nullptr;
    m_vvcC_sent = true;
  }

  m_current_output_data->encoded_image_width = m_encoded_image_width;
  m_current_output_data->encoded_image_height = m_encoded_image_height;


  // Make sure that the encoder plugin works correctly and the encoded image has the correct size.
#if 0
  if (encoder->plugin->plugin_api_version >= 3 &&
      encoder->plugin->query_encoded_size != nullptr) {
    uint32_t check_encoded_width = image->get_width(), check_encoded_height = image->get_height();

    encoder->plugin->query_encoded_size(encoder->encoder,
                                        image->get_width(), image->get_height(),
                                        &check_encoded_width,
                                        &check_encoded_height);

    assert((int)check_encoded_width == encoded_width);
    assert((int)check_encoded_height == encoded_height);
      }
#endif

  m_current_output_data->codingConstraints.intra_pred_used = true;
  m_current_output_data->codingConstraints.all_ref_pics_intra = true; // TODO: change when we use predicted frames

  return {};
}
