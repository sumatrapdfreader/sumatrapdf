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

#include "unc_enc.h"
#include "unc_boxes.h"
#include "error.h"
#include "context.h"
#include "api_structs.h"
#include <cstring>
#include <image-items/unc_image.h>

#include <string>


Result<Encoder::CodedImageData> Encoder_uncompressed::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                     struct heif_encoder* encoder,
                                                     const struct heif_encoding_options& options,
                                                     enum heif_image_input_class input_class)
{
  Encoder::CodedImageData codedImage;

  Result<Encoder::CodedImageData> codingResult = ImageItem_uncompressed::encode_static(image, options);
  if (!codingResult) {
    return codingResult.error();
  }

  codedImage = *codingResult;

  // codedImage.bitstream = std::move(vec);

  codedImage.codingConstraints.intra_pred_used = false;
  codedImage.codingConstraints.all_ref_pics_intra = true;
  codedImage.codingConstraints.max_ref_per_pic = 0;

  return {codedImage};
}


std::shared_ptr<class Box_VisualSampleEntry> Encoder_uncompressed::get_sample_description_box(const CodedImageData& data) const
{
  auto uncv = std::make_shared<Box_uncv>();
  uncv->get_VisualSampleEntry().compressorname = "iso23001-17";

  for (auto prop : data.properties) {
    switch (prop->get_short_type()) {
      case fourcc("cmpd"):
      case fourcc("uncC"):
      case fourcc("cmpC"):
      case fourcc("icef"):
      case fourcc("cpat"):
        uncv->append_child_box(prop);
      break;
    }
  }

  return uncv;
}
