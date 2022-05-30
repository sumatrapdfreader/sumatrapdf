/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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


#include "heif_image.h"
#include <memory>
#include <vector>


namespace heif {

  struct ColorState
  {
    heif_colorspace colorspace = heif_colorspace_undefined;
    heif_chroma chroma = heif_chroma_undefined;
    bool has_alpha = false;
    int bits_per_pixel = 8;
    std::shared_ptr<const color_profile_nclx> nclx_profile;

    ColorState() = default;

    ColorState(heif_colorspace colorspace, heif_chroma chroma, bool has_alpha, int bits_per_pixel)
        : colorspace(colorspace), chroma(chroma), has_alpha(has_alpha), bits_per_pixel(bits_per_pixel)
    {}

    bool operator==(const ColorState&) const;
  };


  enum class ColorConversionCriterion
  {
    Speed,
    Quality,
    Memory,
    Balanced
  };


  struct ColorConversionCosts
  {
    ColorConversionCosts() = default;

    ColorConversionCosts(float _speed, float _quality, float _memory)
    {
      speed = _speed;
      quality = _quality;
      memory = _memory;
    }

    float speed = 0;
    float quality = 0;
    float memory = 0;

    ColorConversionCosts operator+(const ColorConversionCosts& b) const
    {
      return {speed + b.speed,
              quality + b.quality,
              memory + b.memory};
    }

    float total(ColorConversionCriterion) const
    {
      return speed * 0.3f + quality * 0.6f + memory * 0.1f;
    }
  };


  struct ColorConversionOptions
  {
    ColorConversionCriterion criterion = ColorConversionCriterion::Balanced;
  };


  struct ColorStateWithCost
  {
    ColorState color_state;
    ColorConversionCosts costs;
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
                           const ColorConversionOptions& options = ColorConversionOptions()) = 0;

    virtual std::shared_ptr<HeifPixelImage>
    convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                       const ColorState& target_state,
                       const ColorConversionOptions& options = ColorConversionOptions()) = 0;
  };


  class ColorConversionPipeline
  {
  public:
    bool construct_pipeline(const ColorState& input_state,
                            const ColorState& target_state,
                            const ColorConversionOptions& options = ColorConversionOptions());

    std::shared_ptr<HeifPixelImage>
    convert_image(const std::shared_ptr<HeifPixelImage>& input);

    void debug_dump_pipeline() const;

  private:
    std::vector<std::shared_ptr<ColorConversionOperation>> m_operations;
    ColorState m_target_state;
    ColorConversionOptions m_options;
  };


  std::shared_ptr<HeifPixelImage> convert_colorspace(const std::shared_ptr<HeifPixelImage>& input,
                                                     heif_colorspace colorspace,
                                                     heif_chroma chroma,
                                                     const std::shared_ptr<const color_profile_nclx>& target_profile,
                                                     int output_bpp = 0);
}
