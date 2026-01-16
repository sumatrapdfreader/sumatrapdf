/*
 * HEIF mask image codec.
 *
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
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

#include <cstdint>
#include <cstring>
#include <algorithm>
#include <map>
#include <cstring>

#include "libheif/heif.h"
#include "logging.h"
#include "mask_image.h"
#include "image_item.h"
#include "security_limits.h"


Error Box_mskC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  parse_full_box_header(range);
  m_bits_per_pixel = range.read8();
  return range.get_error();
}

std::string Box_mskC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);
  sstr << indent << "bits_per_pixel: " << ((int)m_bits_per_pixel) << "\n";
  return sstr.str();
}

Error Box_mskC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);
  writer.write8(m_bits_per_pixel);
  prepend_header(writer, box_start);
  return Error::Ok;
}


Error MaskImageCodec::decode_mask_image(const HeifContext* context,
                                        heif_item_id ID,
                                        std::shared_ptr<HeifPixelImage>& img,
                                        const std::vector<uint8_t>& data)
{
  auto image = context->get_image(ID, false);
  if (!image) {
    return {heif_error_Invalid_input,
            heif_suberror_Nonexisting_item_referenced};
  }

  std::shared_ptr<Box_ispe> ispe = image->get_property<Box_ispe>();
  std::shared_ptr<Box_mskC> mskC = image->get_property<Box_mskC>();

  uint32_t width = 0;
  uint32_t height = 0;

  if (ispe) {
    width = ispe->get_width();
    height = ispe->get_height();

    Error error = check_for_valid_image_size(context->get_security_limits(), width, height);
    if (error) {
      return error;
    }
  }

  if (!ispe || !mskC) {
    return Error(heif_error_Unsupported_feature,
                  heif_suberror_Unsupported_data_version,
                  "Missing required box for mask codec");
  }

  if ((mskC->get_bits_per_pixel() != 8) && (mskC->get_bits_per_pixel() != 16))
  {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Unsupported bit depth for mask item");
  }

  if (data.size() < width * height) {
    return {heif_error_Invalid_input,
            heif_suberror_Unspecified,
            "Mask image data is too short"};
  }

  img = std::make_shared<HeifPixelImage>();
  img->create(width, height, heif_colorspace_monochrome, heif_chroma_monochrome);
  auto err = img->add_plane(heif_channel_Y, width, height, mskC->get_bits_per_pixel(),
                            context->get_security_limits());
  if (err) {
    return err;
  }

  size_t stride;
  uint8_t* dst = img->get_plane(heif_channel_Y, &stride);
  if (((uint32_t)stride) == width) {
    memcpy(dst, data.data(), data.size());
  }
  else
  {
    for (uint32_t i = 0; i < height; i++)
    {
      memcpy(dst + i * stride, data.data() + i * width, width);
    }
  }
  return Error::Ok;
}


Result<std::shared_ptr<HeifPixelImage>> ImageItem_mask::decode_compressed_image(const heif_decoding_options& options,
                                                                                bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                                std::set<heif_item_id> processed_ids) const
{
  std::shared_ptr<HeifPixelImage> img;

  std::vector<uint8_t> data;

  // image data, usually from 'mdat'

  Error error = get_file()->append_data_from_iloc(get_id(), data);
  if (error) {
    return error;
  }

  Error err = MaskImageCodec::decode_mask_image(get_context(),
                                                get_id(),
                                                img,
                                                data);
  if (err) {
    return err;
  }
  else {
    return img;
  }
}


Result<Encoder::CodedImageData> ImageItem_mask::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                       heif_encoder* encoder,
                                                       const heif_encoding_options& options,
                                                       heif_image_input_class input_class)
{
  Encoder::CodedImageData codedImageData;

  if (image->get_colorspace() != heif_colorspace_monochrome)
  {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Unsupported colourspace for mask region");
  }

  if (image->get_bits_per_pixel(heif_channel_Y) != 8)
  {
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_data_version,
                 "Unsupported bit depth for mask region");
  }

  // TODO: we could add an option to lossless-compress this data
  std::vector<uint8_t> data;
  size_t src_stride;
  uint8_t* src_data = image->get_plane(heif_channel_Y, &src_stride);

  uint32_t w = image->get_width();
  uint32_t h = image->get_height();

  data.resize(w * h);

  if (w == (uint32_t)src_stride) {
    codedImageData.append(src_data, w*h);
  }
  else {
    for (uint32_t y = 0; y < h; y++) {
      codedImageData.append(src_data + y * src_stride, w);
    }
  }

  std::shared_ptr<Box_mskC> mskC = std::make_shared<Box_mskC>();
  mskC->set_bits_per_pixel(image->get_bits_per_pixel(heif_channel_Y));
  codedImageData.properties.push_back(mskC);

  return codedImageData;
}


int ImageItem_mask::get_luma_bits_per_pixel() const
{
  auto mskC = get_property<Box_mskC>();
  if (!mskC) {
    return -1;
  }

  return mskC->get_bits_per_pixel();
}
