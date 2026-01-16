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

#ifndef LIBHEIF_RGB2RGB_H
#define LIBHEIF_RGB2RGB_H

#include "colorconversion.h"
#include <vector>
#include <memory>


class Op_RGB_to_RGB24_32 : public ColorConversionOperation
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


class Op_RGB_HDR_to_RRGGBBaa_BE : public ColorConversionOperation
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


class Op_RGB_to_RRGGBBaa_BE : public ColorConversionOperation
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


class Op_RRGGBBaa_BE_to_RGB_HDR : public ColorConversionOperation
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


class Op_RGB24_32_to_RGB : public ColorConversionOperation
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


class Op_RRGGBBaa_swap_endianness : public ColorConversionOperation
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


#endif //LIBHEIF_RGB2RGB_H
