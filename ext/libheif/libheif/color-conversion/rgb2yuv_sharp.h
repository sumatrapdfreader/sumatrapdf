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

#ifndef LIBHEIF_RGB2YUV_SHARP_H
#define LIBHEIF_RGB2YUV_SHARP_H

#include "colorconversion.h"
#include <vector>
#include <memory>


class Op_Any_RGB_to_YCbCr_420_Sharp : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const heif_color_conversion_options& options,
                         const heif_color_conversion_options_ext& options_ext) const override;

  Result<std::shared_ptr<HeifPixelImage>>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& input_state,
                     const ColorState& target_state,
                     const heif_color_conversion_options& options,
                     const heif_color_conversion_options_ext& options_ext,
                     const heif_security_limits* limits) const override;
};


#endif //LIBHEIF_RGB2YUV_SHARP_H
