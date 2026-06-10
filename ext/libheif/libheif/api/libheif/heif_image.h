/*
 * HEIF codec.
 * Copyright (c) 2017-2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_IMAGE_H
#define LIBHEIF_HEIF_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_library.h>
#include <libheif/heif_error.h>
#include <stddef.h>
#include <stdint.h>


// The heif_chroma enum describes pixel layout (planar vs interleaved, and any
// YUV chroma subsampling). It does NOT describe pixel semantics; the semantic
// interpretation of the planes comes from heif_colorspace.
//
// heif_chroma_planar covers "one or more planar components, no subsampling".
// It is used for:
//   - heif_colorspace_monochrome (single luma plane)
//   - heif_colorspace_filter_array (single CFA mosaic plane)
//   - heif_colorspace_custom (any number of planar components)
// For heif_colorspace_RGB the historical name heif_chroma_444 is the
// canonical form for planar RGB. heif_chroma_planar is accepted as a
// synonym at the C API boundary and is internally canonicalized to
// heif_chroma_444 so existing callers and internal code paths keep seeing
// heif_chroma_444 on read-back. Callers should also accept a returned
// heif_chroma_planar as a synonym. We might switch to this as the
// canonical name in the future.
//
// The YUV subsampling names (heif_chroma_420 / _422 / _444) are kept for
// heif_colorspace_YCbCr.
typedef enum heif_chroma
{
  heif_chroma_undefined = 99,
  heif_chroma_planar = 0,
  heif_chroma_420 = 1,
  heif_chroma_422 = 2,
  heif_chroma_444 = 3,
  heif_chroma_interleaved_RGB = 10,
  heif_chroma_interleaved_RGBA = 11,
  heif_chroma_interleaved_RRGGBB_BE = 12,   // HDR, big endian.
  heif_chroma_interleaved_RRGGBBAA_BE = 13, // HDR, big endian.
  heif_chroma_interleaved_RRGGBB_LE = 14,   // HDR, little endian.
  heif_chroma_interleaved_RRGGBBAA_LE = 15  // HDR, little endian.
} heif_chroma;

// DEPRECATED ENUM NAMES
#define heif_chroma_interleaved_24bit  heif_chroma_interleaved_RGB
#define heif_chroma_interleaved_32bit  heif_chroma_interleaved_RGBA

// Legacy alias for heif_chroma_planar. The name "monochrome" was misleading
// for non-grayscale planar layouts (filter_array, custom); use
// heif_chroma_planar in new code.
#define heif_chroma_monochrome heif_chroma_planar


typedef enum heif_colorspace
{
  heif_colorspace_undefined = 99,

  // heif_colorspace_YCbCr should be used with one of these heif_chroma values:
  // * heif_chroma_444
  // * heif_chroma_422
  // * heif_chroma_420
  heif_colorspace_YCbCr = 0,

  // heif_colorspace_RGB should be used with one of these heif_chroma values:
  // * heif_chroma_444 (for planar RGB)
  // * heif_chroma_planar is accepted as a synonym and is internally canonicalized to heif_chroma_444.
  //                      It is the preferred, future-proof value.
  // * heif_chroma_interleaved_RGB
  // * heif_chroma_interleaved_RGBA
  // * heif_chroma_interleaved_RRGGBB_BE
  // * heif_chroma_interleaved_RRGGBBAA_BE
  // * heif_chroma_interleaved_RRGGBB_LE
  // * heif_chroma_interleaved_RRGGBBAA_LE
  heif_colorspace_RGB = 1,

  // heif_colorspace_monochrome should only be used with heif_chroma = heif_chroma_planar.
  heif_colorspace_monochrome = 2,

  // Indicates that this image has a special, custom arrangement of components.
  // For example, it can have several monochrome channels or just a depth component with no color image.
  // Images of this type are always planar and use heif_chroma_planar.
  heif_colorspace_custom = 3,

  // Images of this type are filter-array (CFA / Bayer) mosaics. The single
  // mosaicked plane is described as heif_chroma_planar.
  heif_colorspace_filter_array = 4
} heif_colorspace;

#define heif_colorspace_nonvisual heif_colorspace_custom

typedef enum heif_channel
{
  heif_channel_Y = 0,
  heif_channel_Cb = 1,
  heif_channel_Cr = 2,
  heif_channel_R = 3,
  heif_channel_G = 4,
  heif_channel_B = 5,
  heif_channel_Alpha = 6,
  heif_channel_interleaved = 10,
  heif_channel_filter_array = 11,
  heif_channel_depth = 12,
  heif_channel_disparity = 13,
  heif_channel_unknown = 65535
} heif_channel;

// An heif_image contains a decoded pixel image in various colorspaces, chroma formats,
// and bit depths.

// Note: when converting images to an interleaved chroma format, the resulting
// image contains only a single channel of type channel_interleaved with, e.g., 3 bytes per pixel,
// containing the interleaved R,G,B values.

// Planar RGB images are specified as heif_colorspace_RGB / heif_chroma_444.
// heif_chroma_planar is accepted as a synonym and is internally canonicalized
// to heif_chroma_444 so heif_image_get_chroma_format() always returns
// heif_chroma_444 for planar RGB.

typedef struct heif_image heif_image;
typedef struct heif_image_handle heif_image_handle;
typedef struct heif_security_limits heif_security_limits;


// Get the colorspace format of the image.
LIBHEIF_API
heif_colorspace heif_image_get_colorspace(const heif_image*);

// Get the chroma format of the image.
LIBHEIF_API
heif_chroma heif_image_get_chroma_format(const heif_image*);

/**
 * Get the width of a specified image channel.
 *
 * @param img the image to get the width for
 * @param channel the channel to select
 * @return the width of the channel in pixels, or -1 the channel does not exist in the image
 */
LIBHEIF_API
int heif_image_get_width(const heif_image* img, heif_channel channel);

/**
 * Get the height of a specified image channel.
 *
 * @param img the image to get the height for
 * @param channel the channel to select
 * @return the height of the channel in pixels, or -1 the channel does not exist in the image
 */
LIBHEIF_API
int heif_image_get_height(const heif_image* img, heif_channel channel);

/**
 * Get the logical width of the image.
 *
 * For well-formed images this equals the size of the main channel (the Y channel
 * in YCbCr or mono, or the RGB channels). Subsampled chroma channels may be smaller.
 *
 * @param img the image to get the primary width for
 * @return the width in pixels
 */
LIBHEIF_API
int heif_image_get_primary_width(const heif_image* img);

/**
 * Get the logical height of the image.
 *
 * For well-formed images this equals the size of the main channel (the Y channel
 * in YCbCr or mono, or the RGB channels). Subsampled chroma channels may be smaller.
 *
 * @param img the image to get the primary height for
 * @return the height in pixels
 */
LIBHEIF_API
int heif_image_get_primary_height(const heif_image* img);

/**
 * Crop the image in place by removing margins from each edge.
 *
 * The four parameters specify the number of pixels to remove from each side,
 * not absolute pixel coordinates. For example, to crop a 100x100 image down
 * to its central 80x80 region, pass `left=10, right=10, top=10, bottom=10`.
 * Passing all zeros leaves the image unchanged.
 *
 * The resulting image has dimensions `(w - left - right) x (h - top - bottom)`,
 * which must be at least 1x1. All four values must be non-negative and the
 * sums `left + right` and `top + bottom` must each be strictly less than the
 * corresponding image dimension; otherwise `heif_error_Usage_error` /
 * `heif_suberror_Invalid_parameter_value` is returned and the image is left
 * unchanged.
 *
 * @param img    the image to crop (modified in place on success)
 * @param left   number of pixels to remove from the left edge
 * @param right  number of pixels to remove from the right edge
 * @param top    number of pixels to remove from the top edge
 * @param bottom number of pixels to remove from the bottom edge
 */
LIBHEIF_API
heif_error heif_image_crop(heif_image* img,
                           int left, int right, int top, int bottom);

LIBHEIF_API
heif_error heif_image_extract_area(const heif_image*,
                                   uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                                   const heif_security_limits* limits,
                                   heif_image** out_image);

// Get the number of bits per pixel in the given image channel. Returns -1 if
// a non-existing channel was given.
// Note that the number of bits per pixel may be different for each color channel.
// This function returns the number of bits used for storage of each pixel.
// Especially for HDR images, this is probably not what you want. Have a look at
// heif_image_get_bits_per_pixel_range() instead.
LIBHEIF_API
int heif_image_get_bits_per_pixel(const heif_image*, heif_channel channel);

// Get the number of bits per pixel in the given image channel. This function returns
// the number of bits used for representing the pixel value, which might be smaller
// than the number of bits used in memory.
// For example, in 12bit HDR images, this function returns '12', while still 16 bits
// are reserved for storage. For interleaved RGBA with 12 bit, this function also returns
// '12', not '48' or '64' (heif_image_get_bits_per_pixel returns 64 in this case).
LIBHEIF_API
int heif_image_get_bits_per_pixel_range(const heif_image*, heif_channel channel);

LIBHEIF_API
int heif_image_has_channel(const heif_image*, heif_channel channel);

// Get a pointer to the actual pixel data.
// The 'out_stride' is returned as "bytes per line".
// When out_stride is NULL, no value will be written.
// Returns NULL if a non-existing channel was given.
// Deprecated, use the safer version heif_image_get_plane_readonly2() instead.
LIBHEIF_API
const uint8_t* heif_image_get_plane_readonly(const heif_image*,
                                             heif_channel channel,
                                             int* out_stride);

// Deprecated, use the safer version heif_image_get_plane2() instead.
LIBHEIF_API
uint8_t* heif_image_get_plane(heif_image*,
                              heif_channel channel,
                              int* out_stride);

// These are safer variants of the two functions above.
// The 'stride' parameter is often multiplied by the image height in the client application.
// For very large images, this can lead to integer overflows and, consequently, illegal memory accesses.
// The changed 'stride' parameter type eliminates this common error.
LIBHEIF_API
const uint8_t* heif_image_get_plane_readonly2(const heif_image*,
                                              heif_channel channel,
                                              size_t* out_stride);

LIBHEIF_API
uint8_t* heif_image_get_plane2(heif_image*,
                               heif_channel channel,
                               size_t* out_stride);


typedef struct heif_scaling_options heif_scaling_options;

// Currently, heif_scaling_options is not defined yet. Pass a NULL pointer.
LIBHEIF_API
heif_error heif_image_scale_image(const heif_image* input,
                                  heif_image** output,
                                  int width, int height,
                                  const heif_scaling_options* options);

// Extends the image size to match the given size by extending the right and bottom borders.
// The border areas are filled with zero.
LIBHEIF_API
heif_error heif_image_extend_to_size_fill_with_zero(heif_image* image,
                                                    uint32_t width, uint32_t height);

// Fills the image decoding warnings into the provided 'out_warnings' array.
// The size of the array has to be provided in max_output_buffer_entries.
// If max_output_buffer_entries==0, the number of decoder warnings is returned.
// The function fills the warnings into the provided buffer, starting with 'first_warning_idx'.
// It returns the number of warnings filled into the buffer.
// Note: you can iterate through all warnings by using 'max_output_buffer_entries=1' and iterate 'first_warning_idx'.
LIBHEIF_API
int heif_image_get_decoding_warnings(heif_image* image,
                                     int first_warning_idx,
                                     heif_error* out_warnings,
                                     int max_output_buffer_entries);

// This function is only for decoder plugin implementors.
LIBHEIF_API
void heif_image_add_decoding_warning(heif_image* image,
                                     heif_error err);

// Release heif_image.
LIBHEIF_API
void heif_image_release(const heif_image*);

LIBHEIF_API
void heif_image_get_pixel_aspect_ratio(const heif_image*, uint32_t* aspect_h, uint32_t* aspect_v);

LIBHEIF_API
void heif_image_set_pixel_aspect_ratio(heif_image*, uint32_t aspect_h, uint32_t aspect_v);

LIBHEIF_API
void heif_image_handle_set_pixel_aspect_ratio(heif_image_handle*, uint32_t aspect_h, uint32_t aspect_v);

// --- heif_image allocation

/**
 * Create a new image of the specified resolution and colorspace.
 *
 * <p>This does not allocate memory for the image data. Use {@link heif_image_add_plane} to
 * add the corresponding planes to match the specified {@code colorspace} and {@code chroma}.
 *
 * @param width the width of the image in pixels
 * @param height the height of the image in pixels
 * @param colorspace the colorspace of the image
 * @param chroma the chroma of the image
 * @param out_image pointer to pointer of the resulting image
 * @return whether the creation succeeded or there was an error
*/
LIBHEIF_API
heif_error heif_image_create(int width, int height,
                             heif_colorspace colorspace,
                             heif_chroma chroma,
                             heif_image** out_image);

/**
 * Add an image plane to the image.
 *
 * <p>The image plane needs to match the colorspace and chroma of the image. Note
 * that this does not need to be a single "planar" format - interleaved pixel channels
 * can also be used if the chroma is interleaved.
 *
 * <p>The indicated bit_depth corresponds to the bit depth per channel. For example,
 * with an interleaved format like RRGGBB where each color is represented by 10 bits,
 * the {@code bit_depth} would be {@code 10} rather than {@code 30}.
 *
 * <p>For backward compatibility, one can also specify 24bits for RGB and 32bits for RGBA,
 * instead of the preferred 8 bits. However, this use is deprecated.
 *
 * @param image the parent image to add the channel plane to
 * @param channel the channel of the plane to add
 * @param width the width of the plane
 * @param height the height of the plane
 * @param bit_depth the bit depth per color channel
 * @return whether the addition succeeded or there was an error
 *
 * @note The width and height are usually the same as the parent image, but can be
 * less for subsampling.
 *
 * @note The specified width can differ from the row stride of the resulting image plane.
 * Always use the result of {@link heif_image_get_plane} or {@link heif_image_get_plane_readonly}
 * to determine row stride.
 */
LIBHEIF_API
heif_error heif_image_add_plane(heif_image* image,
                                heif_channel channel,
                                int width, int height, int bit_depth);

/*
 * The security limits should preferably be the limits from a heif_context.
 * The memory allocated will then be registered in the memory budget of that context.
 */
LIBHEIF_API
heif_error heif_image_add_plane_safe(heif_image* image,
                                     heif_channel channel,
                                     int width, int height, int bit_depth,
                                     const heif_security_limits* limits);

// Signal that the image is premultiplied by the alpha pixel values.
LIBHEIF_API
void heif_image_set_premultiplied_alpha(heif_image* image,
                                        int is_premultiplied_alpha);

LIBHEIF_API
int heif_image_is_premultiplied_alpha(heif_image* image);

// This function extends the padding of the image so that it has at least the given physical size.
// The padding border is filled with the pixels along the right/bottom border.
// This function may be useful if you want to process the image, but have some external padding requirements.
// The image size will not be modified if it is already larger/equal than the given physical size.
// I.e. you cannot assume that after calling this function, the stride will be equal to min_physical_width.
LIBHEIF_API
heif_error heif_image_extend_padding_to_size(heif_image* image,
                                             int min_physical_width, int min_physical_height);


#ifdef __cplusplus
}
#endif

#endif
