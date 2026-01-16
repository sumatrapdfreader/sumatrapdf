/*
 * HEIF codec.
 * Copyright (c) 2017-2023 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_BRANDS_H
#define LIBHEIF_HEIF_BRANDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <libheif/heif_library.h>



typedef uint32_t heif_brand2;

/**
 * HEVC image (`heic`) brand.
 *
 * Image conforms to HEVC (H.265) Main or Main Still profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.1.
 */
#define heif_brand2_heic   heif_fourcc('h','e','i','c')

/**
 * HEVC image (`heix`) brand.
 *
 * Image conforms to HEVC (H.265) Main 10 profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.1.
 */
#define heif_brand2_heix   heif_fourcc('h','e','i','x')

/**
 * HEVC image sequence (`hevc`) brand.
 *
 * Image sequence conforms to HEVC (H.265) Main profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.2.
 */
#define heif_brand2_hevc   heif_fourcc('h','e','v','c')

/**
 * HEVC image sequence (`hevx`) brand.
 *
 * Image sequence conforms to HEVC (H.265) Main 10 profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.2.
 */
#define heif_brand2_hevx   heif_fourcc('h','e','v','x')

/**
 * HEVC layered image (`heim`) brand.
 *
 * Image layers conform to HEVC (H.265) Main or Multiview Main profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.3.
 */
#define heif_brand2_heim   heif_fourcc('h','e','i','m')

/**
 * HEVC layered image (`heis`) brand.
 *
 * Image layers conform to HEVC (H.265) Main, Main 10, Scalable Main
 * or Scalable Main 10 profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.3.
 */
#define heif_brand2_heis   heif_fourcc('h','e','i','s')

/**
 * HEVC layered image sequence (`hevm`) brand.
 *
 * Image sequence layers conform to HEVC (H.265) Main or Multiview Main profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.4.
 */
#define heif_brand2_hevm   heif_fourcc('h','e','v','m')

/**
 * HEVC layered image sequence (`hevs`) brand.
 *
 * Image sequence layers conform to HEVC (H.265) Main, Main 10, Scalable Main
 * or Scalable Main 10 profile.
 *
 * See ISO/IEC 23008-12:2022 Section B.4.4.
 */
#define heif_brand2_hevs   heif_fourcc('h','e','v','s')

/**
 * AV1 image (`avif`) brand.
 *
 * See https://aomediacodec.github.io/av1-avif/#image-and-image-collection-brand
 */
#define heif_brand2_avif   heif_fourcc('a','v','i','f')

/**
 * AV1 image sequence (`avis`) brand.
 *
 * See https://aomediacodec.github.io/av1-avif/#image-sequence-brand
 */
#define heif_brand2_avis   heif_fourcc('a','v','i','s') // AVIF sequence

/**
 * HEIF image structural brand (`mif1`).
 *
 * This does not imply a specific coding algorithm.
 *
 * See ISO/IEC 23008-12:2022 Section 10.2.2.
 */
#define heif_brand2_mif1   heif_fourcc('m','i','f','1')

/**
 * HEIF image structural brand (`mif2`).
 *
 * This does not imply a specific coding algorithm. `mif2` extends
 * the requirements of `mif1` to include the `rref` and `iscl` item
 * properties.
 *
 * See ISO/IEC 23008-12:2022 Section 10.2.3.
 */
#define heif_brand2_mif2   heif_fourcc('m','i','f','2')

/**
 * HEIF image structural brand (`mif3`).
 *
 * This indicates the low-overhead (ftyp+mini) structure.
 */
#define heif_brand2_mif3   heif_fourcc('m','i','f','3')

/**
 * HEIF image sequence structural brand (`msf1`).
 *
 * This does not imply a specific coding algorithm.
 *
 * See ISO/IEC 23008-12:2022 Section 10.3.1.
 */
#define heif_brand2_msf1   heif_fourcc('m','s','f','1')

/**
 * VVC image (`vvic`) brand.
 *
 * See ISO/IEC 23008-12:2022 Section L.4.1.
 */
#define heif_brand2_vvic   heif_fourcc('v','v','i','c')

/**
 * VVC image sequence (`vvis`) brand.
 *
 * See ISO/IEC 23008-12:2022 Section L.4.2.
 */
#define heif_brand2_vvis   heif_fourcc('v','v','i','s')

/**
 * EVC baseline image (`evbi`) brand.
 *
 * See ISO/IEC 23008-12:2022 Section M.4.1.
 */
#define heif_brand2_evbi   heif_fourcc('e','v','b','i')

/**
 * EVC main profile image (`evmi`) brand.
 *
 * See ISO/IEC 23008-12:2022 Section M.4.2.
 */
#define heif_brand2_evmi   heif_fourcc('e','v','m','i')

/**
 * EVC baseline image sequence (`evbs`) brand.
 *
 * See ISO/IEC 23008-12:2022 Section M.4.3.
 */
#define heif_brand2_evbs   heif_fourcc('e','v','b','s')

/**
 * EVC main profile image sequence (`evms`) brand.
 *
 * See ISO/IEC 23008-12:2022 Section M.4.4.
 */
#define heif_brand2_evms   heif_fourcc('e','v','m','s')

/**
 * JPEG image (`jpeg`) brand.
 *
 * See ISO/IEC 23008-12:2022 Annex H.4
 */
#define heif_brand2_jpeg   heif_fourcc('j','p','e','g')

/**
 * JPEG image sequence (`jpgs`) brand.
 *
 * See ISO/IEC 23008-12:2022 Annex H.5
 */
#define heif_brand2_jpgs   heif_fourcc('j','p','g','s')

/**
 * JPEG 2000 image (`j2ki`) brand.
 *
 * See ISO/IEC 15444-16:2021 Section 6.5 
 */
#define heif_brand2_j2ki   heif_fourcc('j','2','k','i')

/**
 * JPEG 2000 image sequence (`j2is`) brand.
 *
 * See ISO/IEC 15444-16:2021 Section 7.6
 */
#define heif_brand2_j2is   heif_fourcc('j','2','i','s')

/**
 * Multi-image application format (MIAF) brand.
 *
 * This is HEIF with additional constraints for interoperability.
 *
 * See ISO/IEC 23000-22.
 */
#define heif_brand2_miaf   heif_fourcc('m','i','a','f')

/**
 * Single picture file brand.
 *
 * This is a compatible brand indicating the file contains a single intra-coded picture.
 *
 * See ISO/IEC 23008-12:2022 Section 10.2.5.
*/
#define heif_brand2_1pic   heif_fourcc('1','p','i','c')

// H.264
#define heif_brand2_avci   heif_fourcc('a','v','c','i')
#define heif_brand2_avcs   heif_fourcc('a','v','c','s')

#define heif_brand2_iso8   heif_fourcc('i','s','o','8')
#define heif_brand2_isom   heif_fourcc('i','s','o','m')
#define heif_brand2_mp41   heif_fourcc('m','p','4','1')
#define heif_brand2_mp42   heif_fourcc('m','p','4','2')

// input data should be at least 12 bytes
LIBHEIF_API
heif_brand2 heif_read_main_brand(const uint8_t* data, int len);

// input data should be at least 16 bytes
LIBHEIF_API
heif_brand2 heif_read_minor_version_brand(const uint8_t* data, int len);

// 'brand_fourcc' must be 4 character long, but need not be 0-terminated
LIBHEIF_API
heif_brand2 heif_fourcc_to_brand(const char* brand_fourcc);

// the output buffer must be at least 4 bytes long
LIBHEIF_API
void heif_brand_to_fourcc(heif_brand2 brand, char* out_fourcc);

// 'brand_fourcc' must be 4 character long, but need not be 0-terminated
// returns 1 if file includes the brand, and 0 if it does not
// returns -1 if the provided data is not sufficient
//            (you should input at least as many bytes as indicated in the first 4 bytes of the file, usually ~50 bytes will do)
// returns -2 on other errors
LIBHEIF_API
int heif_has_compatible_brand(const uint8_t* data, int len, const char* brand_fourcc);

// Returns an array of compatible brands. The array is allocated by this function and has to be freed with 'heif_free_list_of_compatible_brands()'.
// The number of entries is returned in out_size.
LIBHEIF_API
struct heif_error heif_list_compatible_brands(const uint8_t* data, int len, heif_brand2** out_brands, int* out_size);

LIBHEIF_API
void heif_free_list_of_compatible_brands(heif_brand2* brands_list);



// Returns one of these MIME types:
// - image/heic           HEIF file using h265 compression
// - image/heif           HEIF file using any other compression
// - image/heic-sequence  HEIF image sequence using h265 compression
// - image/heif-sequence  HEIF image sequence using any other compression
// - image/avif           AVIF image
// - image/avif-sequence  AVIF sequence
// - image/jpeg    JPEG image
// - image/png     PNG image
// If the format could not be detected, an empty string is returned.
//
// Provide at least 12 bytes of input. With less input, its format might not
// be detected. You may also provide more input to increase detection accuracy.
//
// Note that JPEG and PNG images cannot be decoded by libheif even though the
// formats are detected by this function.
LIBHEIF_API
const char* heif_get_file_mime_type(const uint8_t* data, int len);


// ========================= file type check ======================

enum heif_filetype_result
{
  heif_filetype_no,
  heif_filetype_yes_supported,   // it is heif and can be read by libheif
  heif_filetype_yes_unsupported, // it is heif, but cannot be read by libheif
  heif_filetype_maybe // not sure whether it is an heif, try detection with more input data
};

// input data should be at least 12 bytes
LIBHEIF_API
enum heif_filetype_result heif_check_filetype(const uint8_t* data, int len);

/**
 * Check the filetype box content for a supported file type.
 *
 * <p>The data is assumed to start from the start of the `ftyp` box.
 *
 * <p>This function checks the compatible brands.
 *
 * @returns heif_error_ok if a supported brand is found, or other error if not.
 */
LIBHEIF_API
heif_error heif_has_compatible_filetype(const uint8_t* data, int len);

LIBHEIF_API
int heif_check_jpeg_filetype(const uint8_t* data, int len);


// ===================== DEPRECATED =====================

// DEPRECATED, use heif_brand2 and the heif_brand2_* constants instead
enum heif_brand
{
  heif_unknown_brand,
  heif_heic, // HEIF image with h265
  heif_heix, // 10bit images, or anything that uses h265 with range extension
  heif_hevc, heif_hevx, // brands for image sequences
  heif_heim, // multiview
  heif_heis, // scalable
  heif_hevm, // multiview sequence
  heif_hevs, // scalable sequence
  heif_mif1, // image, any coding algorithm
  heif_msf1, // sequence, any coding algorithm
  heif_avif, // HEIF image with AV1
  heif_avis,
  heif_vvic, // VVC image
  heif_vvis, // VVC sequence
  heif_evbi, // EVC image
  heif_evbs, // EVC sequence
  heif_j2ki, // JPEG2000 image
  heif_j2is, // JPEG2000 image sequence
};

// input data should be at least 12 bytes
// DEPRECATED, use heif_read_main_brand() instead
LIBHEIF_API
enum heif_brand heif_main_brand(const uint8_t* data, int len);

#ifdef __cplusplus
}
#endif

#endif
