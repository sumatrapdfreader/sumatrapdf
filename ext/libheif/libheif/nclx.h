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

#ifndef LIBHEIF_NCLX_H
#define LIBHEIF_NCLX_H

#include "box.h"

#include <cinttypes>
#include <memory>
#include <string>
#include <vector>

struct primaries
{
  primaries() = default;

  primaries(float gx, float gy, float bx, float by, float rx, float ry, float wx, float wy);

  bool defined = false;

  float greenX = 0, greenY = 0;
  float blueX = 0, blueY = 0;
  float redX = 0, redY = 0;
  float whiteX = 0, whiteY = 0;
};

primaries get_colour_primaries(uint16_t primaries_idx);


struct Kr_Kb
{
  float Kr = 0, Kb = 0;

  static Kr_Kb defaults();
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

  float c[3][3] = {{0, 0, 0},
                   {0, 0, 0},
                   {0, 0, 0}};   // e.g. y = c[0][0]*r + c[0][1]*g + c[0][2]*b

  static RGB_to_YCbCr_coefficients defaults();
};

RGB_to_YCbCr_coefficients get_RGB_to_YCbCr_coefficients(uint16_t matrix_coefficients_idx, uint16_t primaries_idx);

//  uint16_t get_transfer_characteristics() const {return m_transfer_characteristics;}
// uint16_t get_matrix_coefficients() const {return m_matrix_coefficients;}
//  bool get_full_range_flag() const {return m_full_range_flag;}


class color_profile
{
public:
  virtual ~color_profile() = default;

  virtual uint32_t get_type() const = 0;

  virtual std::string dump(Indent&) const = 0;

  virtual Error write(StreamWriter& writer) const = 0;
};

class color_profile_raw : public color_profile
{
public:
  color_profile_raw(uint32_t type, const std::vector<uint8_t>& data)
      : m_type(type), m_data(data) {}

  uint32_t get_type() const override { return m_type; }

  const std::vector<uint8_t>& get_data() const { return m_data; }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

private:
  uint32_t m_type;
  std::vector<uint8_t> m_data;
};


struct nclx_profile
{
  uint16_t m_colour_primaries = heif_color_primaries_unspecified;
  uint16_t m_transfer_characteristics = heif_transfer_characteristic_unspecified;
  uint16_t m_matrix_coefficients = heif_matrix_coefficients_unspecified;
  bool m_full_range_flag = true;

  bool operator==(const nclx_profile& b) const = default;

  bool operator!=(const nclx_profile& b) const = default;

  static nclx_profile undefined() { return {}; }

  static nclx_profile defaults() { nclx_profile profile; profile.set_sRGB_defaults(); return profile; }

  heif_color_primaries get_colour_primaries() const { return static_cast<heif_color_primaries>(m_colour_primaries); }

  heif_transfer_characteristics get_transfer_characteristics() const { return static_cast<heif_transfer_characteristics>(m_transfer_characteristics); }

  heif_matrix_coefficients get_matrix_coefficients() const { return static_cast<heif_matrix_coefficients>(m_matrix_coefficients); }

  bool get_full_range_flag() const { return m_full_range_flag; }

  void set_colour_primaries(uint16_t primaries) { m_colour_primaries = primaries; }

  void set_transfer_characteristics(uint16_t characteristics) { m_transfer_characteristics = characteristics; }

  void set_matrix_coefficients(uint16_t coefficients) { m_matrix_coefficients = coefficients; }

  void set_full_range_flag(bool full_range) { m_full_range_flag = full_range; }

  void set_sRGB_defaults();

  void set_undefined();

  bool is_undefined() const;

  void replace_undefined_values_with_sRGB_defaults();

  bool equal_except_transfer_curve(const nclx_profile& b) const;

  Error get_nclx_color_profile(heif_color_profile_nclx** out_data) const;

  void set_from_heif_color_profile_nclx(const heif_color_profile_nclx* nclx);

  void copy_to_heif_color_profile_nclx(heif_color_profile_nclx* nclx);
};


class color_profile_nclx : public color_profile
{
public:
  color_profile_nclx() { m_profile.set_sRGB_defaults(); }

  color_profile_nclx(const nclx_profile& profile) : m_profile(profile) { }

  void set_from_heif_color_profile_nclx(const heif_color_profile_nclx* nclx)
  {
    m_profile.set_from_heif_color_profile_nclx(nclx);
  }

  uint32_t get_type() const override { return fourcc("nclx"); }

  std::string dump(Indent&) const override;

  Error parse(BitstreamRange& range);

  Error parse_nclc(BitstreamRange& range);

  Error write(StreamWriter& writer) const override;

  nclx_profile get_nclx_color_profile() const { return m_profile; }

private:
  nclx_profile m_profile;
};


class Box_colr : public Box
{
public:
  Box_colr()
  {
    set_short_type(fourcc("colr"));
  }

  std::string dump(Indent&) const override;

  uint32_t get_color_profile_type() const { return m_color_profile->get_type(); }

  const std::shared_ptr<const color_profile>& get_color_profile() const { return m_color_profile; }

  void set_color_profile(const std::shared_ptr<const color_profile>& prof) { m_color_profile = prof; }


  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  std::shared_ptr<const color_profile> m_color_profile;
};


#endif //LIBHEIF_NCLX_H
