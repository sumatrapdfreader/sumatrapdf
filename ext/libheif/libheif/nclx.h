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

#ifndef LIBHEIF_NCLX_H
#define LIBHEIF_NCLX_H

#include <cinttypes>

namespace heif {

  struct primaries
  {
    primaries() = default;

    primaries(float gx, float gy, float bx, float by, float rx, float ry, float wx, float wy);

    bool defined = false;

    float greenX=0, greenY=0;
    float blueX=0, blueY=0;
    float redX=0, redY=0;
    float whiteX=0, whiteY=0;
  };

  primaries get_colour_primaries(uint16_t primaries_idx);


  struct Kr_Kb
  {
    float Kr = 0, Kb = 0;
  };

  Kr_Kb get_Kr_Kb(uint16_t matrix_coefficients_idx, uint16_t primaries_idx);

  struct YCbCr_to_RGB_coefficients
  {
    bool defined = false;

    float r_cr = 0;
    float g_cb = 0;
    float g_cr = 0;
    float b_cb = 0;

    static YCbCr_to_RGB_coefficients defaults();
  };

  YCbCr_to_RGB_coefficients get_YCbCr_to_RGB_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx);

  struct RGB_to_YCbCr_coefficients
  {
    bool defined = false;

    float c[3][3] = {{0,0,0},{0,0,0},{0,0,0}};   // e.g. y = c[0][0]*r + c[0][1]*g + c[0][2]*b

    static RGB_to_YCbCr_coefficients defaults();
  };

  RGB_to_YCbCr_coefficients get_RGB_to_YCbCr_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx);

//  uint16_t get_transfer_characteristics() const {return m_transfer_characteristics;}
// uint16_t get_matrix_coefficients() const {return m_matrix_coefficients;}
//  bool get_full_range_flag() const {return m_full_range_flag;}
}


#endif //LIBHEIF_NCLX_H
