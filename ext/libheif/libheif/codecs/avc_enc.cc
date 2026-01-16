/*
 * HEIF codec.
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

#include "avc_enc.h"
#include "avc_boxes.h"
#include "error.h"
#include "context.h"
#include "api_structs.h"

#include <string>

#include "plugins/nalu_utils.h"


// TODO: can we use the new sequences interface for this to avoid duplicate code.
Result<Encoder::CodedImageData> Encoder_AVC::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                    heif_encoder* encoder,
                                                    const heif_encoding_options& options,
                                                    heif_image_input_class input_class)
{
  CodedImageData codedImage;

  auto avcC = std::make_shared<Box_avcC>();

  heif_image c_api_image;
  c_api_image.image = image;

  heif_error err = encoder->plugin->encode_image(encoder->encoder, &c_api_image, input_class);
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

    encoder->plugin->get_compressed_data(encoder->encoder, &data, &size, nullptr);

    if (data == nullptr) {
      break;
    }

    uint8_t nal_type = data[0] & 0x1f;

    if (nal_type == AVC_NAL_UNIT_SPS_NUT) {
      parse_sps_for_avcC_configuration(data, size, &avcC->get_configuration(), &encoded_width, &encoded_height);

      codedImage.encoded_image_width = encoded_width;
      codedImage.encoded_image_height = encoded_height;
    }

    switch (nal_type) {
      case AVC_NAL_UNIT_SPS_NUT:
        avcC->append_sps_nal(data, size);
        break;
      case AVC_NAL_UNIT_SPS_EXT_NUT:
        avcC->append_sps_ext_nal(data, size);
        break;
      case AVC_NAL_UNIT_PPS_NUT:
        avcC->append_pps_nal(data, size);
        break;

      default:
        codedImage.append_with_4bytes_size(data, size);
    }
  }

  if (!encoded_width || !encoded_height) {
    return Error(heif_error_Encoder_plugin_error,
                 heif_suberror_Invalid_image_size);
  }

  codedImage.properties.push_back(avcC);


  // Make sure that the encoder plugin works correctly and the encoded image has the correct size.

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

  codedImage.codingConstraints.intra_pred_used = true;
  codedImage.codingConstraints.all_ref_pics_intra = true; // TODO: change when we use predicted frames

  return codedImage;
}


Error Encoder_AVC::encode_sequence_frame(const std::shared_ptr<HeifPixelImage>& image,
                                         heif_encoder* encoder,
                                         const heif_sequence_encoding_options& options,
                                         heif_image_input_class input_class,
                                         uint32_t framerate_num, uint32_t framerate_denom,
                                         uintptr_t frame_number)
{
  heif_image c_api_image;
  c_api_image.image = image;

  if (!m_encoder_active) {
    heif_error err = encoder->plugin->start_sequence_encoding(encoder->encoder, &c_api_image,
                                                              input_class,
                                                              framerate_num, framerate_denom,
                                                              &options);
    if (err.code) {
      return {
        err.code,
        err.subcode,
        err.message
      };
    }

    m_avcC = std::make_shared<Box_avcC>();
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


Error Encoder_AVC::encode_sequence_flush(heif_encoder* encoder)
{
  encoder->plugin->end_sequence_encoding(encoder->encoder);
  m_encoder_active = false;
  m_end_of_sequence_reached = true;

  return get_data(encoder);
}


std::optional<Encoder::CodedImageData> Encoder_AVC::encode_sequence_get_data()
{
  if (m_output_image_complete) {
    m_output_image_complete = false;
    return std::move(m_current_output_data);
  }
  else {
    return std::nullopt;
  }
}

Error Encoder_AVC::get_data(heif_encoder* encoder)
{
  //CodedImageData codedImage;

  for (;;) {
    uint8_t* data;
    int size;

    uintptr_t frameNr=0;
    int more_frame_packets = 1;
    encoder->plugin->get_compressed_data2(encoder->encoder, &data, &size, &frameNr, nullptr, &more_frame_packets);

    if (data == nullptr) {
      break;
    }

    const uint8_t nal_type = (data[0] & 0x1f);
    const bool is_sync = (nal_type == 5);
    const bool is_image_data = (nal_type > 0 && nal_type <= AVC_NAL_UNIT_MAX_VCL);

    m_output_image_complete |= is_image_data;

    // std::cout << "received frameNr=" << frameNr << " nal_type:" << ((int)nal_type) << " size: " << size << "\n";

    if (nal_type == AVC_NAL_UNIT_SPS_NUT && m_avcC) {
      parse_sps_for_avcC_configuration(data, size,
                                       &m_avcC->get_configuration(),
                                       &m_encoded_image_width, &m_encoded_image_height);
    }

    if (is_image_data) {
      // more_frame_packets = 0;
    }

    switch (nal_type) {
      case AVC_NAL_UNIT_SPS_NUT:
        if (m_avcC && !m_avcC_has_SPS) m_avcC->append_sps_nal(data, size);
        m_avcC_has_SPS = true;
        break;

      case AVC_NAL_UNIT_SPS_EXT_NUT:
        if (m_avcC /*&& !m_avcC_has_SPS*/) m_avcC->append_sps_ext_nal(data, size);
        //m_avcC_has_SPS = true;
        break;

      case AVC_NAL_UNIT_PPS_NUT:
        if (m_avcC && !m_avcC_has_PPS) m_avcC->append_pps_nal(data, size);
        m_avcC_has_PPS = true;
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

  if (!m_output_image_complete) {
    return {};
  }

  if (!m_encoded_image_width || !m_encoded_image_height) {
    return Error(heif_error_Encoder_plugin_error,
                 heif_suberror_Invalid_image_size);
  }


  // --- return avcC when all headers are included and it was not returned yet
  //     TODO: it's maybe better to return this at the end so that we are sure to have all headers
  //           and also complete codingConstraints.

  if (m_end_of_sequence_reached && m_avcC && !m_avcC_sent) {
    m_current_output_data->properties.push_back(m_avcC);
    m_avcC = nullptr;
    m_avcC_sent = true;
  }

  m_current_output_data->encoded_image_width = m_encoded_image_width;
  m_current_output_data->encoded_image_height = m_encoded_image_height;

  m_current_output_data->codingConstraints.intra_pred_used = true;
  m_current_output_data->codingConstraints.all_ref_pics_intra = true; // TODO: change when we use predicted frames

  return {};
}


std::shared_ptr<Box_VisualSampleEntry> Encoder_AVC::get_sample_description_box(const CodedImageData& data) const
{
  auto avc1 = std::make_shared<Box_avc1>();
  avc1->get_VisualSampleEntry().compressorname = "AVC";

  for (auto prop : data.properties) {
    if (prop->get_short_type() == fourcc("avcC")) {
      avc1->append_child_box(prop);
      return avc1;
    }
  }

  // box not yet available
  return nullptr;
}
