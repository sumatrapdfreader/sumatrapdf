/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_COLORCONVERSION_H
#define LIBHEIF_COLORCONVERSION_H

#include "pixelimage.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>


struct ColorState
{
  heif_colorspace colorspace = heif_colorspace_undefined;
  heif_chroma chroma = heif_chroma_undefined;
  bool has_alpha = false;
  int bits_per_pixel = 8;

  // ColorConversionOperations can assume that the input and target nclx has no 'unspecified' values
  // if the colorspace is heif_colorspace_YCbCr. Otherwise, the values should preferably be 'unspecified'.
  nclx_profile nclx;

  ColorState() = default;

  ColorState(heif_colorspace colorspace, heif_chroma chroma, bool has_alpha, int bits_per_pixel)
      : colorspace(colorspace), chroma(chroma), has_alpha(has_alpha), bits_per_pixel(bits_per_pixel) {}

  bool operator==(const ColorState&) const;
};

std::ostream& operator<<(std::ostream& ostr, const ColorState& state);

// These are some integer constants for typical color conversion Op speed costs.
// The integer value is the speed cost. Any other integer can be assigned to the speed cost.
enum SpeedCosts
{
  SpeedCosts_Trivial = 1,
  SpeedCosts_Hardware = 2,
  SpeedCosts_OptimizedSoftware = 5 + 1,
  SpeedCosts_Unoptimized = 10 + 1,
  SpeedCosts_Slow = 15 + 1
};


struct ColorStateWithCost
{
  ColorStateWithCost(ColorState c, int s) : color_state(std::move(c)), speed_costs(s) {}

  ColorState color_state;

  int speed_costs;
};


class ColorConversionOperation
{
public:
  virtual ~ColorConversionOperation() = default;

  // We specify the target state to control the conversion into a direction that is most
  // suitable for reaching the target state. That allows one conversion operation to
  // provide a range of conversion options.
  // Also returns the cost for this conversion.
  virtual std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const heif_color_conversion_options& options,
                         const heif_color_conversion_options_ext& options_ext) const = 0;

  virtual Result<std::shared_ptr<HeifPixelImage>>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& input_state,
                     const ColorState& target_state,
                     const heif_color_conversion_options& options,
                     const heif_color_conversion_options_ext& options_ext,
                     const heif_security_limits* limits) const = 0;
};


class ColorConversionPipeline
{
public:
  static void init_ops();
  static void release_ops();

  bool is_nop() const { return m_conversion_steps.empty(); }

  bool construct_pipeline(const ColorState& input_state,
                          const ColorState& target_state,
                          const heif_color_conversion_options& options,
                          const heif_color_conversion_options_ext& options_ext);

  Result<std::shared_ptr<HeifPixelImage>> convert_image(const std::shared_ptr<HeifPixelImage>& input,
                                                        const heif_security_limits* limits);

  std::string debug_dump_pipeline() const;

private:
  static std::vector<std::shared_ptr<ColorConversionOperation>> m_operation_pool;

  struct ConversionStep {
    std::shared_ptr<ColorConversionOperation> operation;
    ColorState input_state;
    ColorState output_state;
  };

  std::vector<ConversionStep> m_conversion_steps;

  heif_color_conversion_options m_options;
  heif_color_conversion_options_ext m_options_ext;
};


// If no conversion is required, the input is simply passed through without copy.
// The input image is never modified by this function, but the input is still non-const because we may pass it through.
Result<std::shared_ptr<HeifPixelImage>> convert_colorspace(const std::shared_ptr<HeifPixelImage>& input,
                                                           heif_colorspace colorspace,
                                                           heif_chroma chroma,
                                                           const nclx_profile& target_profile,
                                                           int output_bpp,
                                                           const heif_color_conversion_options& options,
                                                           const heif_color_conversion_options_ext* options_ext,
                                                           const heif_security_limits* limits);

Result<std::shared_ptr<const HeifPixelImage>> convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                                 heif_colorspace colorspace,
                                                                 heif_chroma chroma,
                                                                 const nclx_profile& target_profile,
                                                                 int output_bpp,
                                                                 const heif_color_conversion_options& options,
                                                                 const heif_color_conversion_options_ext* options_ext,
                                                                 const heif_security_limits* limits);

#endif
