/*
 * HEIF codec.
 * Copyright (c) 2020 Dirk Farin <dirk.farin@gmail.com>
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
#include "libheif/heif_experimental.h"

#include <cassert>
#include <memory>
#include <string>
#include <vector>


primaries::primaries(float gx, float gy, float bx, float by, float rx, float ry, float wx, float wy)
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


primaries get_colour_primaries(uint16_t primaries_idx)
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

Kr_Kb Kr_Kb::defaults()
{
  Kr_Kb kr_kb;
  // Rec 601.
  kr_kb.Kr = 0.2990f;
  kr_kb.Kb = 0.1140f;
  return kr_kb;
}


Kr_Kb get_Kr_Kb(uint16_t matrix_coefficients_idx, uint16_t primaries_idx)
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


YCbCr_to_RGB_coefficients YCbCr_to_RGB_coefficients::defaults()
{
  YCbCr_to_RGB_coefficients coeffs;
  coeffs.defined = true;
  coeffs.r_cr = 1.402f;
  coeffs.g_cb = -0.344136f;
  coeffs.g_cr = -0.714136f;
  coeffs.b_cb = 1.772f;
  return coeffs;
}

YCbCr_to_RGB_coefficients
get_YCbCr_to_RGB_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx)
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


RGB_to_YCbCr_coefficients
get_RGB_to_YCbCr_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx)
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


RGB_to_YCbCr_coefficients RGB_to_YCbCr_coefficients::defaults()
{
  RGB_to_YCbCr_coefficients coeffs;
  coeffs.defined = true;
  // Rec 601 full. Kr=0.2990f Kb=0.1140f.
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


Error color_profile_nclx::parse(BitstreamRange& range)
{
  StreamReader::grow_status status;
  status = range.wait_for_available_bytes(7);
  if (status != StreamReader::grow_status::size_reached) {
    // TODO: return recoverable error at timeout
    return Error(heif_error_Invalid_input,
                 heif_suberror_End_of_data);
  }

  m_profile.m_colour_primaries = range.read16();
  m_profile.m_transfer_characteristics = range.read16();
  m_profile.m_matrix_coefficients = range.read16();
  m_profile.m_full_range_flag = (range.read8() & 0x80 ? true : false);

  return Error::Ok;
}

Error color_profile_nclx::parse_nclc(BitstreamRange& range)
{
  StreamReader::grow_status status;
  status = range.wait_for_available_bytes(6);
  if (status != StreamReader::grow_status::size_reached) {
    // TODO: return recoverable error at timeout
    return Error(heif_error_Invalid_input,
                 heif_suberror_End_of_data);
  }

  m_profile.m_colour_primaries = range.read16();
  m_profile.m_transfer_characteristics = range.read16();
  m_profile.m_matrix_coefficients = range.read16();

  // use full range for RGB, limited range otherwise
  m_profile.m_full_range_flag = (m_profile.m_matrix_coefficients == 0);

  return Error::Ok;
}

Error nclx_profile::get_nclx_color_profile(heif_color_profile_nclx** out_data) const
{
  *out_data = nullptr;

  heif_color_profile_nclx* nclx = heif_nclx_color_profile_alloc();

  if (nclx == nullptr) {
    return Error(heif_error_Memory_allocation_error,
                 heif_suberror_Unspecified);
  }

  heif_error err;

  nclx->version = 1;

  err = heif_nclx_color_profile_set_color_primaries(nclx, get_colour_primaries());
  if (err.code) {
    heif_nclx_color_profile_free(nclx);
    return {err.code, err.subcode};
  }

  err = heif_nclx_color_profile_set_transfer_characteristics(nclx, get_transfer_characteristics());
  if (err.code) {
    heif_nclx_color_profile_free(nclx);
    return {err.code, err.subcode};
  }

  err = heif_nclx_color_profile_set_matrix_coefficients(nclx, get_matrix_coefficients());
  if (err.code) {
    heif_nclx_color_profile_free(nclx);
    return {err.code, err.subcode};
  }

  nclx->full_range_flag = get_full_range_flag();

  // fill color primaries

  auto primaries = ::get_colour_primaries(nclx->color_primaries);

  nclx->color_primary_red_x = primaries.redX;
  nclx->color_primary_red_y = primaries.redY;
  nclx->color_primary_green_x = primaries.greenX;
  nclx->color_primary_green_y = primaries.greenY;
  nclx->color_primary_blue_x = primaries.blueX;
  nclx->color_primary_blue_y = primaries.blueY;
  nclx->color_primary_white_x = primaries.whiteX;
  nclx->color_primary_white_y = primaries.whiteY;

  *out_data = nclx;

  return Error::Ok;
}


void nclx_profile::set_sRGB_defaults()
{
  // sRGB defaults
  m_colour_primaries = 1;
  m_transfer_characteristics = 13;
  m_matrix_coefficients = 6;
  m_full_range_flag = true;
}


void nclx_profile::set_undefined()
{
  m_colour_primaries = 2;
  m_transfer_characteristics = 2;
  m_matrix_coefficients = 2;
  m_full_range_flag = true;
}


bool nclx_profile::is_undefined() const
{
  return *this == nclx_profile::undefined();
}


void nclx_profile::set_from_heif_color_profile_nclx(const heif_color_profile_nclx* nclx)
{
  if (nclx) {
    m_colour_primaries = nclx->color_primaries;
    m_transfer_characteristics = nclx->transfer_characteristics;
    m_matrix_coefficients = nclx->matrix_coefficients;
    m_full_range_flag = nclx->full_range_flag;
  }
}


void nclx_profile::copy_to_heif_color_profile_nclx(heif_color_profile_nclx* nclx)
{
  assert(nclx);
  nclx->color_primaries = (enum heif_color_primaries)m_colour_primaries;
  nclx->transfer_characteristics = (enum heif_transfer_characteristics)m_transfer_characteristics;
  nclx->matrix_coefficients = (enum heif_matrix_coefficients)m_matrix_coefficients;
  nclx->full_range_flag = m_full_range_flag;
}


void nclx_profile::replace_undefined_values_with_sRGB_defaults()
{
  if (m_matrix_coefficients == heif_matrix_coefficients_unspecified) {
    m_matrix_coefficients = heif_matrix_coefficients_ITU_R_BT_601_6;
  }

  if (m_colour_primaries == heif_color_primaries_unspecified) {
    m_colour_primaries = heif_color_primaries_ITU_R_BT_709_5;
  }

  if (m_transfer_characteristics == heif_transfer_characteristic_unspecified) {
    m_transfer_characteristics = heif_transfer_characteristic_IEC_61966_2_1;
  }
}


bool nclx_profile::equal_except_transfer_curve(const nclx_profile& b) const
{
  return (m_matrix_coefficients == b.m_matrix_coefficients &&
          m_colour_primaries == b.m_colour_primaries &&
          m_full_range_flag == b.m_full_range_flag);
}


Error Box_colr::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  StreamReader::grow_status status;
  uint32_t colour_type = range.read32();

  if (colour_type == fourcc("nclx")) {
    auto color_profile = std::make_shared<color_profile_nclx>();
    m_color_profile = color_profile;
    Error err = color_profile->parse(range);
    if (err) {
      return err;
    }
  }
  else if (colour_type == fourcc("nclc")) {
    auto color_profile = std::make_shared<color_profile_nclx>();
    m_color_profile = color_profile;
    Error err = color_profile->parse_nclc(range);
    if (err) {
      return err;
    }
  }
  else if (colour_type == fourcc("prof") ||
           colour_type == fourcc("rICC")) {
    if (!has_fixed_box_size()) {
      return Error(heif_error_Unsupported_feature, heif_suberror_Unspecified, "colr boxes with undefined box size are not supported");
    }

    uint64_t profile_size_64 = get_box_size() - get_header_size() - 4;
    if (limits->max_color_profile_size && profile_size_64 > limits->max_color_profile_size) {
      return Error(heif_error_Invalid_input, heif_suberror_Security_limit_exceeded, "Color profile exceeds maximum supported size");
    }

    size_t profile_size = static_cast<size_t>(profile_size_64);

    status = range.wait_for_available_bytes(profile_size);
    if (status != StreamReader::grow_status::size_reached) {
      // TODO: return recoverable error at timeout
      return Error(heif_error_Invalid_input,
                   heif_suberror_End_of_data);
    }

    std::vector<uint8_t> rawData(profile_size);
    for (size_t i = 0; i < profile_size; i++) {
      rawData[i] = range.read8();
    }

    m_color_profile = std::make_shared<color_profile_raw>(colour_type, rawData);
  }
  else {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Unknown_color_profile_type);
  }

  return range.get_error();
}


std::string Box_colr::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  if (m_color_profile) {
    sstr << indent << "colour_type: " << fourcc_to_string(get_color_profile_type()) << "\n";
    sstr << m_color_profile->dump(indent);
  }
  else {
    sstr << indent << "colour_type: ---\n";
    sstr << "no color profile\n";
  }

  return sstr.str();
}


std::string color_profile_raw::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << indent << "profile size: " << m_data.size() << "\n";
  return sstr.str();
}


std::string color_profile_nclx::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << indent << "colour_primaries: " << m_profile.m_colour_primaries << "\n"
       << indent << "transfer_characteristics: " << m_profile.m_transfer_characteristics << "\n"
       << indent << "matrix_coefficients: " << m_profile.m_matrix_coefficients << "\n"
       << indent << "full_range_flag: " << m_profile.m_full_range_flag << "\n";
  return sstr.str();
}


Error color_profile_nclx::write(StreamWriter& writer) const
{
  writer.write16(m_profile.m_colour_primaries);
  writer.write16(m_profile.m_transfer_characteristics);
  writer.write16(m_profile.m_matrix_coefficients);
  writer.write8(m_profile.m_full_range_flag ? 0x80 : 0x00);

  return Error::Ok;
}

Error color_profile_raw::write(StreamWriter& writer) const
{
  writer.write(m_data);

  return Error::Ok;
}

Error Box_colr::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  assert(m_color_profile);

  writer.write32(m_color_profile->get_type());

  Error err = m_color_profile->write(writer);
  if (err) {
    return err;
  }

  prepend_header(writer, box_start);

  return Error::Ok;
}


