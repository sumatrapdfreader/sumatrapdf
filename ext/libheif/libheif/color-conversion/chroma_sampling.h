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

#ifndef LIBHEIF_CHROMA_SAMPLING_H
#define LIBHEIF_CHROMA_SAMPLING_H

#include "color-conversion/colorconversion.h"
#include <memory>
#include <vector>


// --- downsampling ---

template <class Pixel>
class Op_YCbCr444_to_YCbCr420_average : public ColorConversionOperation
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


template <class Pixel>
class Op_YCbCr444_to_YCbCr422_average : public ColorConversionOperation
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


// --- upsampling ---

template <class Pixel>
class Op_YCbCr420_bilinear_to_YCbCr444 : public ColorConversionOperation
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

template <class Pixel>
class Op_YCbCr422_bilinear_to_YCbCr444 : public ColorConversionOperation
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

#endif //LIBHEIF_CHROMA_SAMPLING_H
