/*
 * OpenJPEG codec.
 * Copyright (c) 2023 Devon Sookhoo
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

#include "libheif/heif.h"
#include "libheif/heif_plugin.h"
#include "decoder_openjpeg.h"
#include <openjpeg.h>
#include <cstring>

#include <vector>
#include <cassert>
#include <memory>
#include <string>

static const int OPENJPEG_PLUGIN_PRIORITY = 100;
static const int OPENJPEG_PLUGIN_PRIORITY_HTJ2K = 90;

struct openjpeg_decoder
{
  std::vector<uint8_t> encoded_data;
  uintptr_t user_data;

  size_t read_position = 0;
  std::string error_message;
};


#define MAX_PLUGIN_NAME_LENGTH 80
static char plugin_name[MAX_PLUGIN_NAME_LENGTH];

static const char* openjpeg_plugin_name()
{
  snprintf(plugin_name, MAX_PLUGIN_NAME_LENGTH, "OpenJPEG %s", opj_version());
  plugin_name[MAX_PLUGIN_NAME_LENGTH - 1] = 0;

  return plugin_name;
}


static void openjpeg_init_plugin()
{
}


static void openjpeg_deinit_plugin()
{
}


static int openjpeg_does_support_format(heif_compression_format format)
{
  if (format == heif_compression_JPEG2000) {
    return OPENJPEG_PLUGIN_PRIORITY;
  }
  else if (format == heif_compression_HTJ2K) {
    return OPENJPEG_PLUGIN_PRIORITY_HTJ2K;
  }
  else {
    return 0;
  }
}

static int openjpeg_does_support_format2(const heif_decoder_plugin_compressed_format_description* format)
{
  return openjpeg_does_support_format(format->format);
}

heif_error openjpeg_new_decoder2(void** dec, const heif_decoder_plugin_options* options)
{
  openjpeg_decoder* decoder = new openjpeg_decoder();

  *dec = decoder;

  return heif_error_ok;
}

heif_error openjpeg_new_decoder(void** dec)
{
  heif_decoder_plugin_options options;
  options.format = heif_compression_JPEG2000;
  options.num_threads = 0;
  options.strict_decoding = false;

  return openjpeg_new_decoder2(dec, &options);
}

void openjpeg_free_decoder(void* decoder_raw)
{
  openjpeg_decoder* decoder = (openjpeg_decoder*) decoder_raw;

  if (!decoder) {
    return;
  }

  delete decoder;
}


void openjpeg_set_strict_decoding(void* decoder_raw, int flag)
{

}


heif_error openjpeg_push_data2(void* decoder_raw, const void* frame_data, size_t frame_size,
                               uintptr_t user_data)
{
  openjpeg_decoder* decoder = (openjpeg_decoder*) decoder_raw;
  const uint8_t* frame_data_src = (const uint8_t*) frame_data;

  decoder->encoded_data.insert(decoder->encoded_data.end(), frame_data_src, frame_data_src + frame_size);
  decoder->user_data = user_data;

  return heif_error_ok;
}

heif_error openjpeg_push_data(void* decoder_raw, const void* frame_data, size_t frame_size)
{
  return openjpeg_push_data2(decoder_raw, frame_data, frame_size, 0);
}

//**************************************************************************

//  This will read from our memory to the buffer.

static OPJ_SIZE_T opj_memory_stream_read(void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data)
{
  openjpeg_decoder* decoder = (openjpeg_decoder*) p_user_data; // Our data.
  size_t data_size = decoder->encoded_data.size();

  OPJ_SIZE_T l_nb_bytes_read = p_nb_bytes; // Amount to move to buffer.

  // Check if the current offset is outside our data buffer.

  if (decoder->read_position >= data_size) {
    return (OPJ_SIZE_T) -1;
  }

  // Check if we are reading more than we have.

  if (p_nb_bytes > (data_size - decoder->read_position)) {
    //Read all we have.
    l_nb_bytes_read = data_size - decoder->read_position;
  }

  // Copy the data to the internal buffer.

  memcpy(p_buffer, &(decoder->encoded_data[decoder->read_position]), l_nb_bytes_read);

  decoder->read_position += l_nb_bytes_read; // Update the pointer to the new location.

  return l_nb_bytes_read;
}


// This will write from the buffer to our memory.

static OPJ_SIZE_T opj_memory_stream_write(void* p_buffer, OPJ_SIZE_T p_nb_bytes, void* p_user_data)
{
  assert(false); // We should never need to write to the buffer.
  return 0;
}


// Moves the pointer forward, but never more than we have.

static OPJ_OFF_T opj_memory_stream_skip(OPJ_OFF_T p_nb_bytes, void* p_user_data)
{
  openjpeg_decoder* decoder = (openjpeg_decoder*) p_user_data; // Our data.
  size_t data_size = decoder->encoded_data.size();

  OPJ_SIZE_T l_nb_bytes;


  if (p_nb_bytes < 0) {
    //No skipping backwards.
    return -1;
  }

  l_nb_bytes = (OPJ_SIZE_T) p_nb_bytes; // Allowed because it is positive.

  // Do not allow jumping past the end.

  if (l_nb_bytes > data_size - decoder->read_position) {
    l_nb_bytes = data_size - decoder->read_position;//Jump the max.
  }

  // Make the jump.

  decoder->read_position += l_nb_bytes;

  // Return how far we jumped.

  return l_nb_bytes;
}


// Sets the pointer to anywhere in the memory.

static OPJ_BOOL opj_memory_stream_seek(OPJ_OFF_T p_nb_bytes, void* p_user_data)
{
  openjpeg_decoder* decoder = (openjpeg_decoder*) p_user_data; // Our data.
  size_t data_size = decoder->encoded_data.size();

  // No before the buffer.
  if (p_nb_bytes < 0)
    return OPJ_FALSE;

  // No after the buffer.
  if (p_nb_bytes > (OPJ_OFF_T) data_size)
    return OPJ_FALSE;

  // Move to new position.
  decoder->read_position = (OPJ_SIZE_T) p_nb_bytes;

  return OPJ_TRUE;
}

//The system needs a routine to do when finished, the name tells you what I want it to do.

static void opj_memory_stream_do_nothing(void* p_user_data)
{
  OPJ_ARG_NOT_USED(p_user_data);
}


// Create a stream to use memory as the input or output.

opj_stream_t* opj_stream_create_default_memory_stream(openjpeg_decoder* p_decoder, OPJ_BOOL p_is_read_stream)
{
  opj_stream_t* stream;

  if (!(stream = opj_stream_default_create(p_is_read_stream))) {
    return nullptr;
  }

  // Set how to work with the frame buffer.

  if (p_is_read_stream) {
    opj_stream_set_read_function(stream, opj_memory_stream_read);
  }
  else {
    opj_stream_set_write_function(stream, opj_memory_stream_write);
  }

  opj_stream_set_seek_function(stream, opj_memory_stream_seek);

  opj_stream_set_skip_function(stream, opj_memory_stream_skip);

  opj_stream_set_user_data(stream, p_decoder, opj_memory_stream_do_nothing);

  opj_stream_set_user_data_length(stream, p_decoder->encoded_data.size());

  return stream;
}


//**************************************************************************


heif_error openjpeg_decode_next_image2(void* decoder_raw, heif_image** out_img,
                                       uintptr_t* out_user_data,
                                       const heif_security_limits* limits)
{
  auto* decoder = (struct openjpeg_decoder*) decoder_raw;

  if (decoder->encoded_data.empty()) {
    *out_img = nullptr;
    return heif_error_ok;
  }


  OPJ_BOOL success;
  opj_dparameters_t decompression_parameters;
  std::unique_ptr<opj_codec_t, void (OPJ_CALLCONV *)(opj_codec_t*)> l_codec(opj_create_decompress(OPJ_CODEC_J2K),
                                                               opj_destroy_codec);

  // Initialize Decoder
  opj_set_default_decoder_parameters(&decompression_parameters);
  success = opj_setup_decoder(l_codec.get(), &decompression_parameters);
  if (!success) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "opj_setup_decoder()"};
  }


  // Create Input Stream

  OPJ_BOOL is_read_stream = true;
  std::unique_ptr<opj_stream_t, void (OPJ_CALLCONV *)(opj_stream_t*)> stream(opj_stream_create_default_memory_stream(decoder, is_read_stream),
                                                                opj_stream_destroy);


  // Read Codestream Header
  opj_image_t* image_ptr = nullptr;
  success = opj_read_header(stream.get(), l_codec.get(), &image_ptr);
  if (!success) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "opj_read_header()"};
  }

  std::unique_ptr<opj_image_t, void (OPJ_CALLCONV *)(opj_image_t*)> image(image_ptr, opj_image_destroy);

  if (image->numcomps != 3 && image->numcomps != 1) {
    //TODO - Handle other numbers of components
    return {heif_error_Unsupported_feature, heif_suberror_Unsupported_data_version, "Number of components must be 3 or 1"};
  }
  else if ((image->color_space != OPJ_CLRSPC_UNSPECIFIED) && (image->color_space != OPJ_CLRSPC_SRGB)) {
    //TODO - Handle other colorspaces
    return {heif_error_Unsupported_feature, heif_suberror_Unsupported_data_version, "Colorspace must be SRGB"};
  }

  const int width = (image->x1 - image->x0);
  const int height = (image->y1 - image->y0);


  /* Get the decoded image */
  success = opj_decode(l_codec.get(), stream.get(), image.get());
  if (!success) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "opj_decode()"};
  }


  success = opj_end_decompress(l_codec.get(), stream.get());
  if (!success) {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "opj_end_decompress()"};
  }


  heif_colorspace colorspace = heif_colorspace_YCbCr;
  heif_chroma chroma = heif_chroma_444; //heif_chroma_interleaved_RGB;

  std::vector<heif_channel> channels;

  if (image->numcomps == 1) {
    colorspace = heif_colorspace_monochrome;
    chroma = heif_chroma_monochrome;
    channels = {heif_channel_Y};
  }
  else if (image->numcomps == 3 &&
           image->comps[1].dx == 1 &&
           image->comps[1].dy == 1) {
    colorspace = heif_colorspace_YCbCr;
    chroma = heif_chroma_444;
    channels = {heif_channel_Y, heif_channel_Cb, heif_channel_Cr};
  }
  else if (image->numcomps == 3 &&
           image->comps[1].dx == 2 &&
           image->comps[1].dy == 1) {
    colorspace = heif_colorspace_YCbCr;
    chroma = heif_chroma_422;
    channels = {heif_channel_Y, heif_channel_Cb, heif_channel_Cr};
  }
  else if (image->numcomps == 3 &&
           image->comps[1].dx == 2 &&
           image->comps[1].dy == 2) {
    colorspace = heif_colorspace_YCbCr;
    chroma = heif_chroma_420;
    channels = {heif_channel_Y, heif_channel_Cb, heif_channel_Cr};
  }
  else {
    return {heif_error_Decoder_plugin_error, heif_suberror_Unspecified, "unsupported image format"};
  }


  heif_error error = heif_image_create(width, height, colorspace, chroma, out_img);
  if (error.code) {
    return error;
  }

  for (size_t c = 0; c < image->numcomps; c++) {
    const opj_image_comp_t& opj_comp = image->comps[c];

    int bit_depth = opj_comp.prec;
    int cwidth = opj_comp.w;
    int cheight = opj_comp.h;

    error = heif_image_add_plane_safe(*out_img, channels[c], cwidth, cheight, bit_depth, limits);
    if (error.code) {
      // copy error message to decoder object because heif_image will be released
      decoder->error_message = error.message;
      error.message = decoder->error_message.c_str();

      heif_image_release(*out_img);
      *out_img = nullptr;
      return error;
    }

    size_t stride = 0;
    uint8_t* p = heif_image_get_plane2(*out_img, channels[c], &stride);


    // TODO: a SIMD implementation to convert int32 to uint8 would speed this up
    // https://stackoverflow.com/questions/63774643/how-to-convert-uint32-to-uint8-using-simd-but-not-avx512

    if (bit_depth <= 8) {
      for (int y = 0; y < cheight; y++) {
        for (int x = 0; x < cwidth; x++) {
          p[y * stride + x] = (uint8_t) opj_comp.data[y * cwidth + x];
        }
      }
    }
    else {
      uint16_t* p16 = (uint16_t*)p;
      for (int y = 0; y < cheight; y++) {
        for (int x = 0; x < cwidth; x++) {
          p16[y * stride/2 + x] = (uint16_t) opj_comp.data[y * cwidth + x];
        }
      }
    }
  }

  if (out_user_data) {
    *out_user_data = decoder->user_data;
  }

  decoder->encoded_data.clear();
  decoder->read_position = 0;

  return heif_error_ok;
}

heif_error openjpeg_decode_next_image(void* decoder_raw, heif_image** out_img,
                                      const heif_security_limits* limits)
{
  return openjpeg_decode_next_image2(decoder_raw, out_img, nullptr, limits);
}

heif_error openjpeg_decode_image(void* decoder_raw, heif_image** out_img)
{
  auto* limits = heif_get_global_security_limits();
  return openjpeg_decode_next_image(decoder_raw, out_img, limits);
}

heif_error openjpeg_flush_data(void* decoder)
{
  return heif_error_ok;
}


static const heif_decoder_plugin decoder_openjpeg{
    5,
    openjpeg_plugin_name,
    openjpeg_init_plugin,
    openjpeg_deinit_plugin,
    openjpeg_does_support_format,
    openjpeg_new_decoder,
    openjpeg_free_decoder,
    openjpeg_push_data,
    openjpeg_decode_image,
    openjpeg_set_strict_decoding,
    "openjpeg",
    openjpeg_decode_next_image,
    /* minimum_required_libheif_version */ LIBHEIF_MAKE_VERSION(1,21,0),
    openjpeg_does_support_format2,
    openjpeg_new_decoder2,
    openjpeg_push_data2,
    openjpeg_flush_data,
    openjpeg_decode_next_image2
};

const heif_decoder_plugin* get_decoder_plugin_openjpeg()
{
  return &decoder_openjpeg;
}


#if PLUGIN_OPENJPEG_DECODER
heif_plugin_info plugin_info {
  1,
  heif_plugin_type_decoder,
  &decoder_openjpeg
};
#endif
