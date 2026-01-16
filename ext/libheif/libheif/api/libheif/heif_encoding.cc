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

#include "heif_encoding.h"
#include "api_structs.h"
#include "context.h"
#include "init.h"
#include "plugin_registry.h"
#include "image-items/overlay.h"
#include "image-items/tiled.h"
#include "image-items/grid.h"

#include <cstring>

#include <vector>
#include <string>
#include <algorithm>
#include <memory>
#include <utility>


static heif_error error_unsupported_parameter = {
  heif_error_Usage_error,
  heif_suberror_Unsupported_parameter,
  "Unsupported encoder parameter"
};
static heif_error error_invalid_parameter_value = {
  heif_error_Usage_error,
  heif_suberror_Invalid_parameter_value,
  "Invalid parameter value"
};


int heif_have_encoder_for_format(heif_compression_format format)
{
  auto plugin = get_encoder(format);
  return plugin != nullptr;
}


int heif_get_encoder_descriptors(heif_compression_format format,
                                 const char* name,
                                 const heif_encoder_descriptor** out_encoder_descriptors,
                                 int count)
{
  if (out_encoder_descriptors != nullptr && count <= 0) {
    return 0;
  }

  std::vector<const heif_encoder_descriptor*> descriptors;
  descriptors = get_filtered_encoder_descriptors(format, name);

  if (out_encoder_descriptors == nullptr) {
    return static_cast<int>(descriptors.size());
  }

  int i;
  for (i = 0; i < count && static_cast<size_t>(i) < descriptors.size(); i++) {
    out_encoder_descriptors[i] = descriptors[i];
  }

  return i;
}


const char* heif_encoder_descriptor_get_name(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->get_plugin_name();
}


const char* heif_encoder_descriptor_get_id_name(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->id_name;
}


heif_compression_format
heif_encoder_descriptor_get_compression_format(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->compression_format;
}


int heif_encoder_descriptor_supports_lossy_compression(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->supports_lossy_compression;
}


int heif_encoder_descriptor_supports_lossless_compression(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->supports_lossless_compression;
}


heif_error heif_context_get_encoder(heif_context* context,
                                    const heif_encoder_descriptor* descriptor,
                                    heif_encoder** encoder)
{
  // Note: be aware that context may be NULL as we explicitly allowed that in an earlier documentation.

  if (!descriptor || !encoder) {
    return heif_error_null_pointer_argument;
  }

  *encoder = new heif_encoder(descriptor->plugin);
  return (*encoder)->alloc();
}


heif_error heif_context_get_encoder_for_format(heif_context* context,
                                               heif_compression_format format,
                                               heif_encoder** encoder)
{
  // Note: be aware that context may be NULL as we explicitly allowed that in an earlier documentation.

  if (!encoder) {
    return heif_error_null_pointer_argument;
  }

  std::vector<const heif_encoder_descriptor*> descriptors;
  descriptors = get_filtered_encoder_descriptors(format, nullptr);

  if (descriptors.size() > 0) {
    *encoder = new heif_encoder(descriptors[0]->plugin);
    return (*encoder)->alloc();
  }
  else {
    *encoder = nullptr;
    Error err(heif_error_Unsupported_filetype, // TODO: is this the right error code?
              heif_suberror_Unspecified);
    return err.error_struct(context ? context->context.get() : nullptr);
  }
}


void heif_encoder_release(heif_encoder* encoder)
{
  if (encoder) {
    delete encoder;
  }
}


const char* heif_encoder_get_name(const heif_encoder* encoder)
{
  return encoder->plugin->get_plugin_name();
}


// Set a 'quality' factor (0-100). How this is mapped to actual encoding parameters is
// encoder dependent.
heif_error heif_encoder_set_lossy_quality(heif_encoder* encoder,
                                          int quality)
{
  if (!encoder) {
    return heif_error_null_pointer_argument;
  }

  return encoder->plugin->set_parameter_quality(encoder->encoder, quality);
}


heif_error heif_encoder_set_lossless(heif_encoder* encoder, int enable)
{
  if (!encoder) {
    return heif_error_null_pointer_argument;
  }

  return encoder->plugin->set_parameter_lossless(encoder->encoder, enable);
}


heif_error heif_encoder_set_logging_level(heif_encoder* encoder, int level)
{
  if (!encoder) {
    return heif_error_null_pointer_argument;
  }

  if (encoder->plugin->set_parameter_logging_level) {
    return encoder->plugin->set_parameter_logging_level(encoder->encoder, level);
  }

  return heif_error_success;
}


const heif_encoder_parameter* const* heif_encoder_list_parameters(heif_encoder* encoder)
{
  return encoder->plugin->list_parameters(encoder->encoder);
}


const char* heif_encoder_parameter_get_name(const heif_encoder_parameter* param)
{
  return param->name;
}

heif_encoder_parameter_type
heif_encoder_parameter_get_type(const heif_encoder_parameter* param)
{
  return param->type;
}


heif_error
heif_encoder_parameter_get_valid_integer_range(const heif_encoder_parameter* param,
                                               int* have_minimum_maximum,
                                               int* minimum, int* maximum)
{
  if (param->type != heif_encoder_parameter_type_integer) {
    return error_unsupported_parameter; // TODO: correct error ?
  }

  if (param->integer.have_minimum_maximum) {
    if (minimum) {
      *minimum = param->integer.minimum;
    }

    if (maximum) {
      *maximum = param->integer.maximum;
    }
  }

  if (have_minimum_maximum) {
    *have_minimum_maximum = param->integer.have_minimum_maximum;
  }

  return heif_error_success;
}


heif_error heif_encoder_parameter_get_valid_integer_values(const heif_encoder_parameter* param,
                                                           int* have_minimum, int* have_maximum,
                                                           int* minimum, int* maximum,
                                                           int* num_valid_values,
                                                           const int** out_integer_array)
{
  if (param->type != heif_encoder_parameter_type_integer) {
    return error_unsupported_parameter; // TODO: correct error ?
  }


  // --- range of values

  if (param->integer.have_minimum_maximum) {
    if (minimum) {
      *minimum = param->integer.minimum;
    }

    if (maximum) {
      *maximum = param->integer.maximum;
    }
  }

  if (have_minimum) {
    *have_minimum = param->integer.have_minimum_maximum;
  }

  if (have_maximum) {
    *have_maximum = param->integer.have_minimum_maximum;
  }


  // --- set of valid values

  if (param->integer.num_valid_values > 0) {
    if (out_integer_array) {
      *out_integer_array = param->integer.valid_values;
    }
  }

  if (num_valid_values) {
    *num_valid_values = param->integer.num_valid_values;
  }

  return heif_error_success;
}


heif_error
heif_encoder_parameter_get_valid_string_values(const heif_encoder_parameter* param,
                                               const char* const** out_stringarray)
{
  if (param->type != heif_encoder_parameter_type_string) {
    return error_unsupported_parameter; // TODO: correct error ?
  }

  if (out_stringarray) {
    *out_stringarray = param->string.valid_values;
  }

  return heif_error_success;
}


heif_error heif_encoder_set_parameter_integer(heif_encoder* encoder,
                                              const char* parameter_name,
                                              int value)
{
  // --- check if parameter is valid

  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      int have_minimum = 0, have_maximum = 0, minimum = 0, maximum = 0, num_valid_values = 0;
      const int* valid_values = nullptr;
      heif_error err = heif_encoder_parameter_get_valid_integer_values((*params), &have_minimum, &have_maximum,
                                                                       &minimum, &maximum,
                                                                       &num_valid_values,
                                                                       &valid_values);
      if (err.code) {
        return err;
      }

      if ((have_minimum && value < minimum) ||
          (have_maximum && value > maximum)) {
        return error_invalid_parameter_value;
      }

      if (num_valid_values > 0) {
        bool found = false;
        for (int i = 0; i < num_valid_values; i++) {
          if (valid_values[i] == value) {
            found = true;
            break;
          }
        }

        if (!found) {
          return error_invalid_parameter_value;
        }
      }
    }
  }


  // --- parameter is ok, pass it to the encoder plugin

  return encoder->plugin->set_parameter_integer(encoder->encoder, parameter_name, value);
}

heif_error heif_encoder_get_parameter_integer(heif_encoder* encoder,
                                              const char* parameter_name,
                                              int* value_ptr)
{
  return encoder->plugin->get_parameter_integer(encoder->encoder, parameter_name, value_ptr);
}


heif_error heif_encoder_parameter_integer_valid_range(heif_encoder* encoder,
                                                      const char* parameter_name,
                                                      int* have_minimum_maximum,
                                                      int* minimum, int* maximum)
{
  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      return heif_encoder_parameter_get_valid_integer_range(*params, have_minimum_maximum,
                                                            minimum, maximum);
    }
  }

  return error_unsupported_parameter;
}

heif_error heif_encoder_set_parameter_boolean(heif_encoder* encoder,
                                              const char* parameter_name,
                                              int value)
{
  return encoder->plugin->set_parameter_boolean(encoder->encoder, parameter_name, value);
}

heif_error heif_encoder_get_parameter_boolean(heif_encoder* encoder,
                                              const char* parameter_name,
                                              int* value_ptr)
{
  return encoder->plugin->get_parameter_boolean(encoder->encoder, parameter_name, value_ptr);
}

heif_error heif_encoder_set_parameter_string(heif_encoder* encoder,
                                             const char* parameter_name,
                                             const char* value)
{
  return encoder->plugin->set_parameter_string(encoder->encoder, parameter_name, value);
}

heif_error heif_encoder_get_parameter_string(heif_encoder* encoder,
                                             const char* parameter_name,
                                             char* value_ptr, int value_size)
{
  return encoder->plugin->get_parameter_string(encoder->encoder, parameter_name,
                                               value_ptr, value_size);
}

heif_error heif_encoder_parameter_string_valid_values(heif_encoder* encoder,
                                                      const char* parameter_name,
                                                      const char* const** out_stringarray)
{
  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      return heif_encoder_parameter_get_valid_string_values(*params, out_stringarray);
    }
  }

  return error_unsupported_parameter;
}

heif_error heif_encoder_parameter_integer_valid_values(heif_encoder* encoder,
                                                       const char* parameter_name,
                                                       int* have_minimum, int* have_maximum,
                                                       int* minimum, int* maximum,
                                                       int* num_valid_values,
                                                       const int** out_integer_array)
{
  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      return heif_encoder_parameter_get_valid_integer_values(*params, have_minimum, have_maximum, minimum, maximum,
                                                             num_valid_values, out_integer_array);
    }
  }

  return error_unsupported_parameter;
}


static bool parse_boolean(const char* value)
{
  if (strcmp(value, "true") == 0) {
    return true;
  }
  else if (strcmp(value, "false") == 0) {
    return false;
  }
  else if (strcmp(value, "1") == 0) {
    return true;
  }
  else if (strcmp(value, "0") == 0) {
    return false;
  }

  return false;
}


heif_error heif_encoder_set_parameter(heif_encoder* encoder,
                                      const char* parameter_name,
                                      const char* value)
{
  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      switch ((*params)->type) {
        case heif_encoder_parameter_type_integer:
          return heif_encoder_set_parameter_integer(encoder, parameter_name, atoi(value));

        case heif_encoder_parameter_type_boolean:
          return heif_encoder_set_parameter_boolean(encoder, parameter_name, parse_boolean(value));

        case heif_encoder_parameter_type_string:
          return heif_encoder_set_parameter_string(encoder, parameter_name, value);
          break;
      }

      return heif_error_success;
    }
  }

  return heif_encoder_set_parameter_string(encoder, parameter_name, value);

  //return error_unsupported_parameter;
}


heif_error heif_encoder_get_parameter(heif_encoder* encoder,
                                      const char* parameter_name,
                                      char* value_ptr, int value_size)
{
  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      switch ((*params)->type) {
        case heif_encoder_parameter_type_integer: {
          int value;
          heif_error error = heif_encoder_get_parameter_integer(encoder, parameter_name, &value);
          if (error.code) {
            return error;
          }
          else {
            snprintf(value_ptr, value_size, "%d", value);
          }
        }
        break;

        case heif_encoder_parameter_type_boolean: {
          int value;
          heif_error error = heif_encoder_get_parameter_boolean(encoder, parameter_name, &value);
          if (error.code) {
            return error;
          }
          else {
            snprintf(value_ptr, value_size, "%d", value);
          }
        }
        break;

        case heif_encoder_parameter_type_string: {
          heif_error error = heif_encoder_get_parameter_string(encoder, parameter_name,
                                                               value_ptr, value_size);
          if (error.code) {
            return error;
          }
        }
        break;
      }

      return heif_error_success;
    }
  }

  return error_unsupported_parameter;
}


int heif_encoder_has_default(heif_encoder* encoder,
                             const char* parameter_name)
{
  for (const heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
       *params;
       params++) {
    if (strcmp((*params)->name, parameter_name) == 0) {
      if ((*params)->version >= 2) {
        return (*params)->has_default;
      }
      else {
        return true;
      }
    }
  }

  return false;
}


static void set_default_encoding_options(heif_encoding_options& options)
{
  options.version = 7;

  options.save_alpha_channel = true;
  options.macOS_compatibility_workaround = false;
  options.save_two_colr_boxes_when_ICC_and_nclx_available = false;
  options.output_nclx_profile = nullptr;
  options.macOS_compatibility_workaround_no_nclx_profile = false;
  options.image_orientation = heif_orientation_normal;

  options.color_conversion_options.version = 1;
  options.color_conversion_options.preferred_chroma_downsampling_algorithm = heif_chroma_downsampling_average;
  options.color_conversion_options.preferred_chroma_upsampling_algorithm = heif_chroma_upsampling_bilinear;
  options.color_conversion_options.only_use_preferred_chroma_algorithm = false;

  options.prefer_uncC_short_form = true;
}


heif_encoding_options* heif_encoding_options_alloc()
{
  auto options = new heif_encoding_options;

  set_default_encoding_options(*options);

  return options;
}


void heif_encoding_options_copy(heif_encoding_options* dst, const heif_encoding_options* src)
{
  if (src == nullptr) {
    return;
  }

  int min_version = std::min(dst->version, src->version);

  switch (min_version) {
    case 7:
      dst->prefer_uncC_short_form = src->prefer_uncC_short_form;
      [[fallthrough]];
    case 6:
      dst->color_conversion_options = src->color_conversion_options;
      [[fallthrough]];
    case 5:
      dst->image_orientation = src->image_orientation;
      [[fallthrough]];
    case 4:
      dst->output_nclx_profile = src->output_nclx_profile;
      dst->macOS_compatibility_workaround_no_nclx_profile = src->macOS_compatibility_workaround_no_nclx_profile;
      [[fallthrough]];
    case 3:
      dst->save_two_colr_boxes_when_ICC_and_nclx_available = src->save_two_colr_boxes_when_ICC_and_nclx_available;
      [[fallthrough]];
    case 2:
      dst->macOS_compatibility_workaround = src->macOS_compatibility_workaround;
      [[fallthrough]];
    case 1:
      dst->save_alpha_channel = src->save_alpha_channel;
  }
}


void heif_encoding_options_free(heif_encoding_options* options)
{
  delete options;
}

heif_error heif_context_encode_image(heif_context* ctx,
                                     const heif_image* input_image,
                                     heif_encoder* encoder,
                                     const heif_encoding_options* input_options,
                                     heif_image_handle** out_image_handle)
{
  if (!encoder) {
    return heif_error_null_pointer_argument;
  }

  if (out_image_handle) {
    *out_image_handle = nullptr;
  }

  heif_encoding_options options;
  heif_color_profile_nclx nclx;
  set_default_encoding_options(options);
  if (input_options) {
    heif_encoding_options_copy(&options, input_options);

    if (options.output_nclx_profile == nullptr) {
      if (input_image->image->has_nclx_color_profile()) {
        nclx_profile input_nclx = input_image->image->get_color_profile_nclx();

        options.output_nclx_profile = &nclx;
        nclx.version = 1;
        nclx.color_primaries = input_nclx.get_colour_primaries();
        nclx.transfer_characteristics = input_nclx.get_transfer_characteristics();
        nclx.matrix_coefficients = input_nclx.get_matrix_coefficients();
        nclx.full_range_flag = input_nclx.get_full_range_flag();
      }
    }
  }

  auto encodingResult = ctx->context->encode_image(input_image->image,
                                                   encoder,
                                                   options,
                                                   heif_image_input_class_normal);
  if (!encodingResult) {
    return encodingResult.error_struct(ctx->context.get());
  }

  std::shared_ptr<ImageItem> image = *encodingResult;

  // mark the new image as primary image

  if (ctx->context->is_primary_image_set() == false) {
    ctx->context->set_primary_image(image);
  }

  if (out_image_handle) {
    *out_image_handle = new heif_image_handle;
    (*out_image_handle)->image = std::move(image);
    (*out_image_handle)->context = ctx->context;
  }

  return heif_error_success;
}


heif_error heif_context_add_overlay_image(heif_context* ctx,
                                          uint32_t image_width,
                                          uint32_t image_height,
                                          uint16_t nImages,
                                          const heif_item_id* image_ids,
                                          int32_t* offsets,
                                          const uint16_t background_rgba[4],
                                          heif_image_handle** out_iovl_image_handle)
{
  if (!image_ids) {
    return heif_error_null_pointer_argument;
  }
  else if (nImages == 0) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value).error_struct(ctx->context.get());
  }


  std::vector<heif_item_id> refs;
  refs.insert(refs.end(), image_ids, image_ids + nImages);

  ImageOverlay overlay;
  overlay.set_canvas_size(image_width, image_height);

  if (background_rgba) {
    overlay.set_background_color(background_rgba);
  }

  for (uint16_t i = 0; i < nImages; i++) {
    overlay.add_image_on_top(image_ids[i],
                             offsets ? offsets[2 * i] : 0,
                             offsets ? offsets[2 * i + 1] : 0);
  }

  Result<std::shared_ptr<ImageItem_Overlay> > addImageResult = ImageItem_Overlay::add_new_overlay_item(ctx->context.get(), overlay);

  if (!addImageResult) {
    return addImageResult.error_struct(ctx->context.get());
  }

  std::shared_ptr<ImageItem> iovlimage = *addImageResult;


  if (out_iovl_image_handle) {
    *out_iovl_image_handle = new heif_image_handle;
    (*out_iovl_image_handle)->image = std::move(iovlimage);
    (*out_iovl_image_handle)->context = ctx->context;
  }

  return heif_error_success;
}


heif_error heif_context_set_primary_image(heif_context* ctx,
                                          heif_image_handle* image_handle)
{
  ctx->context->set_primary_image(image_handle->image);

  return heif_error_success;
}


void heif_context_set_major_brand(heif_context* ctx,
                                  heif_brand2 major_brand)
{
  auto ftyp = ctx->context->get_heif_file()->get_ftyp_box();
  ftyp->set_major_brand(major_brand);
  ftyp->add_compatible_brand(major_brand);
}


void heif_context_add_compatible_brand(heif_context* ctx,
                                       heif_brand2 compatible_brand)
{
  ctx->context->get_heif_file()->get_ftyp_box()->add_compatible_brand(compatible_brand);
}


// === DEPRECATED ===

// DEPRECATED: typo in function name
int heif_encoder_descriptor_supportes_lossy_compression(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->supports_lossy_compression;
}


// DEPRECATED: typo in function name
int heif_encoder_descriptor_supportes_lossless_compression(const heif_encoder_descriptor* descriptor)
{
  return descriptor->plugin->supports_lossless_compression;
}


// DEPRECATED
int heif_context_get_encoder_descriptors(heif_context* ctx,
                                         heif_compression_format format,
                                         const char* name,
                                         const heif_encoder_descriptor** out_encoder_descriptors,
                                         int count)
{
  return heif_get_encoder_descriptors(format, name, out_encoder_descriptors, count);
}
