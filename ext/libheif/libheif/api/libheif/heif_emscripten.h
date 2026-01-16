#ifndef LIBHEIF_BOX_EMSCRIPTEN_H
#define LIBHEIF_BOX_EMSCRIPTEN_H

#include <emscripten/bind.h>
#include <emscripten/version.h>

#include <memory>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <string.h>
#include <cassert>

#include "heif.h"
#include "heif_items.h"

static std::string heif_js_get_version()
{
  return heif_get_version();
}

static struct heif_error heif_js_context_read_from_memory(
    struct heif_context* context, const std::string& data)
{
  return heif_context_read_from_memory(context, data.data(), data.size(), nullptr);
}

static heif_filetype_result heif_js_check_filetype(const std::string& data) 
{
  return heif_check_filetype((const uint8_t*) data.data(), data.size());
}

static emscripten::val heif_js_context_get_image_handle(
    struct heif_context* context, heif_item_id id)
{
  emscripten::val result = emscripten::val::object();
  if (!context) {
    return result;
  }

  struct heif_image_handle* handle;
  struct heif_error err = heif_context_get_image_handle(context, id, &handle);
  if (err.code != heif_error_Ok) {
    return emscripten::val(err);
  }

#if __EMSCRIPTEN_major__ > 4 ||   \
    (__EMSCRIPTEN_major__ == 4 && \
     (__EMSCRIPTEN_minor__ > 0 || __EMSCRIPTEN_tiny__ >= 9))
  return emscripten::val(handle, emscripten::allow_raw_pointers());
#else
  return emscripten::val(handle);
#endif
}

static emscripten::val heif_js_context_get_primary_image_handle(
    struct heif_context* context)
{
  emscripten::val result = emscripten::val::object();
  if (!context) {
    return result;
  }
  
  heif_image_handle* handle;
  struct heif_error err = heif_context_get_primary_image_handle(context, &handle);

  if (err.code != heif_error_Ok) {
    return emscripten::val(err);
  }

#if __EMSCRIPTEN_major__ > 4 ||   \
    (__EMSCRIPTEN_major__ == 4 && \
     (__EMSCRIPTEN_minor__ > 0 || __EMSCRIPTEN_tiny__ >= 9))
  return emscripten::val(handle, emscripten::allow_raw_pointers());
#else
  return emscripten::val(handle);
#endif
}


static emscripten::val heif_js_context_get_list_of_top_level_image_IDs(
    struct heif_context* context)
{
  emscripten::val result = emscripten::val::array();
  if (!context) {
    return result;
  }

  int count = heif_context_get_number_of_top_level_images(context);
  if (count <= 0) {
    return result;
  }

  heif_item_id* ids = (heif_item_id*) alloca(count * sizeof(heif_item_id));
  if (!ids) {
    struct heif_error err;
    err.code = heif_error_Memory_allocation_error;
    err.subcode = heif_suberror_Security_limit_exceeded;
    return emscripten::val(err);
  }

  int received = heif_context_get_list_of_top_level_image_IDs(context, ids, count);
  if (!received) {
    free(ids);
    return result;
  }

  for (int i = 0; i < received; i++) {
    result.set(i, ids[i]);
  }
  return result;
}


static emscripten::val heif_js_context_get_list_of_item_IDs(
    struct heif_context* context)
{
  emscripten::val result = emscripten::val::array();
  if (!context) {
    return result;
  }

  int count = heif_context_get_number_of_items(context);
  if (count <= 0) {
    return result;
  }

  heif_item_id* ids = (heif_item_id*) alloca(count * sizeof(heif_item_id));
  if (!ids) {
    struct heif_error err;
    err.code = heif_error_Memory_allocation_error;
    err.subcode = heif_suberror_Security_limit_exceeded;
    return emscripten::val(err);
  }

  int num_ids_received = heif_context_get_list_of_item_IDs(context, ids, count);

  for (int i = 0; i < num_ids_received; i++) {
    result.set(i, ids[i]);
  }

  return result;
}


static emscripten::val heif_js_item_get_item_type(
  const struct heif_context* ctx, heif_item_id id)
{
  uint32_t type = heif_item_get_item_type(ctx, id);
  std::string type_string = fourcc_to_string(type);
  return emscripten::val(type_string);
}


static emscripten::val heif_js_item_get_mime_item_content_type(
  const struct heif_context* ctx, heif_item_id id)
{
  std::string content_type = "";
  const char* cstring = heif_item_get_mime_item_content_type(ctx, id);
  if (cstring) {
    content_type = cstring;
  }
  return emscripten::val(content_type);
}


static emscripten::val heif_js_item_get_mime_item_content_encoding(
  const struct heif_context* ctx, heif_item_id id)
{
  std::string content_encoding = "";
  const char* cstring = heif_item_get_mime_item_content_encoding(ctx, id);
  if (cstring) {
    content_encoding = cstring;
  }
  return emscripten::val(content_encoding);
}


static emscripten::val heif_js_item_get_uri_item_uri_type(
  const struct heif_context* ctx, heif_item_id id)
{
  std::string uri_type = "";
  const char* cstring = heif_item_get_uri_item_uri_type(ctx, id);
  if (cstring) {
    uri_type = cstring;
  }
  return emscripten::val(uri_type);
}


static emscripten::val heif_js_item_get_item_name(
  const struct heif_context* ctx, heif_item_id id)
{
  std::string item_name = "";
  const char* cstring = heif_item_get_item_name(ctx, id);
  if (cstring) {
    item_name = cstring;
  }
  return emscripten::val(item_name);
}


#if 0
static void strided_copy(void* dest, const void* src, int width, int height,
                         int stride)
{
  if (width == stride) {
    memcpy(dest, src, width * height);
  }
  else {
    const uint8_t* _src = static_cast<const uint8_t*>(src);
    uint8_t* _dest = static_cast<uint8_t*>(dest);
    for (int y = 0; y < height; y++, _dest += width, _src += stride) {
      memcpy(_dest, _src, width);
    }
  }
}

static int round_odd(int v) {
  return (int) ((v / 2.0) + 0.5);
}

static emscripten::val heif_js_decode_image(struct heif_image_handle* handle,
                                            enum heif_colorspace colorspace, enum heif_chroma chroma)
{
  emscripten::val result = emscripten::val::object();
  if (!handle) {
    return result;
  }

  struct heif_image* image;
  struct heif_error err = heif_decode_image(handle, &image, colorspace, chroma, nullptr);
  if (err.code != heif_error_Ok) {
    return emscripten::val(err);
  }

  result.set("is_primary", heif_image_handle_is_primary_image(handle));
  result.set("thumbnails", heif_image_handle_get_number_of_thumbnails(handle));
  int width = heif_image_handle_get_width(handle);
  result.set("width", width);
  int height = heif_image_handle_get_height(handle);
  result.set("height", height);
  std::vector<unsigned char> data;
  result.set("chroma", heif_image_get_chroma_format(image));
  result.set("colorspace", heif_image_get_colorspace(image));
  switch (heif_image_get_colorspace(image)) {
    case heif_colorspace_YCbCr: {
      size_t stride_y;
      const uint8_t* plane_y = heif_image_get_plane_readonly2(image,
                                                              heif_channel_Y, &stride_y);
      size_t stride_u;
      const uint8_t* plane_u = heif_image_get_plane_readonly2(image,
                                                              heif_channel_Cb, &stride_u);
      size_t stride_v;
      const uint8_t* plane_v = heif_image_get_plane_readonly2(image,
                                                              heif_channel_Cr, &stride_v);
      data.resize((width * height) + (2 * round_odd(width) * round_odd(height)));
      unsigned char* dest = const_cast<unsigned char*>(data.data());
      strided_copy(dest, plane_y, width, height, stride_y);
      strided_copy(dest + (width * height), plane_u,
                   round_odd(width), round_odd(height), stride_u);
      strided_copy(dest + (width * height) + (round_odd(width) * round_odd(height)),
                   plane_v, round_odd(width), round_odd(height), stride_v);
    }
      break;
    case heif_colorspace_RGB: {
      if(heif_image_get_chroma_format(image) == heif_chroma_interleaved_RGB) {
        size_t stride_rgb;
        const uint8_t* plane_rgb = heif_image_get_plane_readonly2(image,
                                                                  heif_channel_interleaved, &stride_rgb);
        data.resize(width * height * 3);
        unsigned char* dest = const_cast<unsigned char*>(data.data());
        strided_copy(dest, plane_rgb, width * 3, height, stride_rgb);
      }
      else if (heif_image_get_chroma_format(image) == heif_chroma_interleaved_RGBA) {
        size_t stride_rgba;
        const uint8_t* plane_rgba = heif_image_get_plane_readonly2(image,
                                                                   heif_channel_interleaved, &stride_rgba);
        data.resize(width * height * 4);
        unsigned char* dest = const_cast<unsigned char*>(data.data());
        strided_copy(dest, plane_rgba, width * 4, height, stride_rgba);
      }
      else {
        assert(false);
      }
    }
      break;
    case heif_colorspace_monochrome: {
      assert(heif_image_get_chroma_format(image) ==
             heif_chroma_monochrome);
      size_t stride_grey;
      const uint8_t* plane_grey = heif_image_get_plane_readonly2(image,
                                                                 heif_channel_Y, &stride_grey);
      data.resize(width * height);
      unsigned char* dest = const_cast<unsigned char*>(data.data());
      strided_copy(dest, plane_grey, width, height, stride_grey);
    }
      break;
    default:
      // Should never reach here.
      break;
  }
  result.set("data", std::move(data));

  if (heif_image_has_channel(image, heif_channel_Alpha)) {
    std::vector<unsigned char> alpha;
    size_t stride_alpha;
    const uint8_t* plane_alpha = heif_image_get_plane_readonly2(image, heif_channel_Alpha, &stride_alpha);
    alpha.resize(width * height);
    unsigned char* dest = const_cast<unsigned char*>(alpha.data());
    strided_copy(dest, plane_alpha, width, height, stride_alpha);
    result.set("alpha", std::move(alpha));
  }

  heif_image_release(image);
  return result;
}
#endif

/*
 * The returned object includes a pointer to an heif_image in the property "image".
 * This image has to be released after the image data has been read (copied) with heif_image_release().
 */
static emscripten::val heif_js_decode_image2(struct heif_image_handle* handle,
                                             enum heif_colorspace colorspace, enum heif_chroma chroma)
{
  emscripten::val result = emscripten::val::object();
  if (!handle) {
    return result;
  }

  struct heif_image* image;
  struct heif_error err = heif_decode_image(handle, &image, colorspace, chroma, nullptr);
  if (err.code != heif_error_Ok) {
    return emscripten::val(err);
  }

#if __EMSCRIPTEN_major__ > 4 ||   \
    (__EMSCRIPTEN_major__ == 4 && \
     (__EMSCRIPTEN_minor__ > 0 || __EMSCRIPTEN_tiny__ >= 9))
  result.set("image", image, emscripten::allow_raw_pointers());
#else
  result.set("image", image);
#endif

  int width = heif_image_handle_get_width(handle);
  result.set("width", width);

  int height = heif_image_handle_get_height(handle);
  result.set("height", height);

  std::vector<unsigned char> data;
  result.set("chroma", heif_image_get_chroma_format(image));
  result.set("colorspace", heif_image_get_colorspace(image));

  std::vector<heif_channel> channels {
    heif_channel_Y,
    heif_channel_Cb,
    heif_channel_Cr,
    heif_channel_R,
    heif_channel_G,
    heif_channel_B,
    heif_channel_Alpha,
    heif_channel_interleaved
  };

  emscripten::val val_channels = emscripten::val::array();

  for (auto channel : channels) {
    if (heif_image_has_channel(image, channel)) {
      emscripten::val val_channel_info = emscripten::val::object();
      val_channel_info.set("id", channel);

      size_t stride;
      const uint8_t* plane = heif_image_get_plane_readonly2(image, channel, &stride);

      val_channel_info.set("stride", stride);
      val_channel_info.set("data", emscripten::val(emscripten::typed_memory_view(stride * height, plane)));

      val_channel_info.set("width", heif_image_get_width(image, channel));
      val_channel_info.set("height", heif_image_get_height(image, channel));

      val_channel_info.set("bits_per_pixel", heif_image_get_bits_per_pixel_range(image, channel));

      val_channels.call<void>("push", val_channel_info);
    }
  }

  result.set("channels", val_channels);

  return result;
}


#define EXPORT_HEIF_FUNCTION(name) \
  emscripten::function(#name, &name, emscripten::allow_raw_pointers())

EMSCRIPTEN_BINDINGS(libheif) {
  
    // heif.h
    emscripten::function("heif_get_version", &heif_js_get_version, emscripten::allow_raw_pointers());
    emscripten::function("heif_context_read_from_memory", &heif_js_context_read_from_memory, emscripten::allow_raw_pointers());
    emscripten::function("heif_check_filetype", &heif_js_check_filetype, emscripten::allow_raw_pointers());
    emscripten::function("heif_context_get_list_of_top_level_image_IDs", &heif_js_context_get_list_of_top_level_image_IDs, emscripten::allow_raw_pointers());
    emscripten::function("heif_context_get_image_handle", &heif_js_context_get_image_handle, emscripten::allow_raw_pointers());
    emscripten::function("heif_context_get_primary_image_handle", &heif_js_context_get_primary_image_handle, emscripten::allow_raw_pointers());
    emscripten::function("heif_js_decode_image2", &heif_js_decode_image2, emscripten::allow_raw_pointers());
    EXPORT_HEIF_FUNCTION(heif_get_version_number);
    EXPORT_HEIF_FUNCTION(heif_context_alloc);
    EXPORT_HEIF_FUNCTION(heif_context_free);
    EXPORT_HEIF_FUNCTION(heif_context_get_number_of_top_level_images);
    EXPORT_HEIF_FUNCTION(heif_image_handle_release);
    EXPORT_HEIF_FUNCTION(heif_image_handle_get_width);
    EXPORT_HEIF_FUNCTION(heif_image_handle_get_height);
    EXPORT_HEIF_FUNCTION(heif_image_handle_is_primary_image);
    EXPORT_HEIF_FUNCTION(heif_image_release);
    EXPORT_HEIF_FUNCTION(heif_image_handle_has_alpha_channel);
    EXPORT_HEIF_FUNCTION(heif_image_handle_is_premultiplied_alpha);

    // heif_items.h
    emscripten::function("heif_context_get_list_of_item_IDs", &heif_js_context_get_list_of_item_IDs, emscripten::allow_raw_pointers());
    emscripten::function("heif_item_get_item_type", heif_js_item_get_item_type, emscripten::allow_raw_pointers());
    emscripten::function("heif_item_get_mime_item_content_type", heif_js_item_get_mime_item_content_type, emscripten::allow_raw_pointers());
    emscripten::function("heif_item_get_mime_item_content_encoding", heif_js_item_get_mime_item_content_encoding, emscripten::allow_raw_pointers());
    emscripten::function("heif_item_get_uri_item_uri_type", heif_js_item_get_uri_item_uri_type, emscripten::allow_raw_pointers());
    emscripten::function("heif_item_get_item_name", heif_js_item_get_item_name, emscripten::allow_raw_pointers());
    EXPORT_HEIF_FUNCTION(heif_context_get_number_of_items);
    EXPORT_HEIF_FUNCTION(heif_item_is_item_hidden);

    // DEPRECATED, use functions without the 'js' prefix.
    emscripten::function("heif_js_check_filetype", &heif_js_check_filetype, emscripten::allow_raw_pointers());
    emscripten::function("heif_js_context_get_list_of_top_level_image_IDs", &heif_js_context_get_list_of_top_level_image_IDs, emscripten::allow_raw_pointers());
    emscripten::function("heif_js_context_get_image_handle", &heif_js_context_get_image_handle, emscripten::allow_raw_pointers());
    emscripten::function("heif_js_context_get_primary_image_handle", &heif_js_context_get_primary_image_handle, emscripten::allow_raw_pointers());

    emscripten::enum_<heif_error_code>("heif_error_code")
    .value("heif_error_Ok", heif_error_Ok)
    .value("heif_error_Input_does_not_exist", heif_error_Input_does_not_exist)
    .value("heif_error_Invalid_input", heif_error_Invalid_input)
    .value("heif_error_Plugin_loading_error", heif_error_Plugin_loading_error)
    .value("heif_error_Unsupported_filetype", heif_error_Unsupported_filetype)
    .value("heif_error_Unsupported_feature", heif_error_Unsupported_feature)
    .value("heif_error_Usage_error", heif_error_Usage_error)
    .value("heif_error_Memory_allocation_error", heif_error_Memory_allocation_error)
    .value("heif_error_Decoder_plugin_error", heif_error_Decoder_plugin_error)
    .value("heif_error_Encoder_plugin_error", heif_error_Encoder_plugin_error)
    .value("heif_error_Encoding_error", heif_error_Encoding_error)
    .value("heif_error_End_of_sequence", heif_error_End_of_sequence)
    .value("heif_error_Color_profile_does_not_exist", heif_error_Color_profile_does_not_exist)
    .value("heif_error_Canceled", heif_error_Canceled);
    emscripten::enum_<heif_suberror_code>("heif_suberror_code")
    .value("heif_suberror_Unspecified", heif_suberror_Unspecified)
    .value("heif_suberror_Cannot_write_output_data", heif_suberror_Cannot_write_output_data)
    .value("heif_suberror_Compression_initialisation_error", heif_suberror_Compression_initialisation_error)
    .value("heif_suberror_Decompression_invalid_data", heif_suberror_Decompression_invalid_data)
    .value("heif_suberror_Encoder_initialization", heif_suberror_Encoder_initialization)
    .value("heif_suberror_Encoder_encoding", heif_suberror_Encoder_encoding)
    .value("heif_suberror_Encoder_cleanup", heif_suberror_Encoder_cleanup)
    .value("heif_suberror_Too_many_regions", heif_suberror_Too_many_regions)
    .value("heif_suberror_End_of_data", heif_suberror_End_of_data)
    .value("heif_suberror_Invalid_box_size", heif_suberror_Invalid_box_size)
    .value("heif_suberror_No_ftyp_box", heif_suberror_No_ftyp_box)
    .value("heif_suberror_No_idat_box", heif_suberror_No_idat_box)
    .value("heif_suberror_No_meta_box", heif_suberror_No_meta_box)
    .value("heif_suberror_No_moov_box", heif_suberror_No_moov_box)
    .value("heif_suberror_No_hdlr_box", heif_suberror_No_hdlr_box)
    .value("heif_suberror_No_hvcC_box", heif_suberror_No_hvcC_box)
    .value("heif_suberror_No_vvcC_box", heif_suberror_No_vvcC_box)
    .value("heif_suberror_No_pitm_box", heif_suberror_No_pitm_box)
    .value("heif_suberror_No_ipco_box", heif_suberror_No_ipco_box)
    .value("heif_suberror_No_ipma_box", heif_suberror_No_ipma_box)
    .value("heif_suberror_No_iloc_box", heif_suberror_No_iloc_box)
    .value("heif_suberror_No_iinf_box", heif_suberror_No_iinf_box)
    .value("heif_suberror_No_iprp_box", heif_suberror_No_iprp_box)
    .value("heif_suberror_No_iref_box", heif_suberror_No_iref_box)
    .value("heif_suberror_No_pict_handler", heif_suberror_No_pict_handler)
    .value("heif_suberror_Ipma_box_references_nonexisting_property", heif_suberror_Ipma_box_references_nonexisting_property)
    .value("heif_suberror_No_properties_assigned_to_item", heif_suberror_No_properties_assigned_to_item)
    .value("heif_suberror_No_item_data", heif_suberror_No_item_data)
    .value("heif_suberror_Invalid_grid_data", heif_suberror_Invalid_grid_data)
    .value("heif_suberror_Missing_grid_images", heif_suberror_Missing_grid_images)
    .value("heif_suberror_No_av1C_box", heif_suberror_No_av1C_box)
    .value("heif_suberror_No_avcC_box", heif_suberror_No_avcC_box)
    .value("heif_suberror_Invalid_mini_box", heif_suberror_Invalid_mini_box)
    .value("heif_suberror_Invalid_clean_aperture", heif_suberror_Invalid_clean_aperture)
    .value("heif_suberror_Invalid_overlay_data", heif_suberror_Invalid_overlay_data)
    .value("heif_suberror_Overlay_image_outside_of_canvas", heif_suberror_Overlay_image_outside_of_canvas)
    .value("heif_suberror_Plugin_is_not_loaded", heif_suberror_Plugin_is_not_loaded)
    .value("heif_suberror_Plugin_loading_error", heif_suberror_Plugin_loading_error)
    .value("heif_suberror_Auxiliary_image_type_unspecified", heif_suberror_Auxiliary_image_type_unspecified)
    .value("heif_suberror_Cannot_read_plugin_directory", heif_suberror_Cannot_read_plugin_directory)
    .value("heif_suberror_No_matching_decoder_installed", heif_suberror_No_matching_decoder_installed)
    .value("heif_suberror_No_or_invalid_primary_item", heif_suberror_No_or_invalid_primary_item)
    .value("heif_suberror_No_infe_box", heif_suberror_No_infe_box)
    .value("heif_suberror_Security_limit_exceeded", heif_suberror_Security_limit_exceeded)
    .value("heif_suberror_Unknown_color_profile_type", heif_suberror_Unknown_color_profile_type)
    .value("heif_suberror_Wrong_tile_image_chroma_format", heif_suberror_Wrong_tile_image_chroma_format)
    .value("heif_suberror_Invalid_fractional_number", heif_suberror_Invalid_fractional_number)
    .value("heif_suberror_Invalid_image_size", heif_suberror_Invalid_image_size)
    .value("heif_suberror_Nonexisting_item_referenced", heif_suberror_Nonexisting_item_referenced)
    .value("heif_suberror_Null_pointer_argument", heif_suberror_Null_pointer_argument)
    .value("heif_suberror_Nonexisting_image_channel_referenced", heif_suberror_Nonexisting_image_channel_referenced)
    .value("heif_suberror_Unsupported_plugin_version", heif_suberror_Unsupported_plugin_version)
    .value("heif_suberror_Unsupported_writer_version", heif_suberror_Unsupported_writer_version)
    .value("heif_suberror_Unsupported_parameter", heif_suberror_Unsupported_parameter)
    .value("heif_suberror_Invalid_parameter_value", heif_suberror_Invalid_parameter_value)
    .value("heif_suberror_Invalid_property", heif_suberror_Invalid_property)
    .value("heif_suberror_Item_reference_cycle", heif_suberror_Item_reference_cycle)
    .value("heif_suberror_Invalid_pixi_box", heif_suberror_Invalid_pixi_box)
    .value("heif_suberror_Invalid_region_data", heif_suberror_Invalid_region_data)
    .value("heif_suberror_Unsupported_codec", heif_suberror_Unsupported_codec)
    .value("heif_suberror_Unsupported_image_type", heif_suberror_Unsupported_image_type)
    .value("heif_suberror_Unsupported_data_version", heif_suberror_Unsupported_data_version)
    .value("heif_suberror_Unsupported_generic_compression_method", heif_suberror_Unsupported_generic_compression_method)
    .value("heif_suberror_Unsupported_essential_property", heif_suberror_Unsupported_essential_property)
    .value("heif_suberror_Unsupported_color_conversion", heif_suberror_Unsupported_color_conversion)
    .value("heif_suberror_Unsupported_item_construction_method", heif_suberror_Unsupported_item_construction_method)
    .value("heif_suberror_Unsupported_header_compression_method", heif_suberror_Unsupported_header_compression_method)
    .value("heif_suberror_Unsupported_bit_depth", heif_suberror_Unsupported_bit_depth)
    .value("heif_suberror_Unsupported_track_type", heif_suberror_Unsupported_track_type)
    .value("heif_suberror_Wrong_tile_image_pixel_depth", heif_suberror_Wrong_tile_image_pixel_depth)
    .value("heif_suberror_Unknown_NCLX_color_primaries", heif_suberror_Unknown_NCLX_color_primaries)
    .value("heif_suberror_Unknown_NCLX_transfer_characteristics", heif_suberror_Unknown_NCLX_transfer_characteristics)
    .value("heif_suberror_Unknown_NCLX_matrix_coefficients", heif_suberror_Unknown_NCLX_matrix_coefficients)
    .value("heif_suberror_No_ispe_property", heif_suberror_No_ispe_property)
    .value("heif_suberror_Camera_intrinsic_matrix_undefined", heif_suberror_Camera_intrinsic_matrix_undefined)
    .value("heif_suberror_Camera_extrinsic_matrix_undefined", heif_suberror_Camera_extrinsic_matrix_undefined)
    .value("heif_suberror_Invalid_J2K_codestream", heif_suberror_Invalid_J2K_codestream)
    .value("heif_suberror_No_icbr_box", heif_suberror_No_icbr_box);

    emscripten::enum_<heif_compression_format>("heif_compression_format")
    .value("heif_compression_undefined", heif_compression_undefined)
    .value("heif_compression_HEVC", heif_compression_HEVC)
    .value("heif_compression_AVC", heif_compression_AVC)
    .value("heif_compression_JPEG", heif_compression_JPEG)
    .value("heif_compression_AV1", heif_compression_AV1)
    .value("heif_compression_VVC", heif_compression_VVC)
    .value("heif_compression_EVC", heif_compression_EVC)
    .value("heif_compression_JPEG2000", heif_compression_JPEG2000)
    .value("heif_compression_uncompressed", heif_compression_uncompressed)
    .value("heif_compression_mask", heif_compression_mask)
    .value("heif_compression_HTJ2K", heif_compression_HTJ2K);
    emscripten::enum_<heif_chroma>("heif_chroma")
    .value("heif_chroma_undefined", heif_chroma_undefined)
    .value("heif_chroma_monochrome", heif_chroma_monochrome)
    .value("heif_chroma_420", heif_chroma_420)
    .value("heif_chroma_422", heif_chroma_422)
    .value("heif_chroma_444", heif_chroma_444)
    .value("heif_chroma_interleaved_RGB", heif_chroma_interleaved_RGB)
    .value("heif_chroma_interleaved_RGBA", heif_chroma_interleaved_RGBA)
    .value("heif_chroma_interleaved_RRGGBB_BE", heif_chroma_interleaved_RRGGBB_BE)
    .value("heif_chroma_interleaved_RRGGBBAA_BE", heif_chroma_interleaved_RRGGBBAA_BE)
    .value("heif_chroma_interleaved_RRGGBB_LE", heif_chroma_interleaved_RRGGBB_LE)
    .value("heif_chroma_interleaved_RRGGBBAA_LE", heif_chroma_interleaved_RRGGBBAA_LE)
    // Aliases
    .value("heif_chroma_interleaved_24bit", heif_chroma_interleaved_24bit)
    .value("heif_chroma_interleaved_32bit", heif_chroma_interleaved_32bit);
    emscripten::enum_<heif_chroma_downsampling_algorithm>("heif_chroma_downsampling_algorithm")
    .value("heif_chroma_downsampling_average", heif_chroma_downsampling_average)
    .value("heif_chroma_downsampling_nearest_neighbor", heif_chroma_downsampling_nearest_neighbor)
    .value("heif_chroma_downsampling_sharp_yuv", heif_chroma_downsampling_sharp_yuv);
    emscripten::enum_<heif_chroma_upsampling_algorithm>("heif_chroma_upsampling_algorithm")
    .value("heif_chroma_upsampling_bilinear", heif_chroma_upsampling_bilinear)
    .value("heif_chroma_upsampling_nearest_neighbor", heif_chroma_upsampling_nearest_neighbor);
    emscripten::enum_<heif_colorspace>("heif_colorspace")
    .value("heif_colorspace_undefined", heif_colorspace_undefined)
    .value("heif_colorspace_YCbCr", heif_colorspace_YCbCr)
    .value("heif_colorspace_RGB", heif_colorspace_RGB)
    .value("heif_colorspace_monochrome", heif_colorspace_monochrome)
    .value("heif_colorspace_nonvisual", heif_colorspace_nonvisual);
    emscripten::enum_<heif_channel>("heif_channel")
    .value("heif_channel_Y", heif_channel_Y)
    .value("heif_channel_Cr", heif_channel_Cr)
    .value("heif_channel_Cb", heif_channel_Cb)
    .value("heif_channel_R", heif_channel_R)
    .value("heif_channel_G", heif_channel_G)
    .value("heif_channel_B", heif_channel_B)
    .value("heif_channel_Alpha", heif_channel_Alpha)
    .value("heif_channel_interleaved", heif_channel_interleaved)
    .value("heif_channel_filter_array", heif_channel_filter_array)
    .value("heif_channel_depth", heif_channel_depth)
    .value("heif_channel_disparity", heif_channel_disparity);

    emscripten::enum_<heif_filetype_result>("heif_filetype_result")
    .value("heif_filetype_no", heif_filetype_no)
    .value("heif_filetype_yes_supported", heif_filetype_yes_supported)
    .value("heif_filetype_yes_unsupported", heif_filetype_yes_unsupported)
    .value("heif_filetype_maybe", heif_filetype_maybe);

    emscripten::class_<heif_context>("heif_context");
    emscripten::class_<heif_image_handle>("heif_image_handle");
    emscripten::class_<heif_image>("heif_image");
    emscripten::value_object<heif_error>("heif_error")
    .field("code", &heif_error::code)
    .field("subcode", &heif_error::subcode)
    .field("message", emscripten::optional_override([](const struct heif_error& err) {
        return std::string(err.message);
    }), emscripten::optional_override([](struct heif_error& err, const std::string& value) {
        err.message = value.c_str();
    }));
}

#endif  // LIBHEIF_BOX_EMSCRIPTEN_H
