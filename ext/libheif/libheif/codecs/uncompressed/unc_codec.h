/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
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


#ifndef LIBHEIF_UNC_CODEC_H
#define LIBHEIF_UNC_CODEC_H

#include "pixelimage.h"
#include "file.h"
#include "context.h"
#include "libheif/heif_uncompressed.h"

#if WITH_UNCOMPRESSED_CODEC
#include "unc_boxes.h"
#endif

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>

class HeifContext;


bool isKnownUncompressedFrameConfigurationBoxProfile(const std::shared_ptr<const Box_uncC>& uncC);

Error fill_cmpd_and_uncC(std::shared_ptr<Box_cmpd>& cmpd,
                         std::shared_ptr<Box_uncC>& uncC,
                         const std::shared_ptr<const HeifPixelImage>& image,
                         const heif_unci_image_parameters* parameters,
                         bool save_alpha_channel);

bool map_uncompressed_component_to_channel(const std::shared_ptr<const Box_cmpd> &cmpd,
                                           const std::shared_ptr<const Box_uncC> &uncC,
                                           Box_uncC::Component component,
                                           heif_channel *channel);


class UncompressedImageCodec
{
public:
  static Error decode_uncompressed_image(const HeifContext* context,
                                         heif_item_id ID,
                                         std::shared_ptr<HeifPixelImage>& img);

  static Error decode_uncompressed_image_tile(const HeifContext* context,
                                              heif_item_id ID,
                                              std::shared_ptr<HeifPixelImage>& img,
                                              uint32_t tile_x0, uint32_t tile_y0);

  struct unci_properties {
    std::shared_ptr<const Box_ispe> ispe;
    std::shared_ptr<const Box_cmpd> cmpd;
    std::shared_ptr<const Box_uncC> uncC;
    std::shared_ptr<const Box_cmpC> cmpC;
    std::shared_ptr<const Box_icef> icef;
    // ...

    void fill_from_image_item(const std::shared_ptr<const ImageItem>&);
  };

  static Result<std::shared_ptr<HeifPixelImage>> decode_uncompressed_image(const unci_properties& properties,
                                                                           const struct DataExtent& extent,
                                                                           const heif_security_limits*);


  static Error get_heif_chroma_uncompressed(const std::shared_ptr<const Box_uncC>& uncC,
                                            const std::shared_ptr<const Box_cmpd>& cmpd,
                                            heif_chroma* out_chroma,
                                            heif_colorspace* out_colourspace,
                                            bool* out_has_alpha);

  static Result<std::shared_ptr<HeifPixelImage>> create_image(std::shared_ptr<const Box_cmpd>,
                                                              std::shared_ptr<const Box_uncC>,
                                                              uint32_t width,
                                                              uint32_t height,
                                                              const heif_security_limits* limits);

  static Error check_header_validity(std::optional<const std::shared_ptr<const Box_ispe>>,
                                     const std::shared_ptr<const Box_cmpd>&,
                                     const std::shared_ptr<const Box_uncC>&);
};

#endif //LIBHEIF_UNC_CODEC_H
