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

#include "avif_enc.h"
#include "avif_boxes.h"
#include "error.h"
#include "context.h"
#include "api_structs.h"

#include <string>

enum heif_av1_obu_type : uint8_t
{
  heif_av1_obu_type_frame = 2
};


Result<Encoder::CodedImageData> Encoder_AVIF::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                     heif_encoder* encoder,
                                                     const heif_encoding_options& options,
                                                     heif_image_input_class input_class)
{
  CodedImageData codedImage;

  Box_av1C::configuration config;

  // Fill preliminary av1C in case we cannot parse the sequence_header() correctly in the code below.
  // TODO: maybe we can remove this later.
  fill_av1C_configuration(&config, image);

  heif_image c_api_image;
  c_api_image.image = image;

  heif_error err = encoder->plugin->encode_image(encoder->encoder, &c_api_image, input_class);
  if (err.code) {
    return Error(err.code,
                 err.subcode,
                 err.message);
  }

  for (;;) {
    uint8_t* data;
    int size;

    encoder->plugin->get_compressed_data(encoder->encoder, &data, &size, nullptr);

    bool found_config = fill_av1C_configuration_from_stream(&config, data, size);
    (void) found_config;

    if (data == nullptr) {
      break;
    }

    codedImage.append(data, size);
  }

  auto av1C = std::make_shared<Box_av1C>();
  av1C->set_configuration(config);
  codedImage.properties.push_back(av1C);

  codedImage.codingConstraints.intra_pred_used = true;
  codedImage.codingConstraints.all_ref_pics_intra = true; // TODO: change when we use predicted frames

  return codedImage;
}


Error Encoder_AVIF::encode_sequence_frame(const std::shared_ptr<HeifPixelImage>& image,
                                    heif_encoder* encoder,
                                    const heif_sequence_encoding_options& options,
                                    heif_image_input_class input_class,
                                    uint32_t framerate_num, uint32_t framerate_denom,
                                    uintptr_t frame_number)
{
  // Box_av1C::configuration config;

  // Fill preliminary av1C in case we cannot parse the sequence_header() correctly in the code below.
  // TODO: maybe we can remove this later.
  fill_av1C_configuration(&m_config, image);

  heif_image c_api_image;
  c_api_image.image = image;

  if (!m_encoder_active) {
    heif_error err = encoder->plugin->start_sequence_encoding(encoder->encoder,
                                                              &c_api_image,
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

    //m_hvcC = std::make_shared<Box_hvcC>();
    m_encoder_active = true;
  }


  heif_error err = encoder->plugin->encode_sequence_frame(encoder->encoder, &c_api_image, frame_number);
  if (err.code) {
    return Error(err.code,
                 err.subcode,
                 err.message);
  }

  m_all_refs_intra = (options.gop_structure == heif_sequence_gop_structure_intra_only);

  return get_data(encoder);
}


Error Encoder_AVIF::get_data(heif_encoder* encoder)
{
  CodedImageData codedImage;

  for (;;) {
    uint8_t* data;
    int size;

    uintptr_t out_frame_number;
    int is_keyframe = 0;
    encoder->plugin->get_compressed_data2(encoder->encoder, &data, &size, &out_frame_number, &is_keyframe, nullptr);

    if (data == nullptr) {
      break;
    }

    uint8_t obu_type = (data[0]>>3) & 0x0F;

    // printf("got OBU type=%d size=%d framenr=%d\n", obu_type, size, out_frame_number);

    bool found_config = fill_av1C_configuration_from_stream(&m_config, data, size);
    (void) found_config;

    codedImage.append(data, size);
    codedImage.frame_nr = out_frame_number;

    if (encoder->plugin->plugin_api_version >= 4 &&
        encoder->plugin->does_indicate_keyframes) {
      codedImage.is_sync_frame = is_keyframe;
    }

    // If we got an image, stop collecting data packets.
    if (obu_type == heif_av1_obu_type_frame) {
      break;
    }
  }

  if (m_end_of_sequence_reached && !m_av1C_sent) {
    auto av1C = std::make_shared<Box_av1C>();
    av1C->set_configuration(m_config);
    codedImage.properties.push_back(av1C);
    m_av1C_sent = true;
  }

  codedImage.codingConstraints.intra_pred_used = true;
  codedImage.codingConstraints.all_ref_pics_intra = m_all_refs_intra;

  m_current_output_data = std::move(codedImage);

  return {};
}


Error Encoder_AVIF::encode_sequence_flush(heif_encoder* encoder)
{
  encoder->plugin->end_sequence_encoding(encoder->encoder);
  m_encoder_active = false;
  m_end_of_sequence_reached = true;

  return get_data(encoder);
}


std::optional<Encoder::CodedImageData> Encoder_AVIF::encode_sequence_get_data()
{
  return std::move(m_current_output_data);
}


std::shared_ptr<Box_VisualSampleEntry> Encoder_AVIF::get_sample_description_box(const CodedImageData& data) const
{
  auto av01 = std::make_shared<Box_av01>();
  av01->get_VisualSampleEntry().compressorname = "AVIF";

  for (auto prop : data.properties) {
    if (prop->get_short_type() == fourcc("av1C")) {
      av01->append_child_box(prop);
      return av01;
    }
  }

  // box not available yet
  return nullptr;
}
