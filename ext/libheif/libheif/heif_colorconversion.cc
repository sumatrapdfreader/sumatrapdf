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


#include "heif_colorconversion.h"
#include "nclx.h"
#include <typeinfo>
#include <algorithm>
#include <cstring>
#include <cassert>
#include <iostream>
#include <set>
#include <cmath>

using namespace heif;

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


static void __attribute__ ((unused)) print_state(std::ostream& ostr, const ColorState& state)
{
  ostr << "colorspace=" << state.colorspace
       << " chroma=" << state.chroma;

  ostr << " bpp(R)=" << state.bits_per_pixel;
  ostr << " alpha=" << (state.has_alpha ? "yes" : "no");
  ostr << " nclx=" << (state.nclx_profile ? "yes" : "no");
  ostr << "\n";
}

#endif


bool ColorState::operator==(const ColorState& b) const
{
  return (colorspace == b.colorspace &&
          chroma == b.chroma &&
          has_alpha == b.has_alpha &&
          bits_per_pixel == b.bits_per_pixel);
}


class Op_RGB_to_RGB24_32 : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RGB_to_RGB24_32::state_after_conversion(const ColorState& input_state,
                                           const ColorState& target_state,
                                           const ColorConversionOptions& options)
{
  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444 ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RGBA (with alpha)

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGBA;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = 8;

  if (input_state.has_alpha == false &&
      target_state.has_alpha == false) {
    costs = ColorConversionCosts(0.1f, 0.0f, 0.25f);
  }
  else {
    costs = ColorConversionCosts(0.1f, 0.0f, 0.0f);
  }

  states.push_back({output_state, costs});


  // --- convert to RGB (without alpha)

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGB;
  output_state.has_alpha = false;
  output_state.bits_per_pixel = 8;

  if (input_state.has_alpha == true &&
      target_state.has_alpha == true) {
    // do not use this conversion because we would lose the alpha channel
  }
  else {
    costs = ColorConversionCosts(0.2f, 0.0f, 0.0f);
  }

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RGB_to_RGB24_32::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                       const ColorState& target_state,
                                       const ColorConversionOptions& options)
{
  bool has_alpha = input->has_channel(heif_channel_Alpha);
  bool want_alpha = target_state.has_alpha;

  if (input->get_bits_per_pixel(heif_channel_R) != 8 ||
      input->get_bits_per_pixel(heif_channel_G) != 8 ||
      input->get_bits_per_pixel(heif_channel_B) != 8) {
    return nullptr;
  }

  if (has_alpha && input->get_bits_per_pixel(heif_channel_Alpha) != 8) {
    return nullptr;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB,
                 want_alpha ? heif_chroma_interleaved_32bit : heif_chroma_interleaved_24bit);

  outimg->add_plane(heif_channel_interleaved, width, height, 8);

  const uint8_t* in_r, * in_g, * in_b, * in_a = nullptr;
  int in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_r = input->get_plane(heif_channel_R, &in_r_stride);
  in_g = input->get_plane(heif_channel_G, &in_g_stride);
  in_b = input->get_plane(heif_channel_B, &in_b_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  if (has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  int x, y;
  for (y = 0; y < height; y++) {

    if (has_alpha && want_alpha) {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 4 * x + 0] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 4 * x + 1] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 4 * x + 2] = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + 4 * x + 3] = in_a[x + y * in_a_stride];
      }
    }
    else if (!want_alpha) {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 3 * x + 0] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 3 * x + 1] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 3 * x + 2] = in_b[x + y * in_b_stride];
      }
    }
    else {
      assert(want_alpha && !has_alpha);

      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 4 * x + 0] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 4 * x + 1] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 4 * x + 2] = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + 4 * x + 3] = 0xFF;
      }
    }
  }

  return outimg;
}


static inline uint8_t clip_int_u8(int x)
{
  if (x < 0) return 0;
  if (x > 255) return 255;
  return static_cast<uint8_t>(x);
}


static inline uint16_t clip_f_u16(float fx, int32_t maxi)
{
  long x = (long int) (fx + 0.5f);
  if (x < 0) return 0;
  if (x > maxi) return (uint16_t) maxi;
  return static_cast<uint16_t>(x);
}


template<class Pixel>
class Op_YCbCr_to_RGB : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


template<class Pixel>
std::vector<ColorStateWithCost>
Op_YCbCr_to_RGB<Pixel>::state_after_conversion(const ColorState& input_state,
                                               const ColorState& target_state,
                                               const ColorConversionOptions& options)
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  if ((input_state.bits_per_pixel != 8) != hdr) {
    return {};
  }

  if (input_state.colorspace != heif_colorspace_YCbCr) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RGB

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_444;
  output_state.has_alpha = input_state.has_alpha;  // we simply keep the old alpha plane
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


template<class Pixel>
std::shared_ptr<HeifPixelImage>
Op_YCbCr_to_RGB<Pixel>::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                           const ColorState& target_state,
                                           const ColorConversionOptions& options)
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  heif_chroma chroma = input->get_chroma_format();

  int bpp_y = input->get_bits_per_pixel(heif_channel_Y);
  int bpp_cb = input->get_bits_per_pixel(heif_channel_Cb);
  int bpp_cr = input->get_bits_per_pixel(heif_channel_Cr);
  int bpp_a = 0;

  bool has_alpha = input->has_channel(heif_channel_Alpha);

  if (has_alpha) {
    bpp_a = input->get_bits_per_pixel(heif_channel_Alpha);
  }

  if (!hdr) {
    if (bpp_y != 8 ||
        bpp_cb != 8 ||
        bpp_cr != 8) {
      return nullptr;
    }
  }
  else {
    if (bpp_y == 8 ||
        bpp_cb == 8 ||
        bpp_cr == 8) {
      return nullptr;
    }
  }


  if (bpp_y != bpp_cb ||
      bpp_y != bpp_cr) {
    // TODO: test with varying bit depths when we have a test image
    return nullptr;
  }


  auto colorProfile = input->get_color_profile_nclx();

  int width = input->get_width();
  int height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_RGB, heif_chroma_444);

  outimg->add_plane(heif_channel_R, width, height, bpp_y);
  outimg->add_plane(heif_channel_G, width, height, bpp_y);
  outimg->add_plane(heif_channel_B, width, height, bpp_y);

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, bpp_a);
  }

  const Pixel* in_y, * in_cb, * in_cr, * in_a;
  int in_y_stride = 0, in_cb_stride = 0, in_cr_stride = 0, in_a_stride = 0;

  Pixel* out_r, * out_g, * out_b, * out_a;
  int out_r_stride = 0, out_g_stride = 0, out_b_stride = 0, out_a_stride = 0;

  in_y = (const Pixel*) input->get_plane(heif_channel_Y, &in_y_stride);
  in_cb = (const Pixel*) input->get_plane(heif_channel_Cb, &in_cb_stride);
  in_cr = (const Pixel*) input->get_plane(heif_channel_Cr, &in_cr_stride);
  out_r = (Pixel*) outimg->get_plane(heif_channel_R, &out_r_stride);
  out_g = (Pixel*) outimg->get_plane(heif_channel_G, &out_g_stride);
  out_b = (Pixel*) outimg->get_plane(heif_channel_B, &out_b_stride);

  if (has_alpha) {
    in_a = (const Pixel*) input->get_plane(heif_channel_Alpha, &in_a_stride);
    out_a = (Pixel*) outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }
  else {
    in_a = nullptr;
    out_a = nullptr;
  }


  uint16_t halfRange = (uint16_t) (1 << (bpp_y - 1));
  int32_t fullRange = (1 << bpp_y) - 1;

  int shiftH = chroma_h_subsampling(chroma) - 1;
  int shiftV = chroma_v_subsampling(chroma) - 1;

  if (hdr) {
    in_y_stride /= 2;
    in_cb_stride /= 2;
    in_cr_stride /= 2;
    in_a_stride /= 2;
    out_r_stride /= 2;
    out_g_stride /= 2;
    out_b_stride /= 2;
    out_a_stride /= 2;
  }

  int matrix_coeffs = 2;
  bool full_range_flag = true;
  YCbCr_to_RGB_coefficients coeffs = YCbCr_to_RGB_coefficients::defaults();
  if (colorProfile) {
    matrix_coeffs = colorProfile->get_matrix_coefficients();
    full_range_flag = colorProfile->get_full_range_flag();
    coeffs = heif::get_YCbCr_to_RGB_coefficients(colorProfile->get_matrix_coefficients(),
                                                 colorProfile->get_colour_primaries());
  }


  int x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int cx = (x >> shiftH);
      int cy = (y >> shiftV);

      if (matrix_coeffs == 0) {
        if (full_range_flag) {
          out_r[y * out_r_stride + x] = in_cr[cy * in_cr_stride + cx];
          out_g[y * out_g_stride + x] = in_y[y * in_y_stride + x];
          out_b[y * out_b_stride + x] = in_cb[cy * in_cb_stride + cx];
        }
        else {
          out_r[y * out_r_stride + x] = Pixel(((in_cr[cy * in_cr_stride + cx] * 219 + 128) >> 8) + 16);
          out_g[y * out_g_stride + x] = Pixel(((in_y[y * in_y_stride + x] * 219 + 128) >> 8) + 16);
          out_b[y * out_b_stride + x] = Pixel(((in_cb[cy * in_cb_stride + cx] * 219 + 128) >> 8) + 16);
        }
      }
      else if (matrix_coeffs == 8) {
        // TODO: check this. I have no input image yet which is known to be correct.
        // TODO: is there a coeff=8 with full_range=false ?

        int yv = in_y[y * in_y_stride + x];
        int cb = in_cb[cy * in_cb_stride + cx] - halfRange;
        int cr = in_cr[cy * in_cr_stride + cx] - halfRange;

        out_r[y * out_r_stride + x] = (Pixel) (clip_int_u8(yv - cb + cr));
        out_g[y * out_g_stride + x] = (Pixel) (clip_int_u8(yv + cb));
        out_b[y * out_b_stride + x] = (Pixel) (clip_int_u8(yv - cb - cr));
      }
      else { // TODO: matrix_coefficients = 10,11,13,14
        float yv, cb, cr;
        yv = static_cast<float>(in_y[y * in_y_stride + x] );
        cb = static_cast<float>(in_cb[cy * in_cb_stride + cx] - halfRange);
        cr = static_cast<float>(in_cr[cy * in_cr_stride + cx] - halfRange);

        if (!full_range_flag) {
          yv = (yv - 16) * 1.1689f;
          cb = cb * 1.1429f;
          cr = cr * 1.1429f;
        }

        out_r[y * out_r_stride + x] = (Pixel) (clip_f_u16(yv + coeffs.r_cr * cr, fullRange));
        out_g[y * out_g_stride + x] = (Pixel) (clip_f_u16(yv + coeffs.g_cb * cb + coeffs.g_cr * cr, fullRange));
        out_b[y * out_b_stride + x] = (Pixel) (clip_f_u16(yv + coeffs.b_cb * cb, fullRange));
      }
    }

    if (has_alpha) {
      int copyWidth = (hdr ? width * 2 : width);
      memcpy(&out_a[y * out_a_stride], &in_a[y * in_a_stride], copyWidth);
    }
  }

  return outimg;
}


template<class Pixel>
class Op_RGB_to_YCbCr : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


template<class Pixel>
std::vector<ColorStateWithCost>
Op_RGB_to_YCbCr<Pixel>::state_after_conversion(const ColorState& input_state,
                                               const ColorState& target_state,
                                               const ColorConversionOptions& options)
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  if ((input_state.bits_per_pixel != 8) != hdr) {
    return {};
  }

  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to YCbCr

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = target_state.chroma;
  output_state.has_alpha = input_state.has_alpha;  // we simply keep the old alpha plane
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.75f, 0.5f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


template<class Pixel>
std::shared_ptr<HeifPixelImage>
Op_RGB_to_YCbCr<Pixel>::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                           const ColorState& target_state,
                                           const ColorConversionOptions& options)
{
  bool hdr = !std::is_same<Pixel, uint8_t>::value;

  int width = input->get_width();
  int height = input->get_height();

  heif_chroma chroma = target_state.chroma;
  int subH = chroma_h_subsampling(chroma);
  int subV = chroma_v_subsampling(chroma);

  int bpp = input->get_bits_per_pixel(heif_channel_R);
  if ((bpp != 8) != hdr) {
    return nullptr;
  }

  bool has_alpha = input->has_channel(heif_channel_Alpha);

  if (has_alpha && input->get_bits_per_pixel(heif_channel_Alpha) != bpp) {
    return nullptr;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_YCbCr, chroma);

  int cwidth = (width + subH - 1) / subH;
  int cheight = (height + subV - 1) / subV;

  outimg->add_plane(heif_channel_Y, width, height, bpp);
  outimg->add_plane(heif_channel_Cb, cwidth, cheight, bpp);
  outimg->add_plane(heif_channel_Cr, cwidth, cheight, bpp);

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, bpp);
  }

  const Pixel* in_r, * in_g, * in_b, * in_a;
  int in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  Pixel* out_y, * out_cb, * out_cr, * out_a;
  int out_y_stride = 0, out_cb_stride = 0, out_cr_stride = 0, out_a_stride = 0;

  in_r = (const Pixel*) input->get_plane(heif_channel_R, &in_r_stride);
  in_g = (const Pixel*) input->get_plane(heif_channel_G, &in_g_stride);
  in_b = (const Pixel*) input->get_plane(heif_channel_B, &in_b_stride);
  out_y = (Pixel*) outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = (Pixel*) outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = (Pixel*) outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (has_alpha) {
    in_a = (const Pixel*) input->get_plane(heif_channel_Alpha, &in_a_stride);
    out_a = (Pixel*) outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }
  else {
    in_a = nullptr;
    out_a = nullptr;
  }

  if (hdr) {
    in_r_stride /= 2;
    in_g_stride /= 2;
    in_b_stride /= 2;
    in_a_stride /= 2;
    out_y_stride /= 2;
    out_cb_stride /= 2;
    out_cr_stride /= 2;
    out_a_stride /= 2;
  }

  uint16_t halfRange = (uint16_t) (1 << (bpp - 1));
  int32_t fullRange = (1 << bpp) - 1;

  int matrix_coeffs = 2;
  RGB_to_YCbCr_coefficients coeffs = RGB_to_YCbCr_coefficients::defaults();
  bool full_range_flag = true;
  if (target_state.nclx_profile) {
    full_range_flag = target_state.nclx_profile->get_full_range_flag();
    matrix_coeffs = target_state.nclx_profile->get_matrix_coefficients();
    coeffs = heif::get_RGB_to_YCbCr_coefficients(target_state.nclx_profile->get_matrix_coefficients(),
                                                 target_state.nclx_profile->get_colour_primaries());
  }

  int x, y;

  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      if (matrix_coeffs == 0) {
        if (full_range_flag) {
          out_y[y * out_y_stride + x] = in_g[y * in_g_stride + x];
        }
        else {
          float v = (((in_g[y * in_g_stride + x] * 219.0f) / 256) + 16);
          out_y[y * out_y_stride + x] = (Pixel) clip_f_u16(v, fullRange);
        }
      }
      else {
        float r = in_r[y * in_r_stride + x];
        float g = in_g[y * in_g_stride + x];
        float b = in_b[y * in_b_stride + x];

        float v = r * coeffs.c[0][0] + g * coeffs.c[0][1] + b * coeffs.c[0][2];
        if (!full_range_flag) {
          v = (((v * 219) / 256) + 16);
        }

        Pixel pix = (Pixel) clip_f_u16(v, fullRange);

        out_y[y * out_y_stride + x] = pix;
      }
    }
  }

  for (y = 0; y < height; y += subV) {
    for (x = 0; x < width; x += subH) {
      if (matrix_coeffs == 0) {
        if (full_range_flag) {
          out_cb[(y / subV) * out_cb_stride + (x / subH)] = in_b[y * in_b_stride + x];
          out_cr[(y / subV) * out_cb_stride + (x / subH)] = in_r[y * in_b_stride + x];
        }
        else {
          out_cb[(y / subV) * out_cb_stride + (x / subH)] = (Pixel) clip_f_u16(
              ((in_b[y * in_b_stride + x] * 219.0f) / 256) + 16, fullRange);
          out_cr[(y / subV) * out_cb_stride + (x / subH)] = (Pixel) clip_f_u16(
              ((in_r[y * in_b_stride + x] * 219.0f) / 256) + 16, fullRange);
        }
      }
      else {
        float r = in_r[y * in_r_stride + x];
        float g = in_g[y * in_g_stride + x];
        float b = in_b[y * in_b_stride + x];

        if (subH > 1 || subV > 1) {
          int x2 = (x + 1 < width && subH == 2 && subV == 2) ? x + 1 : x;  // subV==2 -> Do not center for 4:2:2 (see comment in Op_RGB24_32_to_YCbCr, github issue #521)
          int y2 = (y + 1 < height && subV == 2) ? y + 1 : y;

          r += in_r[y * in_r_stride + x2];
          g += in_g[y * in_g_stride + x2];
          b += in_b[y * in_b_stride + x2];

          r += in_r[y2 * in_r_stride + x];
          g += in_g[y2 * in_g_stride + x];
          b += in_b[y2 * in_b_stride + x];

          r += in_r[y2 * in_r_stride + x2];
          g += in_g[y2 * in_g_stride + x2];
          b += in_b[y2 * in_b_stride + x2];

          r *= 0.25f;
          g *= 0.25f;
          b *= 0.25f;
        }

        float cb, cr;

        cb = r * coeffs.c[1][0] + g * coeffs.c[1][1] + b * coeffs.c[1][2];
        cr = r * coeffs.c[2][0] + g * coeffs.c[2][1] + b * coeffs.c[2][2];

        if (!full_range_flag) {
          cb = (cb * 224) / 256;
          cr = (cr * 224) / 256;
        }

        out_cb[(y / subV) * out_cb_stride + (x / subH)] = (Pixel) clip_f_u16(cb + halfRange, fullRange);
        out_cr[(y / subV) * out_cr_stride + (x / subH)] = (Pixel) clip_f_u16(cr + halfRange, fullRange);
      }
    }
  }


  if (has_alpha) {
    int copyWidth = (hdr ? width * 2 : width);
    for (y = 0; y < height; y++) {
      memcpy(&out_a[y * out_a_stride], &in_a[y * in_a_stride], copyWidth);
    }
  }

  return outimg;
}


class Op_YCbCr420_to_RGB24 : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_YCbCr420_to_RGB24::state_after_conversion(const ColorState& input_state,
                                             const ColorState& target_state,
                                             const ColorConversionOptions& options)
{
  if (input_state.colorspace != heif_colorspace_YCbCr ||
      input_state.chroma != heif_chroma_420 ||
      input_state.bits_per_pixel != 8 ||
      input_state.has_alpha == true) {
    return {};
  }

  if (input_state.nclx_profile) {
    int matrix = input_state.nclx_profile->get_matrix_coefficients();
    if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
      return {};
    }
    if (!input_state.nclx_profile->get_full_range_flag()) {
      return {};
    }
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RGB

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGB;
  output_state.has_alpha = false;
  output_state.bits_per_pixel = 8;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_YCbCr420_to_RGB24::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                         const ColorState& target_state,
                                         const ColorConversionOptions& options)
{
  if (input->get_bits_per_pixel(heif_channel_Y) != 8 ||
      input->get_bits_per_pixel(heif_channel_Cb) != 8 ||
      input->get_bits_per_pixel(heif_channel_Cr) != 8) {
    return nullptr;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_24bit);

  outimg->add_plane(heif_channel_interleaved, width, height, 8);

  auto colorProfile = input->get_color_profile_nclx();
  YCbCr_to_RGB_coefficients coeffs = YCbCr_to_RGB_coefficients::defaults();
  if (colorProfile) {
    coeffs = heif::get_YCbCr_to_RGB_coefficients(colorProfile->get_matrix_coefficients(),
                                                 colorProfile->get_colour_primaries());
  }

  int r_cr = static_cast<int>(std::lround(256 * coeffs.r_cr));
  int g_cr = static_cast<int>(std::lround(256 * coeffs.g_cr));
  int g_cb = static_cast<int>(std::lround(256 * coeffs.g_cb));
  int b_cb = static_cast<int>(std::lround(256 * coeffs.b_cb));

  const uint8_t* in_y, * in_cb, * in_cr;
  int in_y_stride = 0, in_cb_stride = 0, in_cr_stride = 0;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_y = input->get_plane(heif_channel_Y, &in_y_stride);
  in_cb = input->get_plane(heif_channel_Cb, &in_cb_stride);
  in_cr = input->get_plane(heif_channel_Cr, &in_cr_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  int x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {
      int yv = (in_y[y * in_y_stride + x]);
      int cb = (in_cb[y / 2 * in_cb_stride + x / 2] - 128);
      int cr = (in_cr[y / 2 * in_cr_stride + x / 2] - 128);

      out_p[y * out_p_stride + 3 * x + 0] = clip_int_u8(yv + ((r_cr * cr + 128) >> 8));
      out_p[y * out_p_stride + 3 * x + 1] = clip_int_u8(yv + ((g_cb * cb + g_cr * cr + 128) >> 8));
      out_p[y * out_p_stride + 3 * x + 2] = clip_int_u8(yv + ((b_cb * cb + 128) >> 8));
    }
  }

  return outimg;
}


class Op_YCbCr420_to_RGB32 : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_YCbCr420_to_RGB32::state_after_conversion(const ColorState& input_state,
                                             const ColorState& target_state,
                                             const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_YCbCr ||
      input_state.chroma != heif_chroma_420 ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  if (input_state.nclx_profile) {
    int matrix = input_state.nclx_profile->get_matrix_coefficients();
    if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
      return {};
    }
    if (!input_state.nclx_profile->get_full_range_flag()) {
      return {};
    }
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RGB

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGBA;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = 8;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_YCbCr420_to_RGB32::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                         const ColorState& target_state,
                                         const ColorConversionOptions& options)
{
  if (input->get_bits_per_pixel(heif_channel_Y) != 8 ||
      input->get_bits_per_pixel(heif_channel_Cb) != 8 ||
      input->get_bits_per_pixel(heif_channel_Cr) != 8) {
    return nullptr;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_32bit);

  outimg->add_plane(heif_channel_interleaved, width, height, 8);


  // --- get conversion coefficients

  auto colorProfile = input->get_color_profile_nclx();
  YCbCr_to_RGB_coefficients coeffs = YCbCr_to_RGB_coefficients::defaults();
  if (colorProfile) {
    coeffs = heif::get_YCbCr_to_RGB_coefficients(colorProfile->get_matrix_coefficients(),
                                                 colorProfile->get_colour_primaries());
  }

  int r_cr = static_cast<int>(std::lround(256 * coeffs.r_cr));
  int g_cr = static_cast<int>(std::lround(256 * coeffs.g_cr));
  int g_cb = static_cast<int>(std::lround(256 * coeffs.g_cb));
  int b_cb = static_cast<int>(std::lround(256 * coeffs.b_cb));


  const bool with_alpha = input->has_channel(heif_channel_Alpha);

  const uint8_t* in_y, * in_cb, * in_cr, * in_a = nullptr;
  int in_y_stride = 0, in_cb_stride = 0, in_cr_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_y = input->get_plane(heif_channel_Y, &in_y_stride);
  in_cb = input->get_plane(heif_channel_Cb, &in_cb_stride);
  in_cr = input->get_plane(heif_channel_Cr, &in_cr_stride);
  if (with_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  int x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < width; x++) {

      int yv = (in_y[y * in_y_stride + x]);
      int cb = (in_cb[y / 2 * in_cb_stride + x / 2] - 128);
      int cr = (in_cr[y / 2 * in_cr_stride + x / 2] - 128);

      out_p[y * out_p_stride + 4 * x + 0] = clip_int_u8(yv + ((r_cr * cr + 128) >> 8));
      out_p[y * out_p_stride + 4 * x + 1] = clip_int_u8(yv + ((g_cb * cb + g_cr * cr + 128) >> 8));
      out_p[y * out_p_stride + 4 * x + 2] = clip_int_u8(yv + ((b_cb * cb + 128) >> 8));


      if (with_alpha) {
        out_p[y * out_p_stride + 4 * x + 3] = in_a[y * in_a_stride + x];
      }
      else {
        out_p[y * out_p_stride + 4 * x + 3] = 0xFF;
      }
    }
  }

  return outimg;
}


class Op_RGB_HDR_to_RRGGBBaa_BE : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RGB_HDR_to_RRGGBBaa_BE::state_after_conversion(const ColorState& input_state,
                                                  const ColorState& target_state,
                                                  const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444 ||
      input_state.bits_per_pixel == 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RRGGBB_BE

  if (input_state.has_alpha == false) {
    output_state.colorspace = heif_colorspace_RGB;
    output_state.chroma = heif_chroma_interleaved_RRGGBB_BE;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    costs = {0.5f, 0.0f, 0.0f};

    states.push_back({output_state, costs});
  }


  // --- convert to RRGGBBAA_BE

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RRGGBBAA_BE;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});


  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RGB_HDR_to_RRGGBBaa_BE::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                              const ColorState& target_state,
                                              const ColorConversionOptions& options)
{
  if (input->get_bits_per_pixel(heif_channel_R) == 8 ||
      input->get_bits_per_pixel(heif_channel_G) == 8 ||
      input->get_bits_per_pixel(heif_channel_B) == 8) {
    return nullptr;
  }

  //int bpp = input->get_bits_per_pixel(heif_channel_R);

  bool input_has_alpha = input->has_channel(heif_channel_Alpha);
  bool output_has_alpha = input_has_alpha || target_state.has_alpha;

  if (input_has_alpha) {
    if (input->get_bits_per_pixel(heif_channel_Alpha) == 8) {
      return nullptr;
    }

    if (input->get_width(heif_channel_Alpha) != input->get_width(heif_channel_G) ||
        input->get_height(heif_channel_Alpha) != input->get_height(heif_channel_G)) {
      return nullptr;
    }
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB,
                 output_has_alpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE);

  outimg->add_plane(heif_channel_interleaved, width, height, input->get_bits_per_pixel(heif_channel_R));

  const uint16_t* in_r, * in_g, * in_b, * in_a = nullptr;
  int in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_r = (uint16_t*) input->get_plane(heif_channel_R, &in_r_stride);
  in_g = (uint16_t*) input->get_plane(heif_channel_G, &in_g_stride);
  in_b = (uint16_t*) input->get_plane(heif_channel_B, &in_b_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  if (input_has_alpha) {
    in_a = (uint16_t*) input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  in_r_stride /= 2;
  in_g_stride /= 2;
  in_b_stride /= 2;
  in_a_stride /= 2;

  const int pixelsize = (output_has_alpha ? 8 : 6);

  int x, y;
  for (y = 0; y < height; y++) {

    if (input_has_alpha) {
      for (x = 0; x < width; x++) {
        uint16_t r = in_r[x + y * in_r_stride];
        uint16_t g = in_g[x + y * in_g_stride];
        uint16_t b = in_b[x + y * in_b_stride];
        uint16_t a = in_a[x + y * in_a_stride];
        out_p[y * out_p_stride + 8 * x + 0] = (uint8_t) (r >> 8);
        out_p[y * out_p_stride + 8 * x + 1] = (uint8_t) (r & 0xFF);
        out_p[y * out_p_stride + 8 * x + 2] = (uint8_t) (g >> 8);
        out_p[y * out_p_stride + 8 * x + 3] = (uint8_t) (g & 0xFF);
        out_p[y * out_p_stride + 8 * x + 4] = (uint8_t) (b >> 8);
        out_p[y * out_p_stride + 8 * x + 5] = (uint8_t) (b & 0xFF);
        out_p[y * out_p_stride + 8 * x + 6] = (uint8_t) (a >> 8);
        out_p[y * out_p_stride + 8 * x + 7] = (uint8_t) (a & 0xFF);
      }
    }
    else {
      for (x = 0; x < width; x++) {
        uint16_t r = in_r[x + y * in_r_stride];
        uint16_t g = in_g[x + y * in_g_stride];
        uint16_t b = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + pixelsize * x + 0] = (uint8_t) (r >> 8);
        out_p[y * out_p_stride + pixelsize * x + 1] = (uint8_t) (r & 0xFF);
        out_p[y * out_p_stride + pixelsize * x + 2] = (uint8_t) (g >> 8);
        out_p[y * out_p_stride + pixelsize * x + 3] = (uint8_t) (g & 0xFF);
        out_p[y * out_p_stride + pixelsize * x + 4] = (uint8_t) (b >> 8);
        out_p[y * out_p_stride + pixelsize * x + 5] = (uint8_t) (b & 0xFF);

        if (output_has_alpha) {
          out_p[y * out_p_stride + pixelsize * x + 6] = 0xFF;
          out_p[y * out_p_stride + pixelsize * x + 7] = 0xFF;
        }
      }
    }
  }

  return outimg;
}


class Op_RGB_to_RRGGBBaa_BE : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RGB_to_RRGGBBaa_BE::state_after_conversion(const ColorState& input_state,
                                              const ColorState& target_state,
                                              const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      input_state.chroma != heif_chroma_444 ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RRGGBB_BE

  if (input_state.has_alpha == false) {
    output_state.colorspace = heif_colorspace_RGB;
    output_state.chroma = heif_chroma_interleaved_RRGGBB_BE;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    costs = {0.5f, 0.0f, 0.0f};

    states.push_back({output_state, costs});
  }


  // --- convert to RRGGBBAA_BE

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RRGGBBAA_BE;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});


  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RGB_to_RRGGBBaa_BE::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                          const ColorState& target_state,
                                          const ColorConversionOptions& options)
{
  if (input->get_bits_per_pixel(heif_channel_R) != 8 ||
      input->get_bits_per_pixel(heif_channel_G) != 8 ||
      input->get_bits_per_pixel(heif_channel_B) != 8) {
    return nullptr;
  }

  //int bpp = input->get_bits_per_pixel(heif_channel_R);

  bool input_has_alpha = input->has_channel(heif_channel_Alpha);
  bool output_has_alpha = input_has_alpha || target_state.has_alpha;

  if (input_has_alpha && input->get_bits_per_pixel(heif_channel_Alpha) != 8) {
    return nullptr;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB,
                 output_has_alpha ? heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE);

  outimg->add_plane(heif_channel_interleaved, width, height, input->get_bits_per_pixel(heif_channel_R));

  const uint8_t* in_r, * in_g, * in_b, * in_a = nullptr;
  int in_r_stride = 0, in_g_stride = 0, in_b_stride = 0, in_a_stride = 0;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_r = input->get_plane(heif_channel_R, &in_r_stride);
  in_g = input->get_plane(heif_channel_G, &in_g_stride);
  in_b = input->get_plane(heif_channel_B, &in_b_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  if (input_has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  const int pixelsize = (output_has_alpha ? 8 : 6);

  int x, y;
  for (y = 0; y < height; y++) {

    if (input_has_alpha) {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + 8 * x + 0] = 0;
        out_p[y * out_p_stride + 8 * x + 1] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + 8 * x + 2] = 0;
        out_p[y * out_p_stride + 8 * x + 3] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + 8 * x + 4] = 0;
        out_p[y * out_p_stride + 8 * x + 5] = in_b[x + y * in_b_stride];
        out_p[y * out_p_stride + 8 * x + 6] = 0;
        out_p[y * out_p_stride + 8 * x + 7] = in_a[x + y * in_a_stride];
      }
    }
    else {
      for (x = 0; x < width; x++) {
        out_p[y * out_p_stride + pixelsize * x + 0] = 0;
        out_p[y * out_p_stride + pixelsize * x + 1] = in_r[x + y * in_r_stride];
        out_p[y * out_p_stride + pixelsize * x + 2] = 0;
        out_p[y * out_p_stride + pixelsize * x + 3] = in_g[x + y * in_g_stride];
        out_p[y * out_p_stride + pixelsize * x + 4] = 0;
        out_p[y * out_p_stride + pixelsize * x + 5] = in_b[x + y * in_b_stride];

        if (output_has_alpha) {
          out_p[y * out_p_stride + pixelsize * x + 6] = 0;
          out_p[y * out_p_stride + pixelsize * x + 7] = 0xFF;
        }
      }
    }
  }

  return outimg;
}


class Op_RRGGBBaa_BE_to_RGB_HDR : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RRGGBBaa_BE_to_RGB_HDR::state_after_conversion(const ColorState& input_state,
                                                  const ColorState& target_state,
                                                  const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RRGGBB_BE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBBAA_BE) ||
      input_state.bits_per_pixel == 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RRGGBB_BE

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_444;
  output_state.has_alpha = (input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE ||
                            input_state.chroma == heif_chroma_interleaved_RRGGBBAA_BE);
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.2f, 0.0f, 0.0f};

  states.push_back({output_state, costs});


  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RRGGBBaa_BE_to_RGB_HDR::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                              const ColorState& target_state,
                                              const ColorConversionOptions& options)
{
  bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE ||
                    input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE);

  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_RGB, heif_chroma_444);

  outimg->add_plane(heif_channel_R, width, height, input->get_bits_per_pixel(heif_channel_interleaved));
  outimg->add_plane(heif_channel_G, width, height, input->get_bits_per_pixel(heif_channel_interleaved));
  outimg->add_plane(heif_channel_B, width, height, input->get_bits_per_pixel(heif_channel_interleaved));

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, input->get_bits_per_pixel(heif_channel_interleaved));
  }

  const uint8_t* in_p;
  int in_p_stride = 0;
  int in_pix_size = has_alpha ? 8 : 6;

  uint16_t* out_r, * out_g, * out_b, * out_a = nullptr;
  int out_r_stride = 0, out_g_stride = 0, out_b_stride = 0, out_a_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);

  out_r = (uint16_t*) outimg->get_plane(heif_channel_R, &out_r_stride);
  out_g = (uint16_t*) outimg->get_plane(heif_channel_G, &out_g_stride);
  out_b = (uint16_t*) outimg->get_plane(heif_channel_B, &out_b_stride);

  if (has_alpha) {
    out_a = (uint16_t*) outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }

  out_r_stride /= 2;
  out_g_stride /= 2;
  out_b_stride /= 2;
  out_a_stride /= 2;

  int x, y;
  for (y = 0; y < height; y++) {

    for (x = 0; x < width; x++) {
      uint16_t r = (uint16_t) ((in_p[y * in_p_stride + in_pix_size * x + 0] << 8) |
                               in_p[y * in_p_stride + in_pix_size * x + 1]);
      uint16_t g = (uint16_t) ((in_p[y * in_p_stride + in_pix_size * x + 2] << 8) |
                               in_p[y * in_p_stride + in_pix_size * x + 3]);
      uint16_t b = (uint16_t) ((in_p[y * in_p_stride + in_pix_size * x + 4] << 8) |
                               in_p[y * in_p_stride + in_pix_size * x + 5]);

      out_r[x + y * out_r_stride] = r;
      out_g[x + y * out_g_stride] = g;
      out_b[x + y * out_b_stride] = b;

      if (has_alpha) {
        // in_pix_size is always 8 when we have alpha channel
        uint16_t a = (uint16_t) ((in_p[y * in_p_stride + 8 * x + 6] << 8) |
                                 in_p[y * in_p_stride + 8 * x + 7]);

        out_a[x + y * out_a_stride] = a;
      }
    }
  }

  return outimg;
}


class Op_RRGGBBaa_swap_endianness : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RRGGBBaa_swap_endianness::state_after_conversion(const ColorState& input_state,
                                                    const ColorState& target_state,
                                                    const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RRGGBB_LE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBB_BE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBBAA_LE &&
       input_state.chroma != heif_chroma_interleaved_RRGGBBAA_BE)) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- swap RRGGBB

  if (input_state.chroma == heif_chroma_interleaved_RRGGBB_LE ||
      input_state.chroma == heif_chroma_interleaved_RRGGBB_BE) {
    output_state.colorspace = heif_colorspace_RGB;

    if (input_state.chroma == heif_chroma_interleaved_RRGGBB_LE) {
      output_state.chroma = heif_chroma_interleaved_RRGGBB_BE;
    }
    else {
      output_state.chroma = heif_chroma_interleaved_RRGGBB_LE;
    }

    output_state.has_alpha = false;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    costs = {0.1f, 0.0f, 0.0f};

    states.push_back({output_state, costs});
  }


  // --- swap RRGGBBAA

  if (input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE ||
      input_state.chroma == heif_chroma_interleaved_RRGGBBAA_BE) {
    output_state.colorspace = heif_colorspace_RGB;

    if (input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE) {
      output_state.chroma = heif_chroma_interleaved_RRGGBBAA_BE;
    }
    else {
      output_state.chroma = heif_chroma_interleaved_RRGGBBAA_LE;
    }

    output_state.has_alpha = true;
    output_state.bits_per_pixel = input_state.bits_per_pixel;

    costs = {0.1f, 0.0f, 0.0f};

    states.push_back({output_state, costs});
  }


  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RRGGBBaa_swap_endianness::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                const ColorState& target_state,
                                                const ColorConversionOptions& options)
{
  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  switch (input->get_chroma_format()) {
    case heif_chroma_interleaved_RRGGBB_LE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_BE);
      break;
    case heif_chroma_interleaved_RRGGBB_BE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBB_LE);
      break;
    case heif_chroma_interleaved_RRGGBBAA_LE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_BE);
      break;
    case heif_chroma_interleaved_RRGGBBAA_BE:
      outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_RRGGBBAA_LE);
      break;
    default:
      return nullptr;
  }

  outimg->add_plane(heif_channel_interleaved, width, height,
                    input->get_bits_per_pixel(heif_channel_interleaved));

  const uint8_t* in_p = nullptr;
  int in_p_stride = 0;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);
  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  int n_bytes = std::min(in_p_stride, out_p_stride);

  int x, y;
  for (y = 0; y < height; y++) {
    for (x = 0; x < n_bytes; x += 2) {
      out_p[y * out_p_stride + x + 0] = in_p[y * in_p_stride + x + 1];
      out_p[y * out_p_stride + x + 1] = in_p[y * in_p_stride + x + 0];
    }
  }

  return outimg;
}


class Op_mono_to_YCbCr420 : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_mono_to_YCbCr420::state_after_conversion(const ColorState& input_state,
                                            const ColorState& target_state,
                                            const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_monochrome ||
      input_state.chroma != heif_chroma_monochrome) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to YCbCr420

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = heif_chroma_420;
  output_state.has_alpha = input_state.has_alpha;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.1f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_mono_to_YCbCr420::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                        const ColorState& target_state,
                                        const ColorConversionOptions& options)
{
  auto outimg = std::make_shared<HeifPixelImage>();

  int width = input->get_width();
  int height = input->get_height();

  outimg->create(width, height, heif_colorspace_YCbCr, heif_chroma_420);

  int input_bpp = input->get_bits_per_pixel(heif_channel_Y);

  int chroma_width = (width + 1) / 2;
  int chroma_height = (height + 1) / 2;

  outimg->add_plane(heif_channel_Y, width, height, input_bpp);
  outimg->add_plane(heif_channel_Cb, chroma_width, chroma_height, input_bpp);
  outimg->add_plane(heif_channel_Cr, chroma_width, chroma_height, input_bpp);

  int alpha_bpp = 0;
  bool has_alpha = input->has_channel(heif_channel_Alpha);
  if (has_alpha) {
    alpha_bpp = input->get_bits_per_pixel(heif_channel_Alpha);
    outimg->add_plane(heif_channel_Alpha, width, height, alpha_bpp);
  }


  if (input_bpp == 8) {
    uint8_t* out_cb, * out_cr, * out_y;
    int out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0;

    const uint8_t* in_y;
    int in_y_stride = 0;

    in_y = input->get_plane(heif_channel_Y, &in_y_stride);

    out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
    out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
    out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

    memset(out_cb, 128, out_cb_stride * chroma_height);
    memset(out_cr, 128, out_cr_stride * chroma_height);

    for (int y = 0; y < height; y++) {
      memcpy(out_y + y * out_y_stride,
             in_y + y * in_y_stride,
             width);
    }
  }
  else {
    uint16_t* out_cb, * out_cr, * out_y;
    int out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0;

    const uint16_t* in_y;
    int in_y_stride = 0;

    in_y = (const uint16_t*) input->get_plane(heif_channel_Y, &in_y_stride);

    out_y = (uint16_t*) outimg->get_plane(heif_channel_Y, &out_y_stride);
    out_cb = (uint16_t*) outimg->get_plane(heif_channel_Cb, &out_cb_stride);
    out_cr = (uint16_t*) outimg->get_plane(heif_channel_Cr, &out_cr_stride);

    in_y_stride /= 2;
    out_y_stride /= 2;
    out_cb_stride /= 2;
    out_cr_stride /= 2;

    for (int y = 0; y < chroma_height; y++)
      for (int x = 0; x < chroma_width; x++) {
        out_cb[x + y * out_cb_stride] = (uint16_t) (128 << (input_bpp - 8));
        out_cr[x + y * out_cr_stride] = (uint16_t) (128 << (input_bpp - 8));
      }

    for (int y = 0; y < height; y++) {
      memcpy(out_y + y * out_y_stride,
             in_y + y * in_y_stride,
             width * 2);
    }
  }

  if (has_alpha) {
    const uint8_t* in_a;
    uint8_t* out_a;
    int in_a_stride = 0;
    int out_a_stride = 0;

    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);

    int memory_width = (alpha_bpp > 8 ? width * 2 : width);

    for (int y = 0; y < height; y++) {
      memcpy(&out_a[y * out_a_stride], &in_a[y * in_a_stride], memory_width);
    }
  }

  return outimg;
}


class Op_mono_to_RGB24_32 : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_mono_to_RGB24_32::state_after_conversion(const ColorState& input_state,
                                            const ColorState& target_state,
                                            const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if ((input_state.colorspace != heif_colorspace_monochrome &&
       input_state.colorspace != heif_colorspace_YCbCr) ||
      input_state.chroma != heif_chroma_monochrome ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to RGB24

  if (input_state.has_alpha == false) {
    output_state.colorspace = heif_colorspace_RGB;
    output_state.chroma = heif_chroma_interleaved_RGB;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = 8;

    costs = {0.1f, 0.0f, 0.0f};

    states.push_back({output_state, costs});
  }


  // --- convert to RGB32

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = heif_chroma_interleaved_RGBA;
  output_state.has_alpha = true;
  output_state.bits_per_pixel = 8;

  costs = {0.15f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_mono_to_RGB24_32::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                        const ColorState& target_state,
                                        const ColorConversionOptions& options)
{
  int width = input->get_width();
  int height = input->get_height();

  if (input->get_bits_per_pixel(heif_channel_Y) != 8) {
    return nullptr;
  }

  auto outimg = std::make_shared<HeifPixelImage>();

  bool has_alpha = input->has_channel(heif_channel_Alpha);

  if (target_state.has_alpha) {
    outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_32bit);
  }
  else {
    outimg->create(width, height, heif_colorspace_RGB, heif_chroma_interleaved_24bit);
  }

  outimg->add_plane(heif_channel_interleaved, width, height, 8);

  const uint8_t* in_y, * in_a;
  int in_y_stride = 0, in_a_stride;

  uint8_t* out_p;
  int out_p_stride = 0;

  in_y = input->get_plane(heif_channel_Y, &in_y_stride);
  if (has_alpha) {
    in_a = input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);

  int x, y;
  for (y = 0; y < height; y++) {
    if (target_state.has_alpha == false) {
      for (x = 0; x < width; x++) {
        uint8_t v = in_y[x + y * in_y_stride];
        out_p[y * out_p_stride + 3 * x + 0] = v;
        out_p[y * out_p_stride + 3 * x + 1] = v;
        out_p[y * out_p_stride + 3 * x + 2] = v;
      }
    }
    else if (has_alpha) {
      for (x = 0; x < width; x++) {
        uint8_t v = in_y[x + y * in_y_stride];
        out_p[y * out_p_stride + 4 * x + 0] = v;
        out_p[y * out_p_stride + 4 * x + 1] = v;
        out_p[y * out_p_stride + 4 * x + 2] = v;
        out_p[y * out_p_stride + 4 * x + 3] = in_a[x + y * in_a_stride];
      }
    }
    else {
      for (x = 0; x < width; x++) {
        uint8_t v = in_y[x + y * in_y_stride];
        out_p[y * out_p_stride + 4 * x + 0] = v;
        out_p[y * out_p_stride + 4 * x + 1] = v;
        out_p[y * out_p_stride + 4 * x + 2] = v;
        out_p[y * out_p_stride + 4 * x + 3] = 0xFF;
      }
    }
  }

  return outimg;
}


class Op_RGB24_32_to_YCbCr : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RGB24_32_to_YCbCr::state_after_conversion(const ColorState& input_state,
                                             const ColorState& target_state,
                                             const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RGB &&
       input_state.chroma != heif_chroma_interleaved_RGBA)) {
    return {};
  }

  if (target_state.chroma != heif_chroma_420 &&
      target_state.chroma != heif_chroma_422 &&
      target_state.chroma != heif_chroma_444) {
    return {};
  }

  if (target_state.nclx_profile) {
    if (target_state.nclx_profile->get_matrix_coefficients() == 0) {
      return {};
    }
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;


  // --- convert RGB24

  if (input_state.chroma == heif_chroma_interleaved_RGB) {
    output_state.colorspace = heif_colorspace_YCbCr;
    output_state.chroma = target_state.chroma;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = 8;

    costs = {0.75f, 0.5f, 0.0f};  // quality not good since we subsample chroma without filtering

    states.push_back({output_state, costs});
  }


  // --- convert RGB32

  if (input_state.chroma == heif_chroma_interleaved_RGBA) {
    output_state.colorspace = heif_colorspace_YCbCr;
    output_state.chroma = target_state.chroma;
    output_state.has_alpha = true;
    output_state.bits_per_pixel = 8;

    costs = {0.75f, 0.5f, 0.0f};  // quality not good since we subsample chroma without filtering

    states.push_back({output_state, costs});
  }

  return states;
}


static inline uint8_t clip_f_u8(float fx)
{
  long x = (long int) (fx + 0.5f);
  if (x < 0) return 0;
  if (x > 255) return 255;
  return static_cast<uint8_t>(x);
}


inline void set_chroma_pixels(uint8_t* out_cb, uint8_t* out_cr,
                              uint8_t r, uint8_t g, uint8_t b,
                              const RGB_to_YCbCr_coefficients& coeffs,
                              bool full_range_flag)
{
  float cb = r * coeffs.c[1][0] + g * coeffs.c[1][1] + b * coeffs.c[1][2];
  float cr = r * coeffs.c[2][0] + g * coeffs.c[2][1] + b * coeffs.c[2][2];

  if (full_range_flag) {
    *out_cb = clip_f_u8(cb + 128);
    *out_cr = clip_f_u8(cr + 128);
  }
  else {
    *out_cb = (uint8_t) clip_f_u8(cb * 0.875f + 128.0f);
    *out_cr = (uint8_t) clip_f_u8(cr * 0.875f + 128.0f);
  }
}


std::shared_ptr<HeifPixelImage>
Op_RGB24_32_to_YCbCr::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                         const ColorState& target_state,
                                         const ColorConversionOptions& options)
{
  int width = input->get_width();
  int height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  auto chroma = target_state.chroma;
  uint8_t chromaSubH = chroma_h_subsampling(chroma);
  uint8_t chromaSubV = chroma_v_subsampling(chroma);

  outimg->create(width, height, heif_colorspace_YCbCr, chroma);

  int chroma_width = (width + chromaSubH - 1) / chromaSubH;
  int chroma_height = (height + chromaSubV - 1) / chromaSubV;

  const bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_32bit);

  outimg->add_plane(heif_channel_Y, width, height, 8);
  outimg->add_plane(heif_channel_Cb, chroma_width, chroma_height, 8);
  outimg->add_plane(heif_channel_Cr, chroma_width, chroma_height, 8);

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, 8);
  }

  uint8_t* out_cb, * out_cr, * out_y, * out_a;
  int out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0, out_a_stride = 0;

  const uint8_t* in_p;
  int in_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_stride);

  out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (has_alpha) {
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }
  else {
    out_a = nullptr;
  }


  RGB_to_YCbCr_coefficients coeffs = RGB_to_YCbCr_coefficients::defaults();
  bool full_range_flag = true;
  if (target_state.nclx_profile) {
    full_range_flag = target_state.nclx_profile->get_full_range_flag();
    coeffs = heif::get_RGB_to_YCbCr_coefficients(target_state.nclx_profile->get_matrix_coefficients(),
                                                 target_state.nclx_profile->get_colour_primaries());
  }


  int bytes_per_pixel = (has_alpha ? 4 : 3);

  for (int y = 0; y < height; y++) {
    const uint8_t* p = &in_p[y * in_stride];

    for (int x = 0; x < width; x++) {
      uint8_t r = p[0];
      uint8_t g = p[1];
      uint8_t b = p[2];
      p += bytes_per_pixel;

      float yv = r * coeffs.c[0][0] + g * coeffs.c[0][1] + b * coeffs.c[0][2];

      if (full_range_flag) {
        out_y[y * out_y_stride + x] = clip_f_u8(yv);
      }
      else {
        out_y[y * out_y_stride + x] = (uint8_t) (clip_f_u16(yv * 0.85547f, 219) + 16);
      }
    }
  }

  if (chromaSubH == 1 && chromaSubV == 1) {
    // chroma 4:4:4

    for (int y = 0; y < height; y++) {
      const uint8_t* p = &in_p[y * in_stride];

      for (int x = 0; x < width; x++) {
        uint8_t r = p[0];
        uint8_t g = p[1];
        uint8_t b = p[2];
        p += bytes_per_pixel;

        set_chroma_pixels(out_cb + y * out_cb_stride + x,
                          out_cr + y * out_cr_stride + x,
                          r, g, b,
                          coeffs, full_range_flag);
      }
    }
  }
  else if (chromaSubH == 2 && chromaSubV == 2) {
    // chroma 4:2:0

    for (int y = 0; y < (height & ~1); y += 2) {
      const uint8_t* p = &in_p[y * in_stride];

      for (int x = 0; x < (width & ~1); x += 2) {
        uint8_t r = uint8_t((p[0] + p[bytes_per_pixel + 0] + p[in_stride + 0] + p[bytes_per_pixel + in_stride + 0]) / 4);
        uint8_t g = uint8_t((p[1] + p[bytes_per_pixel + 1] + p[in_stride + 1] + p[bytes_per_pixel + in_stride + 1]) / 4);
        uint8_t b = uint8_t((p[2] + p[bytes_per_pixel + 2] + p[in_stride + 2] + p[bytes_per_pixel + in_stride + 2]) / 4);

        p += bytes_per_pixel * 2;

        set_chroma_pixels(out_cb + (y / 2) * out_cb_stride + (x / 2),
                          out_cr + (y / 2) * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);
      }
    }

    // 4:2:0 right column (if odd width)
    if (width & 1) {
      int x = width - 1;
      const uint8_t* p = &in_p[x * bytes_per_pixel];

      for (int y = 0; y < height; y += 2) {
        uint8_t r, g, b;
        if (y + 1 < height) {
          r = uint8_t((p[0] + p[in_stride + 0]) / 2);
          g = uint8_t((p[1] + p[in_stride + 1]) / 2);
          b = uint8_t((p[2] + p[in_stride + 2]) / 2);
        }
        else {
          r = p[0];
          g = p[1];
          b = p[2];
        }

        set_chroma_pixels(out_cb + (y / 2) * out_cb_stride + (x / 2),
                          out_cr + (y / 2) * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);

        p += in_stride * 2;
      }
    }

    // 4:2:0 bottom row (if odd height)
    if (height & 1) {
      int y = height - 1;
      const uint8_t* p = &in_p[y * in_stride];

      for (int x = 0; x < width; x += 2) {
        uint8_t r, g, b;
        if (x + 1 < width) {
          r = uint8_t((p[0] + p[bytes_per_pixel + 0]) / 2);
          g = uint8_t((p[1] + p[bytes_per_pixel + 1]) / 2);
          b = uint8_t((p[2] + p[bytes_per_pixel + 2]) / 2);
        }
        else {
          r = p[0];
          g = p[1];
          b = p[2];
        }

        set_chroma_pixels(out_cb + (y / 2) * out_cb_stride + (x / 2),
                          out_cr + (y / 2) * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);

        p += bytes_per_pixel * 2;
      }
    }
  }
  else if (chromaSubH == 2 && chromaSubV == 1) {
    // chroma 4:2:2

    for (int y = 0; y < height; y++) {
      const uint8_t* p = &in_p[y * in_stride];

      for (int x = 0; x < width; x += 2) {
        uint8_t r, g, b;

        // TODO: it still is an open question where the 'correct' chroma sample positions are for 4:2:2
        // Since 4:2:2 is primarily used for video content and as there is no way to signal center position for h.265,
        // we currently use left-aligned sampling. See the discussion here: https://github.com/strukturag/libheif/issues/521
#if USE_CENTER_CHROMA_422
        if (x + 1 < width) {
          r = uint8_t((p[0] + p[bytes_per_pixel + 0]) / 2);
          g = uint8_t((p[1] + p[bytes_per_pixel + 1]) / 2);
          b = uint8_t((p[2] + p[bytes_per_pixel + 2]) / 2);
        }
        else {
          r = p[0];
          g = p[1];
          b = p[2];
        }
#else
        r = p[0];
        g = p[1];
        b = p[2];
#endif

        p += bytes_per_pixel * 2;

        set_chroma_pixels(out_cb + y * out_cb_stride + (x / 2),
                          out_cr + y * out_cr_stride + (x / 2),
                          r, g, b,
                          coeffs, full_range_flag);
      }
    }
  }


  if (has_alpha) {
    assert(bytes_per_pixel == 4);

    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint8_t a = in_p[y * in_stride + x * 4 + 3];

        // alpha
        out_a[y * out_a_stride + x] = a;
      }
    }
  }

  return outimg;
}


class Op_RGB24_32_to_YCbCr444_GBR : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RGB24_32_to_YCbCr444_GBR::state_after_conversion(const ColorState& input_state,
                                                    const ColorState& target_state,
                                                    const ColorConversionOptions& options)
{
  // Note: no input alpha channel required. It will be filled up with 0xFF.

  if (input_state.colorspace != heif_colorspace_RGB ||
      (input_state.chroma != heif_chroma_interleaved_RGB &&
       input_state.chroma != heif_chroma_interleaved_RGBA)) {
    return {};
  }

  if (!target_state.nclx_profile) {
    return {};
  }

  if (target_state.nclx_profile->get_matrix_coefficients() != 0) {
    return {};
  }

  if (input_state.nclx_profile && !input_state.nclx_profile->get_full_range_flag()) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert RGB24

  if (input_state.chroma == heif_chroma_interleaved_RGB) {
    output_state.colorspace = heif_colorspace_YCbCr;
    output_state.chroma = heif_chroma_444;
    output_state.has_alpha = false;
    output_state.bits_per_pixel = 8;

    costs = {0.75f, 0.5f, 0.0f};  // quality not good since we subsample chroma without filtering

    states.push_back({output_state, costs});
  }


  // --- convert RGB32

  if (input_state.chroma == heif_chroma_interleaved_RGBA) {
    output_state.colorspace = heif_colorspace_YCbCr;
    output_state.chroma = heif_chroma_444;
    output_state.has_alpha = true;
    output_state.bits_per_pixel = 8;

    costs = {0.75f, 0.5f, 0.0f};  // quality not good since we subsample chroma without filtering

    states.push_back({output_state, costs});
  }

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RGB24_32_to_YCbCr444_GBR::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                const ColorState& target_state,
                                                const ColorConversionOptions& options)
{
  int width = input->get_width();
  int height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_YCbCr, heif_chroma_444);

  const bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_32bit);

  outimg->add_plane(heif_channel_Y, width, height, 8);
  outimg->add_plane(heif_channel_Cb, width, height, 8);
  outimg->add_plane(heif_channel_Cr, width, height, 8);

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, 8);
  }

  uint8_t* out_cb, * out_cr, * out_y, * out_a;
  int out_cb_stride = 0, out_cr_stride = 0, out_y_stride = 0, out_a_stride = 0;

  const uint8_t* in_p;
  int in_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_stride);

  out_y = outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (has_alpha) {
    out_a = outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }


  assert(target_state.nclx_profile);
  assert(target_state.nclx_profile->get_matrix_coefficients() == 0);


  if (!has_alpha) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint8_t r = in_p[y * in_stride + x * 3 + 0];
        uint8_t g = in_p[y * in_stride + x * 3 + 1];
        uint8_t b = in_p[y * in_stride + x * 3 + 2];

        out_y[y * out_y_stride + x] = g;
        out_cb[y * out_cb_stride + x] = b;
        out_cr[y * out_cr_stride + x] = r;
      }
    }
  }
  else {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        uint8_t r = in_p[y * in_stride + x * 4 + 0];
        uint8_t g = in_p[y * in_stride + x * 4 + 1];
        uint8_t b = in_p[y * in_stride + x * 4 + 2];
        uint8_t a = in_p[y * in_stride + x * 4 + 3];

        out_y[y * out_y_stride + x] = g;
        out_cb[y * out_cb_stride + x] = b;
        out_cr[y * out_cr_stride + x] = r;

        // alpha
        out_a[y * out_a_stride + x] = a;
      }
    }
  }

  return outimg;
}


class Op_drop_alpha_plane : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_drop_alpha_plane::state_after_conversion(const ColorState& input_state,
                                            const ColorState& target_state,
                                            const ColorConversionOptions& options)
{
  // only drop alpha plane if it is not needed in output

  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.has_alpha == false ||
      target_state.has_alpha == true) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- drop alpha plane

  output_state = input_state;
  output_state.has_alpha = false;

  costs = {0.1f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_drop_alpha_plane::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                        const ColorState& target_state,
                                        const ColorConversionOptions& options)
{
  int width = input->get_width();
  int height = input->get_height();

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height,
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_Y,
                               heif_channel_Cb,
                               heif_channel_Cr,
                               heif_channel_R,
                               heif_channel_G,
                               heif_channel_B}) {
    if (input->has_channel(channel)) {
      outimg->copy_new_plane_from(input, channel, channel);
    }
  }

  return outimg;
}


class Op_to_hdr_planes : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_to_hdr_planes::state_after_conversion(const ColorState& input_state,
                                         const ColorState& target_state,
                                         const ColorConversionOptions& options)
{
  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.bits_per_pixel != 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- increase bit depth

  output_state = input_state;
  output_state.bits_per_pixel = target_state.bits_per_pixel;

  costs = {0.2f, 0.0f, 0.5f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_to_hdr_planes::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                     const ColorState& target_state,
                                     const ColorConversionOptions& options)
{
  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(input->get_width(),
                 input->get_height(),
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_Y,
                               heif_channel_Cb,
                               heif_channel_Cr,
                               heif_channel_R,
                               heif_channel_G,
                               heif_channel_B,
                               heif_channel_Alpha}) {
    if (input->has_channel(channel)) {
      int width = input->get_width(channel);
      int height = input->get_height(channel);
      outimg->add_plane(channel, width, height, target_state.bits_per_pixel);

      int input_bits = input->get_bits_per_pixel(channel);
      int output_bits = target_state.bits_per_pixel;

      int shift1 = output_bits - input_bits;
      int shift2 = 8 - shift1;

      const uint8_t* p_in;
      int stride_in;
      p_in = input->get_plane(channel, &stride_in);

      uint16_t* p_out;
      int stride_out;
      p_out = (uint16_t*) outimg->get_plane(channel, &stride_out);
      stride_out /= 2;

      for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++) {
          int in = p_in[y * stride_in + x];
          p_out[y * stride_out + x] = (uint16_t) ((in << shift1) | (in >> shift2));
        }
    }
  }

  return outimg;
}


class Op_to_sdr_planes : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_to_sdr_planes::state_after_conversion(const ColorState& input_state,
                                         const ColorState& target_state,
                                         const ColorConversionOptions& options)
{
  if ((input_state.chroma != heif_chroma_monochrome &&
       input_state.chroma != heif_chroma_420 &&
       input_state.chroma != heif_chroma_422 &&
       input_state.chroma != heif_chroma_444) ||
      input_state.bits_per_pixel == 8) {
    return {};
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- decrease bit depth

  output_state = input_state;
  output_state.bits_per_pixel = 8;

  costs = {0.2f, 0.0f, 0.5f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_to_sdr_planes::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                     const ColorState& target_state,
                                     const ColorConversionOptions& options)
{

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(input->get_width(),
                 input->get_height(),
                 input->get_colorspace(),
                 input->get_chroma_format());

  for (heif_channel channel : {heif_channel_Y,
                               heif_channel_Cb,
                               heif_channel_Cr,
                               heif_channel_R,
                               heif_channel_G,
                               heif_channel_B,
                               heif_channel_Alpha}) {
    if (input->has_channel(channel)) {
      int input_bits = input->get_bits_per_pixel(channel);

      if (input_bits > 8) {
        int width = input->get_width(channel);
        int height = input->get_height(channel);
        outimg->add_plane(channel, width, height, 8);

        int shift = input_bits - 8;

        const uint16_t* p_in;
        int stride_in;
        p_in = (uint16_t*) input->get_plane(channel, &stride_in);
        stride_in /= 2;

        uint8_t* p_out;
        int stride_out;
        p_out = outimg->get_plane(channel, &stride_out);

        for (int y = 0; y < height; y++)
          for (int x = 0; x < width; x++) {
            int in = p_in[y * stride_in + x];
            p_out[y * stride_out + x] = (uint8_t) (in >> shift); // TODO: I think no rounding here, but am not sure.
          }
      }
      else {
        outimg->copy_new_plane_from(input, channel, channel);
      }
    }
  }

  return outimg;
}


class Op_RRGGBBxx_HDR_to_YCbCr420 : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_RRGGBBxx_HDR_to_YCbCr420::state_after_conversion(const ColorState& input_state,
                                                    const ColorState& target_state,
                                                    const ColorConversionOptions& options)
{
  if (input_state.colorspace != heif_colorspace_RGB ||
      !(input_state.chroma == heif_chroma_interleaved_RRGGBB_BE ||
        input_state.chroma == heif_chroma_interleaved_RRGGBB_LE ||
        input_state.chroma == heif_chroma_interleaved_RRGGBBAA_BE ||
        input_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE) ||
      input_state.bits_per_pixel == 8) {
    return {};
  }

  if (input_state.nclx_profile) {
    int matrix = input_state.nclx_profile->get_matrix_coefficients();
    if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
      return {};
    }
    if (!input_state.nclx_profile->get_full_range_flag()) {
      return {};
    }
  }


  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to YCbCr

  output_state.colorspace = heif_colorspace_YCbCr;
  output_state.chroma = heif_chroma_420;
  output_state.has_alpha = input_state.has_alpha;  // we generate an alpha plane if the source contains data
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_RRGGBBxx_HDR_to_YCbCr420::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                                const ColorState& target_state,
                                                const ColorConversionOptions& options)
{
  int width = input->get_width();
  int height = input->get_height();

  int bpp = input->get_bits_per_pixel(heif_channel_interleaved);

  bool has_alpha = (input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
                    input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE);

  auto outimg = std::make_shared<HeifPixelImage>();

  outimg->create(width, height, heif_colorspace_YCbCr, heif_chroma_420);

  int bytesPerPixel = has_alpha ? 8 : 6;

  int cwidth = (width + 1) / 2;
  int cheight = (height + 1) / 2;

  outimg->add_plane(heif_channel_Y, width, height, bpp);
  outimg->add_plane(heif_channel_Cb, cwidth, cheight, bpp);
  outimg->add_plane(heif_channel_Cr, cwidth, cheight, bpp);

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, bpp);
  }

  const uint8_t* in_p;
  int in_p_stride = 0;

  uint16_t* out_y, * out_cb, * out_cr, * out_a = nullptr;
  int out_y_stride = 0, out_cb_stride = 0, out_cr_stride = 0, out_a_stride = 0;

  in_p = input->get_plane(heif_channel_interleaved, &in_p_stride);
  out_y = (uint16_t*) outimg->get_plane(heif_channel_Y, &out_y_stride);
  out_cb = (uint16_t*) outimg->get_plane(heif_channel_Cb, &out_cb_stride);
  out_cr = (uint16_t*) outimg->get_plane(heif_channel_Cr, &out_cr_stride);

  if (has_alpha) {
    out_a = (uint16_t*) outimg->get_plane(heif_channel_Alpha, &out_a_stride);
  }


  // adapt stride as we are pointing to 16bit integers
  out_y_stride /= 2;
  out_cb_stride /= 2;
  out_cr_stride /= 2;
  out_a_stride /= 2;

  uint16_t halfRange = (uint16_t) (1 << (bpp - 1));
  int32_t fullRange = (1 << bpp) - 1;

  // le=1 for little endian, le=0 for big endian
  int le = (input->get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE ||
            input->get_chroma_format() == heif_chroma_interleaved_RRGGBB_LE) ? 1 : 0;

  auto colorProfile = input->get_color_profile_nclx();
  RGB_to_YCbCr_coefficients coeffs = RGB_to_YCbCr_coefficients::defaults();
  bool full_range_flag = true;
  if (colorProfile) {
    full_range_flag = target_state.nclx_profile->get_full_range_flag();
    coeffs = heif::get_RGB_to_YCbCr_coefficients(colorProfile->get_matrix_coefficients(),
                                                 colorProfile->get_colour_primaries());
  }

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {

      const uint8_t* in = &in_p[y * in_p_stride + bytesPerPixel * x];

      float r = static_cast<float>((in[0 + le] << 8) | in[1 - le]);
      float g = static_cast<float>((in[2 + le] << 8) | in[3 - le]);
      float b = static_cast<float>((in[4 + le] << 8) | in[5 - le]);

      float v = r * coeffs.c[0][0] + g * coeffs.c[0][1] + b * coeffs.c[0][2];

      if (!full_range_flag) {
        v = v * 0.85547f + 16;  // 0.85547 = 219/256
      }

      out_y[y * out_y_stride + x] = clip_f_u16(v, fullRange);

      if (has_alpha) {
        uint16_t a = (uint16_t) ((in[6 + le] << 8) | in[7 - le]);
        out_a[y * out_a_stride + x] = a;
      }
    }
  }

  for (int y = 0; y < height; y += 2) {
    for (int x = 0; x < width; x += 2) {
      const uint8_t* in = &in_p[y * in_p_stride + bytesPerPixel * x];

      float r = static_cast<float>((in[0 + le] << 8) | in[1 - le]);
      float g = static_cast<float>((in[2 + le] << 8) | in[3 - le]);
      float b = static_cast<float>((in[4 + le] << 8) | in[5 - le]);

      int dx = (x + 1 < width) ? bytesPerPixel : 0;
      int dy = (y + 1 < height) ? in_p_stride : 0;

      r += static_cast<float>((in[0 + le + dx] << 8) | in[1 - le + dx]);
      g += static_cast<float>((in[2 + le + dx] << 8) | in[3 - le + dx]);
      b += static_cast<float>((in[4 + le + dx] << 8) | in[5 - le + dx]);

      r += static_cast<float>((in[0 + le + dy] << 8) | in[1 - le + dy]);
      g += static_cast<float>((in[2 + le + dy] << 8) | in[3 - le + dy]);
      b += static_cast<float>((in[4 + le + dy] << 8) | in[5 - le + dy]);

      r += static_cast<float>((in[0 + le + dx + dy] << 8) | in[1 - le + dx + dy]);
      g += static_cast<float>((in[2 + le + dx + dy] << 8) | in[3 - le + dx + dy]);
      b += static_cast<float>((in[4 + le + dx + dy] << 8) | in[5 - le + dx + dy]);

      r *= 0.25f;
      g *= 0.25f;
      b *= 0.25f;

      float cb = r * coeffs.c[1][0] + g * coeffs.c[1][1] + b * coeffs.c[1][2];
      float cr = r * coeffs.c[2][0] + g * coeffs.c[2][1] + b * coeffs.c[2][2];

      if (!full_range_flag) {
        cb = cb * 0.85547f;  // 0.85547 = 219/256
        cr = cr * 0.85547f;  // 0.85547 = 219/256
      }

      out_cb[(y / 2) * out_cb_stride + (x / 2)] = clip_f_u16(halfRange + cb, fullRange);
      out_cr[(y / 2) * out_cr_stride + (x / 2)] = clip_f_u16(halfRange + cr, fullRange);
    }
  }


  return outimg;
}


class Op_YCbCr420_to_RRGGBBaa : public ColorConversionOperation
{
public:
  std::vector<ColorStateWithCost>
  state_after_conversion(const ColorState& input_state,
                         const ColorState& target_state,
                         const ColorConversionOptions& options) override;

  std::shared_ptr<HeifPixelImage>
  convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                     const ColorState& target_state,
                     const ColorConversionOptions& options) override;
};


std::vector<ColorStateWithCost>
Op_YCbCr420_to_RRGGBBaa::state_after_conversion(const ColorState& input_state,
                                                const ColorState& target_state,
                                                const ColorConversionOptions& options)
{
  if (input_state.colorspace != heif_colorspace_YCbCr ||
      input_state.chroma != heif_chroma_420 ||
      input_state.bits_per_pixel == 8) {
    return {};
  }

  if (input_state.nclx_profile) {
    int matrix = input_state.nclx_profile->get_matrix_coefficients();
    if (matrix == 0 || matrix == 8 || matrix == 11 || matrix == 14) {
      return {};
    }
  }

  std::vector<ColorStateWithCost> states;

  ColorState output_state;
  ColorConversionCosts costs;

  // --- convert to YCbCr

  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = (input_state.has_alpha ?
                         heif_chroma_interleaved_RRGGBBAA_LE : heif_chroma_interleaved_RRGGBB_LE);
  output_state.has_alpha = input_state.has_alpha;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});


  output_state.colorspace = heif_colorspace_RGB;
  output_state.chroma = (input_state.has_alpha ?
                         heif_chroma_interleaved_RRGGBBAA_BE : heif_chroma_interleaved_RRGGBB_BE);
  output_state.has_alpha = input_state.has_alpha;
  output_state.bits_per_pixel = input_state.bits_per_pixel;

  costs = {0.5f, 0.0f, 0.0f};

  states.push_back({output_state, costs});

  return states;
}


std::shared_ptr<HeifPixelImage>
Op_YCbCr420_to_RRGGBBaa::convert_colorspace(const std::shared_ptr<const HeifPixelImage>& input,
                                            const ColorState& target_state,
                                            const ColorConversionOptions& options)
{
  int width = input->get_width();
  int height = input->get_height();

  int bpp = input->get_bits_per_pixel(heif_channel_Y);
  bool has_alpha = input->has_channel(heif_channel_Alpha);

  int le = (target_state.chroma == heif_chroma_interleaved_RRGGBB_LE ||
            target_state.chroma == heif_chroma_interleaved_RRGGBBAA_LE) ? 1 : 0;

  auto outimg = std::make_shared<HeifPixelImage>();
  outimg->create(width, height, heif_colorspace_RGB, target_state.chroma);

  int bytesPerPixel = has_alpha ? 8 : 6;

  outimg->add_plane(heif_channel_interleaved, width, height, bpp);

  if (has_alpha) {
    outimg->add_plane(heif_channel_Alpha, width, height, bpp);
  }

  uint8_t* out_p;
  int out_p_stride = 0;

  const uint16_t* in_y, * in_cb, * in_cr, * in_a = nullptr;
  int in_y_stride = 0, in_cb_stride = 0, in_cr_stride = 0, in_a_stride = 0;

  out_p = outimg->get_plane(heif_channel_interleaved, &out_p_stride);
  in_y = (uint16_t*) input->get_plane(heif_channel_Y, &in_y_stride);
  in_cb = (uint16_t*) input->get_plane(heif_channel_Cb, &in_cb_stride);
  in_cr = (uint16_t*) input->get_plane(heif_channel_Cr, &in_cr_stride);

  if (has_alpha) {
    in_a = (uint16_t*) input->get_plane(heif_channel_Alpha, &in_a_stride);
  }

  int maxval = (1 << bpp) - 1;

  bool full_range_flag = true;
  YCbCr_to_RGB_coefficients coeffs = YCbCr_to_RGB_coefficients::defaults();

  auto colorProfile = input->get_color_profile_nclx();
  if (colorProfile) {
    full_range_flag = colorProfile->get_full_range_flag();
    coeffs = heif::get_YCbCr_to_RGB_coefficients(colorProfile->get_matrix_coefficients(),
                                                 colorProfile->get_colour_primaries());
  }

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {

      float y_ = in_y[y * in_y_stride / 2 + x];
      float cb = static_cast<float>(in_cb[y / 2 * in_cb_stride / 2 + x / 2] - (1 << (bpp - 1)));
      float cr = static_cast<float>(in_cr[y / 2 * in_cr_stride / 2 + x / 2] - (1 << (bpp - 1)));

      if (!full_range_flag) {
        y_ = (y_ - 16) * 1.1689f;
        cb = cb * 1.1429f;
        cr = cr * 1.1429f;
      }

      int r = clip_f_u16(y_ + coeffs.r_cr * cr, maxval);
      int g = clip_f_u16(y_ + coeffs.g_cb * cb - coeffs.g_cr * cr, maxval);
      int b = clip_f_u16(y_ + coeffs.b_cb * cb, maxval);

      out_p[y * out_p_stride + bytesPerPixel * x + 0 + le] = (uint8_t) (r >> 8);
      out_p[y * out_p_stride + bytesPerPixel * x + 2 + le] = (uint8_t) (g >> 8);
      out_p[y * out_p_stride + bytesPerPixel * x + 4 + le] = (uint8_t) (b >> 8);

      out_p[y * out_p_stride + bytesPerPixel * x + 1 - le] = (uint8_t) (r & 0xff);
      out_p[y * out_p_stride + bytesPerPixel * x + 3 - le] = (uint8_t) (g & 0xff);
      out_p[y * out_p_stride + bytesPerPixel * x + 5 - le] = (uint8_t) (b & 0xff);

      if (has_alpha) {
        out_p[y * out_p_stride + 8 * x + 6 + le] = (uint8_t) (in_a[y * in_a_stride / 2 + x] >> 8);
        out_p[y * out_p_stride + 8 * x + 7 - le] = (uint8_t) (in_a[y * in_a_stride / 2 + x] & 0xff);
      }
    }
  }


  return outimg;
}


struct Node
{
  Node() = default;

  Node(int prev, const std::shared_ptr<ColorConversionOperation>& _op, const ColorStateWithCost& state)
  {
    prev_processed_idx = prev;
    op = _op;
    color_state = state;
  }

  int prev_processed_idx = -1;
  std::shared_ptr<ColorConversionOperation> op;
  ColorStateWithCost color_state;
};


bool ColorConversionPipeline::construct_pipeline(const ColorState& input_state,
                                                 const ColorState& target_state,
                                                 const ColorConversionOptions& options)
{
  m_operations.clear();

  m_target_state = target_state;
  m_options = options;

  if (input_state == target_state) {
    return true;
  }

#if DEBUG_ME
  std::cerr << "--- construct_pipeline\n";
  std::cerr << "from: ";
  print_state(std::cerr, input_state);
  std::cerr << "to: ";
  print_state(std::cerr, target_state);
#endif

  std::vector<std::shared_ptr<ColorConversionOperation>> ops;
  ops.push_back(std::make_shared<Op_RGB_to_RGB24_32>());
  ops.push_back(std::make_shared<Op_YCbCr_to_RGB<uint16_t>>());
  ops.push_back(std::make_shared<Op_YCbCr_to_RGB<uint8_t>>());
  ops.push_back(std::make_shared<Op_YCbCr420_to_RGB24>());
  ops.push_back(std::make_shared<Op_YCbCr420_to_RGB32>());
  ops.push_back(std::make_shared<Op_YCbCr420_to_RRGGBBaa>());
  ops.push_back(std::make_shared<Op_RGB_HDR_to_RRGGBBaa_BE>());
  ops.push_back(std::make_shared<Op_RGB_to_RRGGBBaa_BE>());
  ops.push_back(std::make_shared<Op_mono_to_YCbCr420>());
  ops.push_back(std::make_shared<Op_mono_to_RGB24_32>());
  ops.push_back(std::make_shared<Op_RRGGBBaa_swap_endianness>());
  ops.push_back(std::make_shared<Op_RRGGBBaa_BE_to_RGB_HDR>());
  ops.push_back(std::make_shared<Op_RGB24_32_to_YCbCr>());
  ops.push_back(std::make_shared<Op_RGB24_32_to_YCbCr444_GBR>());
  ops.push_back(std::make_shared<Op_RGB_to_YCbCr<uint8_t>>());
  ops.push_back(std::make_shared<Op_RGB_to_YCbCr<uint16_t>>());
  ops.push_back(std::make_shared<Op_drop_alpha_plane>());
  ops.push_back(std::make_shared<Op_to_hdr_planes>());
  ops.push_back(std::make_shared<Op_to_sdr_planes>());
  ops.push_back(std::make_shared<Op_RRGGBBxx_HDR_to_YCbCr420>());


  // --- Dijkstra search for the minimum-cost conversion pipeline

  std::vector<Node> processed_states;
  std::vector<Node> border_states;
  border_states.push_back({-1, nullptr, {input_state, ColorConversionCosts()}});

  while (!border_states.empty()) {
    size_t minIdx = -1;
    float minCost;
    for (size_t i = 0; i < border_states.size(); i++) {
      float cost = border_states[i].color_state.costs.total(options.criterion);
      if (i == 0 || cost < minCost) {
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
    std::cerr << "- expand node: ";
    print_state(std::cerr, processed_states.back().color_state.color_state);
#endif

    if (processed_states.back().color_state.color_state == target_state) {
      // end-state found, backtrack path to find conversion pipeline

      size_t idx = processed_states.size() - 1;
      int len = 0;
      while (idx > 0) {
        idx = processed_states[idx].prev_processed_idx;
        len++;
      }

      m_operations.resize(len);

      idx = processed_states.size() - 1;
      int step = 0;
      while (idx > 0) {
        m_operations[len - 1 - step] = processed_states[idx].op;
        //printf("cost: %f\n",processed_states[idx].color_state.costs.total(options.criterion));
        idx = processed_states[idx].prev_processed_idx;
        step++;
      }

#if DEBUG_ME
      debug_dump_pipeline();
#endif

      return true;
    }


    // expand the node with minimum cost

    for (const auto& op_ptr : ops) {

#if DEBUG_PIPELINE_CREATION
      auto& op = *op_ptr;
      std::cerr << "-- apply op: " << typeid(op).name() << "\n";
#endif

      auto out_states = op_ptr->state_after_conversion(processed_states.back().color_state.color_state,
                                                       target_state,
                                                       options);
      for (const auto& out_state : out_states) {
#if DEBUG_PIPELINE_CREATION
        std::cerr << "--- ";
        print_state(std::cerr, out_state.color_state);
#endif

        bool state_exists = false;
        for (const auto& s : processed_states) {
          if (s.color_state.color_state == out_state.color_state) {
            state_exists = true;
            break;
          }
        }

        if (!state_exists) {
          for (auto& s : border_states) {
            if (s.color_state.color_state == out_state.color_state) {
              state_exists = true;

              // if we reached the same border node with a lower cost, replace the border node

              ColorConversionCosts new_op_costs = out_state.costs + processed_states.back().color_state.costs;

              if (s.color_state.costs.total(options.criterion) > new_op_costs.total(options.criterion)) {
                s = {(int) (processed_states.size() - 1),
                     op_ptr,
                     out_state};

                s.color_state.costs = new_op_costs;
              }
              break;
            }
          }
        }


        // enter the new output state into the list of border states

        if (!state_exists) {
          ColorStateWithCost s = out_state;
          s.costs = s.costs + processed_states.back().color_state.costs;

          border_states.push_back({(int) (processed_states.size() - 1), op_ptr, s});
        }
      }
    }
  }

  return false;
}


void ColorConversionPipeline::debug_dump_pipeline() const
{
  for (const auto& op_ptr : m_operations) {
    auto& op = *op_ptr;
    std::cerr << "> " << typeid(op).name() << "\n";
  }
}


std::shared_ptr<HeifPixelImage> ColorConversionPipeline::convert_image(const std::shared_ptr<HeifPixelImage>& input)
{
  std::shared_ptr<HeifPixelImage> in = input;
  std::shared_ptr<HeifPixelImage> out = in;

  for (const auto& op_ptr : m_operations) {

#if DEBUG_ME
    std::cerr << "input spec: ";
    print_spec(std::cerr, in);
#endif

    out = op_ptr->convert_colorspace(in, m_target_state, m_options);
    if (!out) {
      return nullptr; // TODO: we should return a proper error
    }

    // --- pass the color profiles to the new image

    out->set_color_profile_nclx(m_target_state.nclx_profile);
    out->set_color_profile_icc(in->get_color_profile_icc());

    out->set_premultiplied_alpha(in->is_premultiplied_alpha());

    auto warnings = in->get_warnings();
    for (const auto& warning : warnings) {
      out->add_warning(warning);
    }

    in = out;
  }

  return out;
}


std::shared_ptr<HeifPixelImage> heif::convert_colorspace(const std::shared_ptr<HeifPixelImage>& input,
                                                         heif_colorspace target_colorspace,
                                                         heif_chroma target_chroma,
                                                         const std::shared_ptr<const color_profile_nclx>& target_profile,
                                                         int output_bpp)
{
  // --- check that input image is valid

  int width = input->get_width();
  int height = input->get_height();

  // alpha image should have full image resolution

  if (input->has_channel(heif_channel_Alpha)) {
    if (input->get_width(heif_channel_Alpha) != width ||
        input->get_height(heif_channel_Alpha) != height) {
      return nullptr;
    }
  }

  // check for valid target YCbCr chroma formats

  if (target_colorspace == heif_colorspace_YCbCr) {
    if (target_chroma != heif_chroma_monochrome &&
        target_chroma != heif_chroma_420 &&
        target_chroma != heif_chroma_422 &&
        target_chroma != heif_chroma_444) {
      return nullptr;
    }
  }

  // --- prepare conversion

  ColorState input_state;
  input_state.colorspace = input->get_colorspace();
  input_state.chroma = input->get_chroma_format();
  input_state.has_alpha = input->has_channel(heif_channel_Alpha) || is_chroma_with_alpha(input->get_chroma_format());
  input_state.nclx_profile = input->get_color_profile_nclx();

  std::set<enum heif_channel> channels = input->get_channel_set();
  assert(!channels.empty());
  input_state.bits_per_pixel = input->get_bits_per_pixel(*(channels.begin()));

  ColorState output_state = input_state;
  output_state.colorspace = target_colorspace;
  output_state.chroma = target_chroma;
  output_state.nclx_profile = target_profile;

  // If we convert to an interleaved format, we want alpha only if present in the
  // interleaved output format.
  // For planar formats, we include an alpha plane when included in the input.

  if (num_interleaved_pixels_per_plane(target_chroma) > 1) {
    output_state.has_alpha = is_chroma_with_alpha(target_chroma);
  }
  else {
    output_state.has_alpha = input_state.has_alpha;
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
  bool success = pipeline.construct_pipeline(input_state, output_state);
  if (!success) {
    return nullptr;
  }

  return pipeline.convert_image(input);
}
