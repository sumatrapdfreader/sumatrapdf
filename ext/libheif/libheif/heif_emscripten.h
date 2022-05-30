#ifndef LIBHEIF_BOX_EMSCRIPTEN_H
#define LIBHEIF_BOX_EMSCRIPTEN_H

#include <emscripten/bind.h>

#include <memory>
#include <string>
#include <sstream>
#include <utility>
#include <vector>

#include "heif.h"

static std::string _heif_get_version()
{
  return heif_get_version();
}

static struct heif_error _heif_context_read_from_memory(
    struct heif_context* context, const std::string& data)
{
  return heif_context_read_from_memory(context, data.data(), data.size(), nullptr);
}

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
      memcpy(_dest, _src, stride);
    }
  }
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

  return emscripten::val(handle);
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

  heif_item_id* ids = (heif_item_id*) malloc(count * sizeof(heif_item_id));
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
  free(ids);
  return result;
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
  int width = heif_image_get_width(image, heif_channel_Y);
  result.set("width", width);
  int height = heif_image_get_height(image, heif_channel_Y);
  result.set("height", height);
  std::string data;
  result.set("chroma", heif_image_get_chroma_format(image));
  result.set("colorspace", heif_image_get_colorspace(image));
  switch (heif_image_get_colorspace(image)) {
    case heif_colorspace_YCbCr: {
      int stride_y;
      const uint8_t* plane_y = heif_image_get_plane_readonly(image,
                                                             heif_channel_Y, &stride_y);
      int stride_u;
      const uint8_t* plane_u = heif_image_get_plane_readonly(image,
                                                             heif_channel_Cb, &stride_u);
      int stride_v;
      const uint8_t* plane_v = heif_image_get_plane_readonly(image,
                                                             heif_channel_Cr, &stride_v);
      data.resize((width * height) + (2 * round_odd(width) * round_odd(height)));
      char* dest = const_cast<char*>(data.data());
      strided_copy(dest, plane_y, width, height, stride_y);
      strided_copy(dest + (width * height), plane_u,
                   round_odd(width), round_odd(height), stride_u);
      strided_copy(dest + (width * height) + (round_odd(width) * round_odd(height)),
                   plane_v, round_odd(width), round_odd(height), stride_v);
    }
      break;
    case heif_colorspace_RGB: {
      assert(heif_image_get_chroma_format(image) ==
             heif_chroma_interleaved_24bit);
      int stride_rgb;
      const uint8_t* plane_rgb = heif_image_get_plane_readonly(image,
                                                               heif_channel_interleaved, &stride_rgb);
      data.resize(width * height * 3);
      char* dest = const_cast<char*>(data.data());
      strided_copy(dest, plane_rgb, width * 3, height, stride_rgb);
    }
      break;
    case heif_colorspace_monochrome: {
      assert(heif_image_get_chroma_format(image) ==
             heif_chroma_monochrome);
      int stride_grey;
      const uint8_t* plane_grey = heif_image_get_plane_readonly(image,
                                                                heif_channel_Y, &stride_grey);
      data.resize(width * height);
      char* dest = const_cast<char*>(data.data());
      strided_copy(dest, plane_grey, width, height, stride_grey);
    }
      break;
    default:
      // Should never reach here.
      break;
  }
  result.set("data", std::move(data));
  heif_image_release(image);
  return result;
}

#define EXPORT_HEIF_FUNCTION(name) \
  emscripten::function(#name, &name, emscripten::allow_raw_pointers())

EMSCRIPTEN_BINDINGS(libheif) {
    emscripten::function("heif_get_version", &_heif_get_version,
                         emscripten::allow_raw_pointers());
    EXPORT_HEIF_FUNCTION(heif_get_version_number);

    EXPORT_HEIF_FUNCTION(heif_context_alloc);
    EXPORT_HEIF_FUNCTION(heif_context_free);
    emscripten::function("heif_context_read_from_memory",
    &_heif_context_read_from_memory, emscripten::allow_raw_pointers());
    EXPORT_HEIF_FUNCTION(heif_context_get_number_of_top_level_images);
    emscripten::function("heif_js_context_get_list_of_top_level_image_IDs",
    &heif_js_context_get_list_of_top_level_image_IDs, emscripten::allow_raw_pointers());
    emscripten::function("heif_js_context_get_image_handle",
    &heif_js_context_get_image_handle, emscripten::allow_raw_pointers());
    emscripten::function("heif_js_decode_image",
    &heif_js_decode_image, emscripten::allow_raw_pointers());
    EXPORT_HEIF_FUNCTION(heif_image_handle_release);

    emscripten::enum_<heif_error_code>("heif_error_code")
    .value("heif_error_Ok", heif_error_Ok)
    .value("heif_error_Input_does_not_exist", heif_error_Input_does_not_exist)
    .value("heif_error_Invalid_input", heif_error_Invalid_input)
    .value("heif_error_Unsupported_filetype", heif_error_Unsupported_filetype)
    .value("heif_error_Unsupported_feature", heif_error_Unsupported_feature)
    .value("heif_error_Usage_error", heif_error_Usage_error)
    .value("heif_error_Memory_allocation_error", heif_error_Memory_allocation_error)
    .value("heif_error_Decoder_plugin_error", heif_error_Decoder_plugin_error)
    .value("heif_error_Encoder_plugin_error", heif_error_Encoder_plugin_error)
    .value("heif_error_Encoding_error", heif_error_Encoding_error)
    .value("heif_error_Color_profile_does_not_exist", heif_error_Color_profile_does_not_exist);
    emscripten::enum_<heif_suberror_code>("heif_suberror_code")
    .value("heif_suberror_Unspecified", heif_suberror_Unspecified)
    .value("heif_suberror_Cannot_write_output_data", heif_suberror_Cannot_write_output_data)
    .value("heif_suberror_End_of_data", heif_suberror_End_of_data)
    .value("heif_suberror_Invalid_box_size", heif_suberror_Invalid_box_size)
    .value("heif_suberror_No_ftyp_box", heif_suberror_No_ftyp_box)
    .value("heif_suberror_No_idat_box", heif_suberror_No_idat_box)
    .value("heif_suberror_No_meta_box", heif_suberror_No_meta_box)
    .value("heif_suberror_No_hdlr_box", heif_suberror_No_hdlr_box)
    .value("heif_suberror_No_hvcC_box", heif_suberror_No_hvcC_box)
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
    .value("heif_suberror_Invalid_clean_aperture", heif_suberror_Invalid_clean_aperture)
    .value("heif_suberror_Invalid_overlay_data", heif_suberror_Invalid_overlay_data)
    .value("heif_suberror_Overlay_image_outside_of_canvas", heif_suberror_Overlay_image_outside_of_canvas)
    .value("heif_suberror_Auxiliary_image_type_unspecified", heif_suberror_Auxiliary_image_type_unspecified)
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
    .value("heif_suberror_Invalid_pixi_box", heif_suberror_Invalid_pixi_box)
    .value("heif_suberror_Unsupported_codec", heif_suberror_Unsupported_codec)
    .value("heif_suberror_Unsupported_image_type", heif_suberror_Unsupported_image_type)
    .value("heif_suberror_Unsupported_data_version", heif_suberror_Unsupported_data_version)
    .value("heif_suberror_Unsupported_color_conversion", heif_suberror_Unsupported_color_conversion)
    .value("heif_suberror_Unsupported_item_construction_method", heif_suberror_Unsupported_item_construction_method)
    .value("heif_suberror_Unsupported_bit_depth", heif_suberror_Unsupported_bit_depth)
    .value("heif_suberror_Wrong_tile_image_pixel_depth", heif_suberror_Wrong_tile_image_pixel_depth)
    .value("heif_suberror_Unknown_NCLX_color_primaries", heif_suberror_Unknown_NCLX_color_primaries)
    .value("heif_suberror_Unknown_NCLX_transfer_characteristics", heif_suberror_Unknown_NCLX_transfer_characteristics)
    .value("heif_suberror_Unknown_NCLX_matrix_coefficients", heif_suberror_Unknown_NCLX_matrix_coefficients);
    emscripten::enum_<heif_compression_format>("heif_compression_format")
    .value("heif_compression_undefined", heif_compression_undefined)
    .value("heif_compression_HEVC", heif_compression_HEVC)
    .value("heif_compression_AVC", heif_compression_AVC)
    .value("heif_compression_JPEG", heif_compression_JPEG)
    .value("heif_compression_AV1", heif_compression_AV1);
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
    emscripten::enum_<heif_colorspace>("heif_colorspace")
    .value("heif_colorspace_undefined", heif_colorspace_undefined)
    .value("heif_colorspace_YCbCr", heif_colorspace_YCbCr)
    .value("heif_colorspace_RGB", heif_colorspace_RGB)
    .value("heif_colorspace_monochrome", heif_colorspace_monochrome);
    emscripten::enum_<heif_channel>("heif_channel")
    .value("heif_channel_Y", heif_channel_Y)
    .value("heif_channel_Cr", heif_channel_Cr)
    .value("heif_channel_Cb", heif_channel_Cb)
    .value("heif_channel_R", heif_channel_R)
    .value("heif_channel_G", heif_channel_G)
    .value("heif_channel_B", heif_channel_B)
    .value("heif_channel_Alpha", heif_channel_Alpha)
    .value("heif_channel_interleaved", heif_channel_interleaved);

    emscripten::class_<heif_context>("heif_context");
    emscripten::class_<heif_image_handle>("heif_image_handle");
    emscripten::class_<heif_image>("heif_image");
    emscripten::value_object<heif_error>("heif_error")
    .field("code", &heif_error::code)
    .field("subcode", &heif_error::subcode);
}

#endif  // LIBHEIF_BOX_EMSCRIPTEN_H
