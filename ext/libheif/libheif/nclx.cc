/*
 * HEIF codec.
 * Copyright (c) 2020 struktur AG, Dirk Farin <farin@struktur.de>
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


#include "nclx.h"


heif::primaries::primaries(float gx, float gy, float bx, float by, float rx, float ry, float wx, float wy)
{
  defined = true;
  redX = rx;
  redY = ry;
  greenX = gx;
  greenY = gy;
  blueX = bx;
  blueY = by;
  whiteX = wx;
  whiteY = wy;
}


heif::primaries heif::get_colour_primaries(uint16_t primaries_idx)
{
  switch (primaries_idx) {
    case 1:
      return {0.300f, 0.600f, 0.150f, 0.060f, 0.640f, 0.330f, 0.3127f, 0.3290f};
    case 4:
      return {0.21f, 0.71f, 0.14f, 0.08f, 0.67f, 0.33f, 0.310f, 0.316f};
    case 5:
      return {0.29f, 0.60f, 0.15f, 0.06f, 0.64f, 0.33f, 0.3127f, 0.3290f};
    case 6:
    case 7:
      return {0.310f, 0.595f, 0.155f, 0.070f, 0.630f, 0.340f, 0.3127f, 0.3290f};
    case 8:
      return {0.243f, 0.692f, 0.145f, 0.049f, 0.681f, 0.319f, 0.310f, 0.316f};
    case 9:
      return {0.170f, 0.797f, 0.131f, 0.046f, 0.708f, 0.292f, 0.3127f, 0.3290f};
    case 10:
      return {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.333333f, 0.33333f};
    case 11:
      return {0.265f, 0.690f, 0.150f, 0.060f, 0.680f, 0.320f, 0.314f, 0.351f};
    case 12:
      return {0.265f, 0.690f, 0.150f, 0.060f, 0.680f, 0.320f, 0.3127f, 0.3290f};
    case 22:
      return {0.295f, 0.605f, 0.155f, 0.077f, 0.630f, 0.340f, 0.3127f, 0.3290f};
    default:
      return {};
  }
}


heif::Kr_Kb heif::get_Kr_Kb(uint16_t matrix_coefficients_idx, uint16_t primaries_idx)
{
  Kr_Kb result;

  if (matrix_coefficients_idx == 12 ||
      matrix_coefficients_idx == 13) {

    primaries p = get_colour_primaries(primaries_idx);
    float zr = 1 - (p.redX + p.redY);
    float zg = 1 - (p.greenX + p.greenY);
    float zb = 1 - (p.blueX + p.blueY);
    float zw = 1 - (p.whiteX + p.whiteY);

    float denom = p.whiteY * (p.redX * (p.greenY * zb - p.blueY * zg) + p.greenX * (p.blueY * zr - p.redY * zb) +
                              p.blueX * (p.redY * zg - p.greenY * zr));

    if (denom == 0.0f) {
      return result;
    }

    result.Kr = (p.redY * (p.whiteX * (p.greenY * zb - p.blueY * zg) + p.whiteY * (p.blueX * zg - p.greenX * zb) +
                           zw * (p.greenX * p.blueY - p.blueX * p.greenY))) / denom;
    result.Kb = (p.blueY * (p.whiteX * (p.redY * zg - p.greenY * zr) + p.whiteY * (p.greenX * zr - p.redX * zg) +
                            zw * (p.redX * p.greenY - p.greenX * p.redY))) / denom;
  }
  else
    switch (matrix_coefficients_idx) {
      case 1:
        result.Kr = 0.2126f;
        result.Kb = 0.0722f;
        break;
      case 4:
        result.Kr = 0.30f;
        result.Kb = 0.11f;
        break;
      case 5:
      case 6:
        result.Kr = 0.299f;
        result.Kb = 0.114f;
        break;
      case 7:
        result.Kr = 0.212f;
        result.Kb = 0.087f;
        break;
      case 9:
      case 10:
        result.Kr = 0.2627f;
        result.Kb = 0.0593f;
        break;
      default:;
    }

  return result;
}


heif::YCbCr_to_RGB_coefficients heif::YCbCr_to_RGB_coefficients::defaults()
{
  YCbCr_to_RGB_coefficients coeffs;
  coeffs.defined = true;
  coeffs.r_cr = 1.402f;
  coeffs.g_cb = -0.344136f;
  coeffs.g_cr = -0.714136f;
  coeffs.b_cb = 1.772f;
  return coeffs;
}

heif::YCbCr_to_RGB_coefficients
heif::get_YCbCr_to_RGB_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx)
{
  YCbCr_to_RGB_coefficients coeffs;

  Kr_Kb k = get_Kr_Kb(matrix_coefficients_idx, primaries_idx);

  if (k.Kb != 0 || k.Kr != 0) { // both will be != 0 when valid
    coeffs.defined = true;
    coeffs.r_cr = 2 * (-k.Kr + 1);
    coeffs.g_cb = 2 * k.Kb * (-k.Kb + 1) / (k.Kb + k.Kr - 1);
    coeffs.g_cr = 2 * k.Kr * (-k.Kr + 1) / (k.Kb + k.Kr - 1);
    coeffs.b_cb = 2 * (-k.Kb + 1);
  }
  else {
    coeffs = YCbCr_to_RGB_coefficients::defaults();
  }

  return coeffs;
}


heif::RGB_to_YCbCr_coefficients
heif::get_RGB_to_YCbCr_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx)
{
  RGB_to_YCbCr_coefficients coeffs;

  Kr_Kb k = get_Kr_Kb(matrix_coefficients_idx, primaries_idx);

  if (k.Kb != 0 || k.Kr != 0) { // both will be != 0 when valid
    coeffs.defined = true;
    coeffs.c[0][0] = k.Kr;
    coeffs.c[0][1] = 1 - k.Kr - k.Kb;
    coeffs.c[0][2] = k.Kb;
    coeffs.c[1][0] = -k.Kr / (1 - k.Kb) / 2;
    coeffs.c[1][1] = -(1 - k.Kr - k.Kb) / (1 - k.Kb) / 2;
    coeffs.c[1][2] = 0.5f;
    coeffs.c[2][0] = 0.5f;
    coeffs.c[2][1] = -(1 - k.Kr - k.Kb) / (1 - k.Kr) / 2;
    coeffs.c[2][2] = -k.Kb / (1 - k.Kr) / 2;
  }
  else {
    coeffs = RGB_to_YCbCr_coefficients::defaults();
  }

  return coeffs;
}


heif::RGB_to_YCbCr_coefficients heif::RGB_to_YCbCr_coefficients::defaults()
{
  RGB_to_YCbCr_coefficients coeffs;
  coeffs.defined = true;

  coeffs.c[0][0] = 0.299f;
  coeffs.c[0][1] = 0.587f;
  coeffs.c[0][2] = 0.114f;
  coeffs.c[1][0] = -0.168735f;
  coeffs.c[1][1] = -0.331264f;
  coeffs.c[1][2] = 0.5f;
  coeffs.c[2][0] = 0.5f;
  coeffs.c[2][1] = -0.418688f;
  coeffs.c[2][2] = -0.081312f;

  return coeffs;
}

