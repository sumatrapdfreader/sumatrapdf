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

#ifndef LIBHEIF_HEIF_PLUGIN_H
#define LIBHEIF_HEIF_PLUGIN_H
#include "heif_sequences.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_encoding.h>


// ====================================================================================================
//  This file is for codec plugin developers only.
// ====================================================================================================

// API versions table
//
// release    decoder   encoder   enc.params
// -----------------------------------------
//  1.0          1        N/A        N/A
//  1.1          1         1          1
//  1.4          1         1          2
//  1.8          1         2          2
//  1.13         2         3          2
//  1.15         3         3          2
//  1.20         4         3          2
//  1.21         5         4          2

#define heif_decoder_plugin_latest_version 5
#define heif_encoder_plugin_latest_version 4

// The minimum plugin versions that can be used with this libheif version.
#define heif_decoder_plugin_minimum_version 5
#define heif_encoder_plugin_minimum_version 4

// ====================================================================================================
//  Decoder plugin API
//  In order to decode images in other formats than HEVC, additional compression codecs can be
//  added as plugins. A plugin has to implement the functions specified in heif_decoder_plugin
//  and the plugin has to be registered to the libheif library using heif_register_decoder().

typedef struct heif_decoder_plugin_compressed_format_description
{
  enum heif_compression_format format;

} heif_decoder_plugin_compressed_format_description;


typedef struct heif_decoder_plugin_options
{
  enum heif_compression_format format;
  int strict_decoding; // bool
  int num_threads; // 0 - undefined, use decoder default

} heif_decoder_plugin_options;


typedef struct heif_decoder_plugin
{
  // API version supported by this plugin (see table above for supported versions)
  int plugin_api_version;


  // --- version 1 functions ---

  // Human-readable name of the plugin
  const char* (* get_plugin_name)(void);

  // Global plugin initialization (may be NULL)
  void (* init_plugin)(void);

  // Global plugin deinitialization (may be NULL)
  void (* deinit_plugin)(void);

  // Query whether the plugin supports decoding of the given format
  // Result is a priority value. The plugin with the largest value wins.
  // Default priority is 100. Returning 0 indicates that the plugin cannot decode this format.
  int (* does_support_format)(enum heif_compression_format format);

  // Create a new decoder context for decoding an image
  heif_error (* new_decoder)(void** decoder);

  // Free the decoder context (heif_image can still be used after destruction)
  void (* free_decoder)(void* decoder);

  // Push more data into the decoder. This can be called multiple times.
  // This may not be called after any decode_*() function has been called.
  heif_error (* push_data)(void* decoder, const void* data, size_t size);


  // --- After pushing the data into the decoder, the decode functions may be called only once.

  heif_error (* decode_image)(void* decoder, heif_image** out_img);


  // --- version 2 functions ---

  void (* set_strict_decoding)(void* decoder, int flag);

  // If not NULL, this can provide a specialized function to convert YCbCr to sRGB, because
  // only the codec itself knows how to interpret the chroma samples and their locations.
  /*
  struct heif_error (*convert_YCbCr_to_sRGB)(void* decoder,
                                             struct heif_image* in_YCbCr_img,
                                             struct heif_image** out_sRGB_img);

  */

  // Reset decoder, such that we can feed in new data for another image.
  // void (*reset_image)(void* decoder);

  // --- version 3 functions ---

  const char* id_name;

  // --- version 4 functions ---

  heif_error (* decode_next_image)(void* decoder, heif_image** out_img,
                                   const heif_security_limits* limits);

  // --- version 5 functions will follow below ... ---

  uint32_t minimum_required_libheif_version;

  // Query whether the plugin supports decoding of the given format
  // Result is a priority value. The plugin with the largest value wins.
  // Default priority is 100. Returning 0 indicates that the plugin cannot decode this format.
  int (* does_support_format2)(const heif_decoder_plugin_compressed_format_description* format);

  // Create a new decoder context for decoding an image
  heif_error (* new_decoder2)(void** decoder, const heif_decoder_plugin_options*);

  heif_error (* push_data2)(void* decoder, const void* data, size_t size, uintptr_t user_data);

  heif_error (* flush_data)(void* decoder);

  heif_error (* decode_next_image2)(void* decoder, heif_image** out_img,
                                    uintptr_t* out_user_data,
                                    const heif_security_limits* limits);

  // --- Note: when adding new versions, also update `heif_decoder_plugin_latest_version`.
} heif_decoder_plugin;


enum heif_encoded_data_type
{
  heif_encoded_data_type_HEVC_header = 1,
  heif_encoded_data_type_HEVC_image = 2,
  heif_encoded_data_type_HEVC_depth_SEI = 3
};


// Specifies the class of the input image content.
// The encoder may want to encode different classes with different parameters
// (e.g. always encode alpha lossless)
enum heif_image_input_class
{
  heif_image_input_class_normal = 1,
  heif_image_input_class_alpha = 2,
  heif_image_input_class_depth = 3,
  heif_image_input_class_thumbnail = 4
};


typedef struct heif_encoder_plugin
{
  // API version supported by this plugin (see table above for supported versions)
  int plugin_api_version;


  // --- version 1 functions ---

  // The compression format generated by this plugin.
  enum heif_compression_format compression_format;

  // Short name of the encoder that can be used as command line parameter when selecting an encoder.
  // Hence, it should stay stable and not contain any version numbers that will change.
  const char* id_name;

  // Default priority is 100.
  int priority;


  // Feature support
  int supports_lossy_compression;
  int supports_lossless_compression;


  // Human-readable name of the plugin
  const char* (* get_plugin_name)(void);

  // Global plugin initialization (may be NULL)
  void (* init_plugin)(void);

  // Global plugin cleanup (may be NULL).
  // Free data that was allocated in init_plugin()
  void (* cleanup_plugin)(void);

  // Create a new decoder context for decoding an image
  heif_error (* new_encoder)(void** encoder);

  // Free the decoder context (heif_image can still be used after destruction)
  void (* free_encoder)(void* encoder);

  heif_error (* set_parameter_quality)(void* encoder, int quality);

  heif_error (* get_parameter_quality)(void* encoder, int* quality);

  heif_error (* set_parameter_lossless)(void* encoder, int lossless);

  heif_error (* get_parameter_lossless)(void* encoder, int* lossless);

  heif_error (* set_parameter_logging_level)(void* encoder, int logging);

  heif_error (* get_parameter_logging_level)(void* encoder, int* logging);

  const heif_encoder_parameter** (* list_parameters)(void* encoder);

  heif_error (* set_parameter_integer)(void* encoder, const char* name, int value);

  heif_error (* get_parameter_integer)(void* encoder, const char* name, int* value);

  heif_error (* set_parameter_boolean)(void* encoder, const char* name, int value);

  heif_error (* get_parameter_boolean)(void* encoder, const char* name, int* value);

  heif_error (* set_parameter_string)(void* encoder, const char* name, const char* value);

  heif_error (* get_parameter_string)(void* encoder, const char* name, char* value, int value_size);

  // Replace the input colorspace/chroma with the one that is supported by the encoder and that
  // comes as close to the input colorspace/chroma as possible.
  void (* query_input_colorspace)(enum heif_colorspace* inout_colorspace,
                                  enum heif_chroma* inout_chroma);

  // Encode an image.
  // After pushing an image into the encoder, you should call get_compressed_data() to
  // get compressed data until it returns a NULL data pointer.
  heif_error (* encode_image)(void* encoder, const heif_image* image,
                              enum heif_image_input_class image_class);

  // Get a packet of decoded data. The data format depends on the codec.
  // For HEVC, each packet shall contain exactly one NAL, starting with the NAL header without startcode.
  heif_error (* get_compressed_data)(void* encoder, uint8_t** data, int* size,
                                            enum heif_encoded_data_type* type);


  // --- version 2 ---

  void (* query_input_colorspace2)(void* encoder,
                                   enum heif_colorspace* inout_colorspace,
                                   enum heif_chroma* inout_chroma);

  // --- version 3 ---

  // The encoded image size may be different from the input frame size, e.g. because
  // of required rounding, or a required minimum size. Use this function to return
  // the encoded size for a given input image size.
  // You may set this to NULL if no padding is required for any image size.
  void (* query_encoded_size)(void* encoder, uint32_t input_width, uint32_t input_height,
                              uint32_t* encoded_width, uint32_t* encoded_height);

  // --- version 4 ---

  uint32_t minimum_required_libheif_version;

  heif_error (* start_sequence_encoding)(void* encoder, const heif_image* image,
                                         enum heif_image_input_class image_class,
                                         uint32_t framerate_num, uint32_t framerate_denom,
                                         const heif_sequence_encoding_options* options);
  // TODO: is heif_sequence_encoding_options a good choice here?

  heif_error (* encode_sequence_frame)(void* encoder, const heif_image* image, uintptr_t frame_nr);

  heif_error (* end_sequence_encoding)(void* encoder);

  heif_error (* get_compressed_data2)(void* encoder, uint8_t** data, int* size,
                                      uintptr_t* frame_nr,
                                      int* is_keyframe, int* more_frame_packets);

  int does_indicate_keyframes;

  // --- version 5 functions will follow below ... ---

  // --- Note: when adding new versions, also update `heif_encoder_plugin_latest_version`.
} heif_encoder_plugin;


// Names for standard parameters. These should only be used by the encoder plugins.
#define heif_encoder_parameter_name_quality  "quality"
#define heif_encoder_parameter_name_lossless "lossless"

// For use only by the encoder plugins.
// Application programs should use the access functions.
// NOLINTNEXTLINE(clang-analyzer-optin.performance.Padding)
typedef struct heif_encoder_parameter
{
  int version; // current version: 2

  // --- version 1 fields ---

  const char* name;
  enum heif_encoder_parameter_type type;

  union
  {
    struct
    {
      int default_value;

      uint8_t have_minimum_maximum; // bool
      int minimum;
      int maximum;

      int* valid_values;
      int num_valid_values;
    } integer;

    struct
    {
      const char* default_value;

      const char* const* valid_values;
    } string; // NOLINT

    struct
    {
      int default_value;
    } boolean;
  };

  // --- version 2 fields

  int has_default;
} heif_encoder_parameter;


extern heif_error heif_error_ok;
extern heif_error heif_error_unsupported_parameter;
extern heif_error heif_error_invalid_parameter_value;

#define HEIF_WARN_OR_FAIL(strict, image, cmd, cleanupBlock) \
{ heif_error e = cmd;                         \
  if (e.code != heif_error_Ok) {              \
    if (strict) {                             \
      cleanupBlock                            \
      return e;                               \
    }                                         \
    else {                                    \
      heif_image_add_decoding_warning(image, e); \
    }                                         \
  }                                           \
}
#ifdef __cplusplus
}
#endif

#endif
