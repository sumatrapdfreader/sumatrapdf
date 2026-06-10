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

#ifndef LIBHEIF_HEIF_DECODING_H
#define LIBHEIF_HEIF_DECODING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_library.h>
#include <libheif/heif_image.h>
#include <libheif/heif_context.h>
#include <libheif/heif_color.h>


// If the maximum threads number is set to 0, the image tiles are decoded in the main thread.
// This is different from setting it to 1, which will generate a single background thread to decode the tiles.
// Note that this setting only affects libheif itself. The codecs itself may still use multi-threaded decoding.
// You can use it, for example, in cases where you are decoding several images in parallel anyway you thus want
// to minimize parallelism in each decoder.
LIBHEIF_API
void heif_context_set_max_decoding_threads(heif_context* ctx, int max_threads);

// Returns the current maximum number of background threads used for parallel tile decoding.
// This reflects either the library default or the value last passed to
// heif_context_set_max_decoding_threads().
// If 'ctx' is NULL, the library default is returned.
LIBHEIF_API
int heif_context_get_max_decoding_threads(const heif_context* ctx);

// Quick check whether there is a decoder available for the given format.
// Note that the decoder still may not be able to decode all variants of that format.
// You will have to query that further (todo) or just try to decode and check the returned error.
LIBHEIF_API
int heif_have_decoder_for_format(heif_compression_format format);


typedef enum heif_progress_step
{
  heif_progress_step_total = 0,
  heif_progress_step_load_tile = 1
} heif_progress_step;


typedef struct heif_decoding_options
{
  uint8_t version;

  // version 1 options

  // Ignore geometric transformations like cropping, rotation, mirroring.
  // Default: false (do not ignore).
  uint8_t ignore_transformations;

  // Any of the progress functions may be called from background threads.
  void (* start_progress)(heif_progress_step step, int max_progress, void* progress_user_data);

  void (* on_progress)(heif_progress_step step, int progress, void* progress_user_data);

  void (* end_progress)(heif_progress_step step, void* progress_user_data);

  void* progress_user_data;

  // version 2 options

  uint8_t convert_hdr_to_8bit;

  // version 3 options

  // When enabled, an error is returned for invalid input. Otherwise, it will try its best and
  // add decoding warnings to the decoded heif_image. Default is non-strict.
  uint8_t strict_decoding;

  // version 4 options

  // name_id of the decoder to use for the decoding.
  // If set to NULL (default), the highest priority decoder is chosen.
  // The priority is defined in the plugin.
  const char* decoder_id;

  // version 5 options

  heif_color_conversion_options color_conversion_options;

  // version 6 options

  int (* cancel_decoding)(void* progress_user_data);

  // version 7 options

  // When set to NULL, default options will be used
  heif_color_conversion_options_ext* color_conversion_options_ext;

  // version 8 options (v1.21.0)

  // If enabled, it will decode the media timeline, ignoring the sequence tracks edit-list.
  int ignore_sequence_editlist; // bool

  // Requested NCLX color profile of the decoded output image. If the input
  // image's NCLX differs, libheif will color-convert the pixels accordingly
  // (e.g. YCbCr matrix, primaries, range) so the result matches what is
  // requested here.
  //
  // When set to NULL, the behavior depends on the flag
  // output_image_nclx_profile_passthrough below: by default NULL means
  // "convert to sRGB"; with the passthrough flag enabled, NULL means
  // "keep the input image's NCLX".
  heif_color_profile_nclx* output_image_nclx_profile;

  int num_library_threads; // 0 = let libheif decide (TODO, currently ignored)
  int num_codec_threads; // 0 = use decoder default

  // version 9 options

  // If enabled, libheif will attempt to work around known broken-input quirks
  // (e.g. Sony HIF files where the NCLX `colr` box disagrees with the HEVC VUI
  // on the YCbCr range flag). Default: false (strict spec-conformant behavior).
  uint8_t autocorrect_broken_input;

  // version 10 options

  // Controls the meaning of output_image_nclx_profile == NULL.
  //
  // When false (default), a NULL output_image_nclx_profile means "convert the
  // decoded image to sRGB" (BT.709 primaries, sRGB transfer, BT.601 matrix,
  // full-range). For HDR inputs (e.g. BT.2100 PQ) this silently discards the
  // original color volume.
  //
  // When true, a NULL output_image_nclx_profile means "keep the input image's
  // NCLX". The decoded image carries the input file's primaries / transfer /
  // matrix / range, and no extra color-space conversion is performed solely
  // because the output NCLX was unspecified. If a YCbCr<->RGB colorspace
  // conversion fires for another reason, the input NCLX is used to drive that
  // conversion (so the result is tagged consistently with the source).
  //
  // Although this flag is off by default to preserve historical behavior, new
  // code that wants to preserve HDR through decode should generally enable it.
  // Setting output_image_nclx_profile to a non-NULL value overrides this flag.
  uint8_t output_image_nclx_profile_passthrough;
} heif_decoding_options;


// Allocate decoding options and fill with default values.
// Note: you should always get the decoding options through this function since the
// option structure may grow in size in future versions.
LIBHEIF_API
heif_decoding_options* heif_decoding_options_alloc(void);

LIBHEIF_API
void heif_decoding_options_copy(heif_decoding_options* dst,
                                const heif_decoding_options* src);

LIBHEIF_API
void heif_decoding_options_free(heif_decoding_options*);


typedef struct heif_decoder_descriptor heif_decoder_descriptor;

// Get a list of available decoders. You can filter the encoders by compression format.
// Use format_filter==heif_compression_undefined to get all available decoders.
// The returned list of decoders is sorted by their priority (which is a plugin property).
// The number of decoders is returned, which are not more than 'count' if (out_decoders != nullptr).
// By setting out_decoders==nullptr, you can query the number of decoders, 'count' is ignored.
LIBHEIF_API
int heif_get_decoder_descriptors(heif_compression_format format_filter,
                                 const heif_decoder_descriptor** out_decoders,
                                 int count);

// Return a long, descriptive name of the decoder (including version information).
LIBHEIF_API
const char* heif_decoder_descriptor_get_name(const heif_decoder_descriptor*);

// Return a short, symbolic name for identifying the decoder.
// This name should stay constant over different decoder versions.
// Note: the returned ID may be NULL for old plugins that don't support this yet.
LIBHEIF_API
const char* heif_decoder_descriptor_get_id_name(const heif_decoder_descriptor*);


// Decode an heif_image_handle into the actual pixel image and also carry out
// all geometric transformations specified in the HEIF file (rotation, cropping, mirroring).
//
// If colorspace or chroma is set to heif_colorspace_undefined or heif_chroma_undefined,
// respectively, the original colorspace is taken.
// Decoding options may be NULL. If you want to supply options, always use
// heif_decoding_options_alloc() to get the structure.
LIBHEIF_API
heif_error heif_decode_image(const heif_image_handle* in_handle,
                             heif_image** out_img,
                             heif_colorspace colorspace,
                             heif_chroma chroma,
                             const heif_decoding_options* options);

#ifdef __cplusplus
}
#endif

#endif
