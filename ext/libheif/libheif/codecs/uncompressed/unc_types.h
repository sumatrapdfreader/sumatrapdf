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


#ifndef LIBHEIF_UNC_TYPES_H
#define LIBHEIF_UNC_TYPES_H

#include <cstdint>

/**
 * Component type.
 *
 * See ISO/IEC 23001-17 Table 1.
*/
enum heif_uncompressed_component_type
{
  /**
   * Monochrome component.
   */
  component_type_monochrome = 0,

  /**
   * Luma component (Y).
   */
  component_type_Y = 1,

  /**
   * Chroma component (Cb / U).
   */
  component_type_Cb = 2,

  /**
   * Chroma component (Cr / V).
   */
  component_type_Cr = 3,

  /**
   * Red component (R).
   */
  component_type_red = 4,

  /**
   * Green component (G).
   */
  component_type_green = 5,

  /**
   * Blue component (B).
   */
  component_type_blue = 6,

  /**
   * Alpha / transparency component (A).
   */
  component_type_alpha = 7,

  /**
   * Depth component (D).
   */
  component_type_depth = 8,

  /**
   * Disparity component (Disp).
   */
  component_type_disparity = 9,

  /**
   * Palette component (P).
   *
   * The {@code component_format} value for this component shall be 0.
   */
  component_type_palette = 10,

  /**
   * Filter Array (FA) component such as Bayer, RGBW, etc.
   */
  component_type_filter_array = 11,

  /**
   * Padded component (unused bits/bytes).
   */
  component_type_padded = 12,

  /**
   * Cyan component (C).
   */
  component_type_cyan = 13,

  /**
   * Magenta component (M).
   */
  component_type_magenta = 14,

  /**
   * Yellow component (Y).
   */
  component_type_yellow = 15,

  /**
   * Key (black) component (K).
   */
  component_type_key_black = 16,

  /**
   * Maximum valid component type value.
   */
  component_type_max_valid = component_type_key_black
};

/**
 * HEIF uncompressed component format.
 *
 * The binary representation of a component is determined by the
 * {@code component_bit_depth} and the component format.
 *
 * See ISO/IEC 23001-17 Table 2.
 */
enum heif_uncompressed_component_format
{
  /**
   * Unsigned integer.
   *
   * The component value is an unsigned integer.
   */
  component_format_unsigned = 0,

  /**
   * Floating point value.
   *
   * The component value is an IEEE 754 binary float.
   * Valid bit depths for this format are:
   * <ul>
   *  <li>16 (half precision)
   *  <li>32 (single precision)
   *  <li>64 (double precision)
   *  <li>128 (quadruple precision)
   *  <li>256 (octuple precision)
   * </ul>
   */
  component_format_float = 1,

  /**
   * Complex value.
   *
   * The component value is two IEEE 754 binary float numbers
   * where the first part is the real part of the value and
   * the second part is the imaginary part of the value.
   *
   * Each part has the same number of bits, which is half
   * the component bit depth value. Valid bit depths for this
   * format are:
   * <ul>
   *  <li>32 - each part is 16 bits (half precision)
   *  <li>64 - each part is 32 bits (single precision)
   *  <li>128 - each part is 64 bits (double precision)
   *  <li>256 - each part is 128 bits (quadruple precision)
   * </ul>
   */
  component_format_complex = 2,

  /**
   * Signed integer.
   *
   * The component value is a two's complement signed integer.
   */
  component_format_signed = 3,

  /**
   * Maximum valid component format identifier.
   */
  component_format_max_valid = component_format_signed
};


/**
 * HEIF uncompressed sampling mode.
 *
 * All components in a frame use the same dimensions, or use pre-defined
 * sampling modes. This is only valid for YCbCr formats.
 *
 * See ISO/IEC 23001-17 Table 3.
 */
enum heif_uncompressed_sampling_mode
{
  /**
   * No subsampling.
   */
  sampling_mode_no_subsampling = 0,

  /**
   * YCbCr 4:2:2 subsampling.
   *
   * Y dimensions are the same as the dimensions of the frame.
   * Cb (U) and Cr (V) have the same height as the frame, but only have
   * half the width.
   */
  sampling_mode_422 = 1,

  /**
   * YCbCr 4:2:0 subsampling.
   *
   * Y dimensions are the same as the dimensions of the frame.
   * Cb (U) and Cr (V) have the half the height and half the width of
   * the frame.
   */
  sampling_mode_420 = 2,

  /**
   * YCbCr 4:1:1 subsampling.
   *
   * Y dimensions are the same as the dimensions of the frame.
   * Cb (U) and Cr (V) have the same height as the frame, but only have
   * one quarter the width.
   */
  sampling_mode_411 = 3,

  /**
   * Maximum valid sampling mode identifier.
   */
  sampling_mode_max_valid = sampling_mode_411
};


/**
 * HEIF uncompressed interleaving mode.
 *
 * See ISO/IEC 23001-17 Table 4.
 */
enum heif_uncompressed_interleave_mode
{
  /**
   * Component interleaving.
   */
  interleave_mode_component = 0,

  /**
   * Pixel interleaving.
   */
  interleave_mode_pixel = 1,

  /**
   * Mixed interleaving.
   *
   * This is associated with YCbCr images, with subsampling
   * and "semi-planar" interleave.
   */
  interleave_mode_mixed = 2,

  /**
   * Row interleaving.
   */
  interleave_mode_row = 3,

  /**
   * Tile-component interleaving.
   */
  interleave_mode_tile_component = 4,

  /**
   * Multi-Y pixel interleaving.
   *
   * This is only valid with 4:2:2 and 4:1:1 subsampling.
  */
  interleave_mode_multi_y = 5,

  /**
   * Maximum valid interleave mode identifier.
   */
  interleave_mode_max_valid = interleave_mode_multi_y
};




#endif //LIBHEIF_UNC_TYPES_H
