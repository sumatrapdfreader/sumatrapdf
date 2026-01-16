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


#include "colorconversion.h"
#include "common_utils.h"
#include "nclx.h"
#include <typeinfo>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>
#include <set>
#include <cmath>
#include <limits>
#include <string>
#include "rgb2yuv.h"
#include "rgb2yuv_sharp.h"
#include "yuv2rgb.h"
#include "rgb2rgb.h"
#include "monochrome.h"
#include "alpha.h"
#include "hdr_sdr.h"
#include "chroma_sampling.h"

#if ENABLE_MULTITHREADING_SUPPORT

#include <mutex>

#endif

#define DEBUG_ME 0
#define DEBUG_PIPELINE_CREATION 0

#define USE_CENTER_CHROMA_422 0


std::ostream& operator<<(std::ostream& ostr, heif_colorspace c)
{
  switch (c) {
    case heif_colorspace_RGB:
      ostr << "RGB";
      break;
    case heif_colorspace_YCbCr:
      ostr << "YCbCr";
      break;
    case heif_colorspace_monochrome:
      ostr << "mono";
      break;
    case heif_colorspace_undefined:
      ostr << "undefined";
      break;
    default:
      assert(false);
  }

  return ostr;
}

std::ostream& operator<<(std::ostream& ostr, heif_chroma c)
{
  switch (c) {
    case heif_chroma_420:
      ostr << "420";
      break;
    case heif_chroma_422:
      ostr << "422";
      break;
    case heif_chroma_444:
      ostr << "444";
      break;
    case heif_chroma_monochrome:
      ostr << "mono";
      break;
    case heif_chroma_interleaved_RGB:
      ostr << "RGB";
      break;
    case heif_chroma_interleaved_RGBA:
      ostr << "RGBA";
      break;
    case heif_chroma_interleaved_RRGGBB_BE:
      ostr << "RRGGBB_BE";
      break;
    case heif_chroma_interleaved_RRGGBB_LE:
      ostr << "RRGGBBB_LE";
      break;
    case heif_chroma_interleaved_RRGGBBAA_BE:
      ostr << "RRGGBBAA_BE";
      break;
    case heif_chroma_interleaved_RRGGBBAA_LE:
      ostr << "RRGGBBBAA_LE";
      break;
    case heif_chroma_undefined:
      ostr << "undefined";
      break;
    default:
      assert(false);
  }

  return ostr;
}

#if DEBUG_ME

static void __attribute__ ((unused)) print_spec(std::ostream& ostr, const std::shared_ptr<HeifPixelImage>& img)
{
  ostr << "colorspace=" << img->get_colorspace()
       << " chroma=" << img->get_chroma_format();

  if (img->get_colorspace() == heif_colorspace_RGB) {
    if (img->get_chroma_format() == heif_chroma_444) {
      ostr << " bpp(R)=" << ((int) img->get_bits_per_pixel(heif_channel_R));
    }
    else {
      ostr << " bpp(interleaved)=" << ((int) img->get_bits_per_pixel(heif_channel_interleaved));
    }
  }
  else if (img->get_colorspace() == heif_colorspace_YCbCr ||
           img->get_colorspace() == heif_colorspace_monochrome) {
    ostr << " bpp(Y)=" << ((int) img->get_bits_per_pixel(heif_channel_Y));
  }

  ostr << "\n";
}

#endif


bool ColorState::operator==(const ColorState& b) const
{
  bool mainParamsMatch = (colorspace == b.colorspace &&
                          chroma == b.chroma &&
                          has_alpha == b.has_alpha &&
                          bits_per_pixel == b.bits_per_pixel);

  if (!mainParamsMatch) {
    return false;
  }

  if (colorspace == heif_colorspace_YCbCr) {
    bool ycbcr_parameters_match = nclx.equal_except_transfer_curve(b.nclx);

    if (!ycbcr_parameters_match) {
      return false;
    }
  }

  return true;
}


struct Node
{
  Node() = default;

  Node(int prev,
       const std::shared_ptr<ColorConversionOperation>& _op,
      //const ColorState& _input_state,
       const ColorState& _output_state,
       int _speed_cost)
  {
    prev_processed_idx = prev;
    op = _op;
    //input_state = _input_state;
    output_state = _output_state;
    speed_costs = _speed_cost;
  }

  int prev_processed_idx = -1;
  std::shared_ptr<ColorConversionOperation> op;
  //ColorState input_state;
  ColorState output_state;
  int speed_costs;
};

std::ostream& operator<<(std::ostream& ostr, const ColorState& state)
{
  ostr << "colorspace=" << state.colorspace << " chroma=" << state.chroma
           << " bpp(R)=" << state.bits_per_pixel
              << " alpha=" << (state.has_alpha ? "yes" : "no");

  if (state.colorspace == heif_colorspace_YCbCr) {
    ostr << " matrix-coefficients=" << state.nclx.get_matrix_coefficients()
         << " colour-primaries=" << state.nclx.get_colour_primaries()
         << " transfer-characteristics=" << state.nclx.get_transfer_characteristics()
         << " full-range=" << (state.nclx.get_full_range_flag() ? "yes" : "no");
  }

  return ostr;
}

std::vector<std::shared_ptr<ColorConversionOperation>> ColorConversionPipeline::m_operation_pool;

void ColorConversionPipeline::init_ops()
{
#if ENABLE_MULTITHREADING_SUPPORT
  static std::mutex init_ops_mutex;
  std::lock_guard<std::mutex> lock(init_ops_mutex);
#endif
  if (!m_operation_pool.empty()) {
    return;
  }

  std::vector<std::shared_ptr<ColorConversionOperation>>& ops = m_operation_pool;
  ops.emplace_back(std::make_shared<Op_RGB_to_RGB24_32>());
  ops.emplace_back(std::make_shared<Op_RGB24_32_to_RGB>());
  ops.emplace_back(std::make_shared<Op_YCbCr_to_RGB<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr_to_RGB<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr420_to_RGB24>());
  ops.emplace_back(std::make_shared<Op_YCbCr420_to_RGB32>());
  ops.emplace_back(std::make_shared<Op_YCbCr420_to_RRGGBBaa>());
  ops.emplace_back(std::make_shared<Op_RGB_HDR_to_RRGGBBaa_BE>());
  ops.emplace_back(std::make_shared<Op_RGB_to_RRGGBBaa_BE>());
  ops.emplace_back(std::make_shared<Op_mono_to_YCbCr420>());
  ops.emplace_back(std::make_shared<Op_mono_to_RGB24_32>());
  ops.emplace_back(std::make_shared<Op_RRGGBBaa_swap_endianness>());
  ops.emplace_back(std::make_shared<Op_RRGGBBaa_BE_to_RGB_HDR>());
  ops.emplace_back(std::make_shared<Op_RGB24_32_to_YCbCr>());
  ops.emplace_back(std::make_shared<Op_RGB_to_YCbCr<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_RGB_to_YCbCr<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_RRGGBBxx_HDR_to_YCbCr420>());
  ops.emplace_back(std::make_shared<Op_RGB24_32_to_YCbCr444_GBR>());
  ops.emplace_back(std::make_shared<Op_drop_alpha_plane>());
  ops.emplace_back(std::make_shared<Op_flatten_alpha_plane<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_flatten_alpha_plane<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_to_hdr_planes>());
  ops.emplace_back(std::make_shared<Op_to_sdr_planes>());
  ops.emplace_back(std::make_shared<Op_YCbCr420_bilinear_to_YCbCr444<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr420_bilinear_to_YCbCr444<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr422_bilinear_to_YCbCr444<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr422_bilinear_to_YCbCr444<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr444_to_YCbCr420_average<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr444_to_YCbCr420_average<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr444_to_YCbCr422_average<uint8_t>>());
  ops.emplace_back(std::make_shared<Op_YCbCr444_to_YCbCr422_average<uint16_t>>());
  ops.emplace_back(std::make_shared<Op_Any_RGB_to_YCbCr_420_Sharp>());
}


void ColorConversionPipeline::release_ops()
{
  m_operation_pool.clear();
}


bool ColorConversionPipeline::construct_pipeline(const ColorState& input_state,
                                                 const ColorState& target_state,
                                                 const heif_color_conversion_options& options,
                                                 const heif_color_conversion_options_ext& options_ext)
{
  m_conversion_steps.clear();

  m_options = options;
  m_options_ext = options_ext;

  if (input_state == target_state) {
    return true;
  }

#if DEBUG_ME
  std::cerr << "--- construct_pipeline\n";
  std::cerr << "from: " << input_state << "\nto: " << target_state << "\n";
#endif

  init_ops(); // to be sure these are initialized even without heif_init()

  std::vector<std::shared_ptr<ColorConversionOperation>>& ops = m_operation_pool;

  // --- Dijkstra search for the minimum-cost conversion pipeline

  std::vector<Node> processed_states;
  std::vector<Node> border_states;
  border_states.emplace_back(-1, nullptr, input_state, 0);

  while (!border_states.empty()) {
    int minIdx = -1;
    int minCost = std::numeric_limits<int>::max();
    for (int i = 0; i < (int) border_states.size(); i++) {
      int cost = border_states[i].speed_costs;
      if (cost < minCost) {
        minIdx = i;
        minCost = cost;
      }
    }

    assert(minIdx >= 0);


    // move minimum-cost border_state into processed_states

    processed_states.push_back(border_states[minIdx]);

    border_states[minIdx] = border_states.back();
    border_states.pop_back();

#if DEBUG_PIPELINE_CREATION
    std::cerr << "- expand node: " << processed_states.back().output_state
        << " with cost " << processed_states.back().speed_costs << " \n";
#endif

    if (processed_states.back().output_state == target_state) {
      // end-state found, backtrack path to find conversion pipeline

      size_t idx = processed_states.size() - 1;
      int len = 0;
      while (idx > 0) {
        idx = processed_states[idx].prev_processed_idx;
        len++;
      }

      m_conversion_steps.resize(len);

      idx = processed_states.size() - 1;
      int step = 0;
      while (idx > 0) {
        m_conversion_steps[len - 1 - step].operation = processed_states[idx].op;
        m_conversion_steps[len - 1 - step].output_state = processed_states[idx].output_state;
        if (step > 0) {
          m_conversion_steps[len - step].input_state = m_conversion_steps[len - 1 - step].output_state;
        }

        //printf("cost: %f\n",processed_states[idx].color_state.costs.total(options.criterion));
        idx = processed_states[idx].prev_processed_idx;
        step++;
      }

      m_conversion_steps[0].input_state = input_state;

      assert(m_conversion_steps.back().output_state == target_state);

#if DEBUG_ME
      std::cerr << debug_dump_pipeline();
#endif

      return true;
    }


    // expand the node with minimum cost

    for (const auto& op_ptr : ops) {

#if DEBUG_PIPELINE_CREATION
      auto& op = *op_ptr;
      std::cerr << "-- apply op: " << typeid(op).name() << "\n";
#endif

      auto out_states = op_ptr->state_after_conversion(processed_states.back().output_state,
                                                       target_state,
                                                       options, options_ext);
      for (const auto& out_state : out_states) {
        int new_op_costs = out_state.speed_costs + processed_states.back().speed_costs;
#if DEBUG_PIPELINE_CREATION
        std::cerr << "--- " << out_state.color_state << " with cost " << new_op_costs << "\n";
#endif

        bool state_exists = false;
        for (const auto& s : processed_states) {
          if (s.output_state == out_state.color_state) {
            state_exists = true;
            break;
          }
        }

        if (!state_exists) {
          for (auto& s : border_states) {
            if (s.output_state == out_state.color_state) {
              state_exists = true;

              // if we reached the same border node with a lower cost, replace the border node

              if (s.speed_costs > new_op_costs) {
                s = {(int) (processed_states.size() - 1),
                     op_ptr,
                     out_state.color_state,
                     out_state.speed_costs};

                s.speed_costs = new_op_costs;
              }
              break;
            }
          }
        }


        // enter the new output state into the list of border states

        if (!state_exists) {
          ColorStateWithCost s = out_state;
          s.speed_costs = s.speed_costs + processed_states.back().speed_costs;

          border_states.emplace_back((int) (processed_states.size() - 1),
                                     op_ptr,
                                     s.color_state,
                                     s.speed_costs);
        }
      }
    }
  }

  return false;
}


std::string ColorConversionPipeline::debug_dump_pipeline() const
{
  std::ostringstream ostr;
  ostr << "final pipeline has " << m_conversion_steps.size() << " steps:\n";
  for (const auto& step : m_conversion_steps) {
    auto& op = *step.operation;
    ostr << "> " << typeid(op).name() << "\n";
  }
  return ostr.str();
}


Result<std::shared_ptr<HeifPixelImage>> ColorConversionPipeline::convert_image(const std::shared_ptr<HeifPixelImage>& input,
                                                                               const heif_security_limits* limits)
{
  std::shared_ptr<HeifPixelImage> in = input;
  std::shared_ptr<HeifPixelImage> out = in;

  for (const auto& step : m_conversion_steps) {

#if DEBUG_ME
    std::cerr << "input spec: ";
    print_spec(std::cerr, in);
#endif

    auto outResult = step.operation->convert_colorspace(in, step.input_state, step.output_state, m_options, m_options_ext, limits);
    if (!outResult) {
      return outResult.error();
    }
    else {
      out = *outResult;
    }

    // --- pass the color profiles to the new image

    out->set_color_profile_nclx(step.output_state.nclx);
    out->set_color_profile_icc(in->get_color_profile_icc());

    out->set_premultiplied_alpha(in->is_premultiplied_alpha());

    // pass through HDR information
    if (in->has_clli()) {
      out->set_clli(in->get_clli());
    }

    if (in->has_mdcv()) {
      out->set_mdcv(in->get_mdcv());
    }

    if (in->has_nonsquare_pixel_ratio()) {
      uint32_t h, v;
      in->get_pixel_ratio(&h, &v);
      out->set_pixel_ratio(h, v);
    }

    if (in->has_gimi_sample_content_id()) {
      out->set_gimi_sample_content_id(in->get_gimi_sample_content_id());
    }

    if (auto* tai = in->get_tai_timestamp()) {
      out->set_tai_timestamp(tai);
    }

    out->set_sample_duration(in->get_sample_duration());

    const auto& warnings = in->get_warnings();
    for (const auto& warning : warnings) {
      out->add_warning(warning);
    }

    in = out;
  }

  return out;
}


Result<std::shared_ptr<HeifPixelImage>> convert_colorspace(const std::shared_ptr<HeifPixelImage>& input,
                                                           heif_colorspace target_colorspace,
                                                           heif_chroma target_chroma,
                                                           const nclx_profile& target_profile,
                                                           int output_bpp,
                                                           const heif_color_conversion_options& options,
                                                           const heif_color_conversion_options_ext* options_ext_optional,
                                                           const heif_security_limits* limits)
{
  std::unique_ptr<heif_color_conversion_options_ext, void(*)(heif_color_conversion_options_ext*)>
      options_ext(heif_color_conversion_options_ext_alloc(), heif_color_conversion_options_ext_free);

  heif_color_conversion_options_ext_copy(options_ext.get(), options_ext_optional);


  // --- check that input image is valid

  uint32_t width = input->get_width();
  uint32_t height = input->get_height();

  // alpha image should have full image resolution

  if (input->has_channel(heif_channel_Alpha)) {
    if (input->get_width(heif_channel_Alpha) != width ||
        input->get_height(heif_channel_Alpha) != height) {
      return Error::InternalError;
    }
  }

  // check for valid target YCbCr chroma formats

  if (target_colorspace == heif_colorspace_YCbCr) {
    if (target_chroma != heif_chroma_420 &&
        target_chroma != heif_chroma_422 &&
        target_chroma != heif_chroma_444) {
      return Error::InternalError;
    }
  }

  // --- prepare conversion

  ColorState input_state;
  input_state.colorspace = input->get_colorspace();
  input_state.chroma = input->get_chroma_format();
  input_state.has_alpha = input->has_channel(heif_channel_Alpha) || is_interleaved_with_alpha(input->get_chroma_format());
  if (input->has_nclx_color_profile()) {
    input_state.nclx = input->get_color_profile_nclx();
  }

  input_state.nclx.replace_undefined_values_with_sRGB_defaults();

  std::set<enum heif_channel> channels = input->get_channel_set();
  assert(!channels.empty());
  input_state.bits_per_pixel = input->get_bits_per_pixel(*(channels.begin()));

  ColorState output_state = input_state;
  output_state.colorspace = target_colorspace;
  output_state.chroma = target_chroma;
  output_state.nclx = target_profile;

  // If some output nclx values are unspecified, set them to the same as the input.

  if (output_state.nclx.get_matrix_coefficients() == heif_matrix_coefficients_unspecified) {
    output_state.nclx.set_matrix_coefficients(input_state.nclx.get_matrix_coefficients());
  }

  if (output_state.nclx.get_colour_primaries() == heif_color_primaries_unspecified) {
    output_state.nclx.set_colour_primaries(input_state.nclx.get_colour_primaries());
  }

  if (output_state.nclx.get_transfer_characteristics() == heif_transfer_characteristic_unspecified) {
    output_state.nclx.set_transfer_characteristics(input_state.nclx.get_transfer_characteristics());
  }

  // If we convert to an interleaved format, we want alpha only if present in the
  // interleaved output format.
  // For planar formats, we include an alpha plane when included in the input.

  if (num_interleaved_pixels_per_plane(target_chroma) > 1) {
    output_state.has_alpha = is_interleaved_with_alpha(target_chroma);
  }
  else {
    if (options_ext->alpha_composition_mode != heif_alpha_composition_mode_none) {
      output_state.has_alpha = false;
    }
    else {
      output_state.has_alpha = input_state.has_alpha;
    }
  }

  if (output_bpp) {
    output_state.bits_per_pixel = output_bpp;
  }


  // interleaved RGB formats always have to be 8-bit

  if (target_chroma == heif_chroma_interleaved_RGB ||
      target_chroma == heif_chroma_interleaved_RGBA) {
    output_state.bits_per_pixel = 8;
  }

  // interleaved RRGGBB formats have to be >8-bit.
  // If we don't know a target bit-depth, use 10 bit.

  if ((target_chroma == heif_chroma_interleaved_RRGGBB_LE ||
       target_chroma == heif_chroma_interleaved_RRGGBB_BE ||
       target_chroma == heif_chroma_interleaved_RRGGBBAA_LE ||
       target_chroma == heif_chroma_interleaved_RRGGBBAA_BE) &&
      output_state.bits_per_pixel <= 8) {
    output_state.bits_per_pixel = 10;
  }

  ColorConversionPipeline pipeline;
  bool success = pipeline.construct_pipeline(input_state, output_state, options, *options_ext);
  if (!success) {
    return Error{heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_color_conversion};
  }

  if (pipeline.is_nop()) {
    return input;
  }
  else {
    return pipeline.convert_image(input, limits);
  }
}


Result<std::shared_ptr<const HeifPixelImage>> convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                                 heif_colorspace colorspace,
                                                                 heif_chroma chroma,
                                                                 const nclx_profile& target_profile,
                                                                 int output_bpp,
                                                                 const heif_color_conversion_options& options,
                                                                 const heif_color_conversion_options_ext* options_ext,
                                                                 const heif_security_limits* limits)
{
  std::shared_ptr<HeifPixelImage> non_const_input = std::const_pointer_cast<HeifPixelImage>(input);

  auto result = convert_colorspace(non_const_input, colorspace, chroma, target_profile, output_bpp, options, options_ext, limits);
  if (!result) {
    return result.error();
  }
  else {
    // TODO: can we simplify this? It's a bit awkward to do these assignments just to get a "const HeifPixelImage".
    std::shared_ptr<const HeifPixelImage> constImage = *result;
    return constImage;
  }
}
