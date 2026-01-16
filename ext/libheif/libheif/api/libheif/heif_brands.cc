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

#include "heif_brands.h"
#include "heif_error.h"
#include "error.h"
#include "common_utils.h"
#include "bitstream.h"
#include "box.h"

#include <cstring>
#include <set>
#include <memory>



heif_brand2 heif_read_main_brand(const uint8_t* data, int len)
{
  if (len < 12) {
    return heif_unknown_brand;
  }

  return heif_fourcc_to_brand((char*) (data + 8));
}


heif_brand2 heif_read_minor_version_brand(const uint8_t* data, int len)
{
  if (len < 16) {
    return heif_unknown_brand;
  }
  return heif_fourcc_to_brand((char*) (data + 12));
}


heif_brand2 heif_fourcc_to_brand(const char* fourcc_string)
{
  if (fourcc_string == nullptr || !fourcc_string[0] || !fourcc_string[1] || !fourcc_string[2] || !fourcc_string[3]) {
    return 0;
  }

  return fourcc(fourcc_string);
}

void heif_brand_to_fourcc(heif_brand2 brand, char* out_fourcc)
{
  if (out_fourcc) {
    out_fourcc[0] = (char) ((brand >> 24) & 0xFF);
    out_fourcc[1] = (char) ((brand >> 16) & 0xFF);
    out_fourcc[2] = (char) ((brand >> 8) & 0xFF);
    out_fourcc[3] = (char) ((brand >> 0) & 0xFF);
  }
}


int heif_has_compatible_brand(const uint8_t* data, int len, const char* brand_fourcc)
{
  if (data == nullptr || len <= 0 || brand_fourcc == nullptr || !brand_fourcc[0] || !brand_fourcc[1] || !brand_fourcc[2] || !brand_fourcc[3]) {
    return -1;
  }

  auto stream = std::make_shared<StreamReader_memory>(data, len, false);
  BitstreamRange range(stream, len);

  std::shared_ptr<Box> box;
  Error err = Box::read(range, &box, heif_get_global_security_limits());
  if (err) {
    if (err.sub_error_code == heif_suberror_End_of_data) {
      return -1;
    }

    return -2;
  }

  auto ftyp = std::dynamic_pointer_cast<Box_ftyp>(box);
  if (!ftyp) {
    return -2;
  }

  return ftyp->has_compatible_brand(fourcc(brand_fourcc)) ? 1 : 0;
}


heif_error heif_list_compatible_brands(const uint8_t* data, int len, heif_brand2** out_brands, int* out_size)
{
  if (data == nullptr || out_brands == nullptr || out_size == nullptr) {
    return heif_error_null_pointer_argument;
  }

  if (len <= 0) {
    return {heif_error_Usage_error, heif_suberror_Invalid_parameter_value, "data length must be positive"};
  }

  const heif_security_limits* security_limits = heif_get_global_security_limits();


  // --- check that file begins with ftyp box

  {
    auto stream = std::make_shared<StreamReader_memory>(data, len, false);
    BitstreamRange range(stream, len);

    BoxHeader hdr;
    Error err = hdr.parse_header(range);
    if (err) {
      return {err.error_code, err.sub_error_code, "error reading ftype box header"};
    }

    if (hdr.get_short_type() != fourcc("ftyp")) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "File does not begin with 'ftyp' box."
      };
    }
  }

  auto stream = std::make_shared<StreamReader_memory>(data, len, false);
  BitstreamRange range(stream, len);

  std::shared_ptr<Box> box;
  Error err = Box::read(range, &box, security_limits);
  if (err) {
    if (err.sub_error_code == heif_suberror_End_of_data) {
      return {err.error_code, err.sub_error_code, "insufficient input data"};
    }

    return {err.error_code, err.sub_error_code, "error reading ftyp box"};
  }

  auto ftyp = std::dynamic_pointer_cast<Box_ftyp>(box);
  if (!ftyp) {
    return {heif_error_Invalid_input, heif_suberror_No_ftyp_box, "input is not a ftyp box"};
  }

  auto brands = ftyp->list_brands();
  size_t nBrands = brands.size();

  if (nBrands > security_limits->max_number_of_file_brands) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "File contains more brands than allowed by security limits."
    };
  }

  *out_brands = (heif_brand2*) malloc(sizeof(heif_brand2) * nBrands);
  *out_size = (int)nBrands;

  for (size_t i = 0; i < nBrands; i++) {
    (*out_brands)[i] = brands[i];
  }

  return heif_error_success;
}


void heif_free_list_of_compatible_brands(heif_brand2* brands_list)
{
  if (brands_list) {
    free(brands_list);
  }
}


enum class TriBool
{
  No, Yes, Unknown
};

static TriBool is_jpeg(const uint8_t* data, int len)
{
  if (len < 12) {
    return TriBool::Unknown;
  }

  if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF && data[3] == 0xE0 &&
      data[4] == 0x00 && data[5] == 0x10 && data[6] == 0x4A && data[7] == 0x46 &&
      data[8] == 0x49 && data[9] == 0x46 && data[10] == 0x00 && data[11] == 0x01) {
    return TriBool::Yes;
  }
  if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF && data[3] == 0xE1 &&
      data[6] == 0x45 && data[7] == 0x78 && data[8] == 0x69 && data[9] == 0x66 &&
      data[10] == 0x00 && data[11] == 0x00) {
    return TriBool::Yes;
  }
  else {
    return TriBool::No;
  }
}


static TriBool is_png(const uint8_t* data, int len)
{
  if (len < 8) {
    return TriBool::Unknown;
  }

  if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47 &&
      data[4] == 0x0D && data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
    return TriBool::Yes;
  }
  else {
    return TriBool::No;
  }
}


const char* heif_get_file_mime_type(const uint8_t* data, int len)
{
  heif_brand2 mainBrand = heif_read_main_brand(data, len);

  if (mainBrand == heif_brand2_heic ||
      mainBrand == heif_brand2_heix ||
      mainBrand == heif_brand2_heim ||
      mainBrand == heif_brand2_heis) {
    return "image/heic";
  }
  else if (mainBrand == heif_brand2_mif1) {
    return "image/heif";
  }
  else if (mainBrand == heif_brand2_hevc ||
           mainBrand == heif_brand2_hevx ||
           mainBrand == heif_brand2_hevm ||
           mainBrand == heif_brand2_hevs) {
    return "image/heic-sequence";
  }
  else if (mainBrand == heif_brand2_msf1) {
    return "image/heif-sequence";
  }
  else if (mainBrand == heif_brand2_avif) {
    return "image/avif";
  }
  else if (mainBrand == heif_brand2_avis) {
    return "image/avif-sequence";
  }
  else if (mainBrand == heif_brand2_avci) {
    return "image/avci";
  }
  else if (mainBrand == heif_brand2_avcs) {
    return "image/avcs";
  }
#if ENABLE_EXPERIMENTAL_MINI_FORMAT
  else if (mainBrand == heif_brand2_mif3) {
    heif_brand2 minorBrand = heif_read_minor_version_brand(data, len);
    if (minorBrand == heif_brand2_avif) {
      return "image/avif";
    }
    if (minorBrand == heif_brand2_heic ||
        minorBrand == heif_brand2_heix ||
        minorBrand == heif_brand2_heim ||
        minorBrand == heif_brand2_heis) {
      return "image/heic";
    }
    // There could be other options in here, like VVC or J2K
    return "image/heif";
  }
#endif
  else if (mainBrand == heif_brand2_j2ki) {
    return "image/hej2k";
  }
  else if (mainBrand == heif_brand2_j2is) {
    return "image/j2is";
  }
  else if (is_jpeg(data, len) == TriBool::Yes) {
    return "image/jpeg";
  }
  else if (is_png(data, len) == TriBool::Yes) {
    return "image/png";
  }
  else {
    return "";
  }
}

heif_filetype_result heif_check_filetype(const uint8_t* data, int len)
{
  if (len < 8) {
    return heif_filetype_maybe;
  }

  if (data[4] != 'f' ||
      data[5] != 't' ||
      data[6] != 'y' ||
      data[7] != 'p') {
    return heif_filetype_no;
  }

  if (len >= 12) {
    heif_brand2 brand = heif_read_main_brand(data, len);

    if (brand == heif_brand2_heic) {
      return heif_filetype_yes_supported;
    }
    else if (brand == heif_brand2_heix) {
      return heif_filetype_yes_supported;
    }
    else if (brand == heif_brand2_avif) {
      return heif_filetype_yes_supported;
    }
    else if (brand == heif_brand2_jpeg) {
      return heif_filetype_yes_supported;
    }
    else if (brand == heif_brand2_j2ki) {
      return heif_filetype_yes_supported;
    }
    else if (brand == heif_brand2_mif1) {
      return heif_filetype_maybe;
    }
    else if (brand == heif_brand2_mif2) {
      return heif_filetype_maybe;
    }
    else {
      return heif_filetype_yes_unsupported;
    }
  }

  return heif_filetype_maybe;
}


heif_error heif_has_compatible_filetype(const uint8_t* data, int len)
{
  // Get compatible brands first, because that does validity checks
  heif_brand2* compatible_brands = nullptr;
  int nBrands = 0;
  struct heif_error err = heif_list_compatible_brands(data, len, &compatible_brands, &nBrands);
  if (err.code) {
    assert(compatible_brands == nullptr); // NOLINT(clang-analyzer-unix.Malloc)
    return err;
  }

  heif_brand2 main_brand = heif_read_main_brand(data, len);

  std::set<heif_brand2> supported_brands{
      heif_brand2_avif,
      heif_brand2_heic,
      heif_brand2_heix,
      heif_brand2_j2ki,
      heif_brand2_jpeg,
      heif_brand2_miaf,
      heif_brand2_mif1,
      heif_brand2_mif2
#if ENABLE_EXPERIMENTAL_MINI_FORMAT
      , heif_brand2_mif3
#endif
      ,heif_brand2_msf1,
    heif_brand2_isom,
    heif_brand2_mp41,
    heif_brand2_mp42
  };

  auto it = supported_brands.find(main_brand);
  if (it != supported_brands.end()) {
    heif_free_list_of_compatible_brands(compatible_brands);
    return heif_error_success;
  }

  for (int i = 0; i < nBrands; i++) {
    heif_brand2 compatible_brand = compatible_brands[i];
    it = supported_brands.find(compatible_brand);
    if (it != supported_brands.end()) {
      heif_free_list_of_compatible_brands(compatible_brands);
      return heif_error_success;
    }
  }
  heif_free_list_of_compatible_brands(compatible_brands);
  return {heif_error_Invalid_input, heif_suberror_Unsupported_image_type, "No supported brands found."};;
}


int heif_check_jpeg_filetype(const uint8_t* data, int len)
{
  if (len < 4 || data == nullptr) {
    return -1;
  }

  return (data[0] == 0xFF &&
	  data[1] == 0xD8 &&
	  data[2] == 0xFF &&
	  (data[3] & 0xF0) == 0xE0);
}


static heif_brand heif_fourcc_to_brand_enum(const char* fourcc)
{
  if (fourcc == nullptr || !fourcc[0] || !fourcc[1] || !fourcc[2] || !fourcc[3]) {
    return heif_unknown_brand;
  }

  char brand[5];
  brand[0] = fourcc[0];
  brand[1] = fourcc[1];
  brand[2] = fourcc[2];
  brand[3] = fourcc[3];
  brand[4] = 0;

  if (strcmp(brand, "heic") == 0) {
    return heif_heic;
  }
  else if (strcmp(brand, "heix") == 0) {
    return heif_heix;
  }
  else if (strcmp(brand, "hevc") == 0) {
    return heif_hevc;
  }
  else if (strcmp(brand, "hevx") == 0) {
    return heif_hevx;
  }
  else if (strcmp(brand, "heim") == 0) {
    return heif_heim;
  }
  else if (strcmp(brand, "heis") == 0) {
    return heif_heis;
  }
  else if (strcmp(brand, "hevm") == 0) {
    return heif_hevm;
  }
  else if (strcmp(brand, "hevs") == 0) {
    return heif_hevs;
  }
  else if (strcmp(brand, "mif1") == 0) {
    return heif_mif1;
  }
  else if (strcmp(brand, "msf1") == 0) {
    return heif_msf1;
  }
  else if (strcmp(brand, "avif") == 0) {
    return heif_avif;
  }
  else if (strcmp(brand, "avis") == 0) {
    return heif_avis;
  }
  else if (strcmp(brand, "vvic") == 0) {
    return heif_vvic;
  }
  else if (strcmp(brand, "j2ki") == 0) {
    return heif_j2ki;
  }
  else if (strcmp(brand, "j2is") == 0) {
    return heif_j2is;
  }
  else {
    return heif_unknown_brand;
  }
}


enum heif_brand heif_main_brand(const uint8_t* data, int len)
{
  if (len < 12) {
    return heif_unknown_brand;
  }

  return heif_fourcc_to_brand_enum((char*) (data + 8));
}
