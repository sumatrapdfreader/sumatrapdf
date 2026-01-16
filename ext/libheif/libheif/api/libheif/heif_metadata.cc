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

#include "heif_metadata.h"
#include "libheif/heif.h"
#include "api_structs.h"

#include "box.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <array>


int heif_metadata_compression_method_supported(enum heif_metadata_compression method)
{
  switch (method) {
    case heif_metadata_compression_off:
    case heif_metadata_compression_auto:
      return true;

    case heif_metadata_compression_deflate:
    case heif_metadata_compression_zlib:
#if HAVE_ZLIB
      return true;
#else
      return false;
#endif

    case heif_metadata_compression_brotli:
#if HAVE_BROTLI
      return true;
#else
      return false;
#endif

    default:
      return false;
  }
}


int heif_image_handle_get_number_of_metadata_blocks(const heif_image_handle* handle,
                                                    const char* type_filter)
{
  int cnt = 0;
  for (const auto& metadata : handle->image->get_metadata()) {
    if (type_filter == nullptr ||
        metadata->item_type == type_filter) {
      cnt++;
    }
  }

  return cnt;
}


int heif_image_handle_get_list_of_metadata_block_IDs(const heif_image_handle* handle,
                                                     const char* type_filter,
                                                     heif_item_id* ids, int count)
{
  int cnt = 0;
  for (const auto& metadata : handle->image->get_metadata()) {
    if (type_filter == nullptr ||
        metadata->item_type == type_filter) {
      if (cnt < count) {
        ids[cnt] = metadata->item_id;
        cnt++;
      }
      else {
        break;
      }
    }
  }

  return cnt;
}


const char* heif_image_handle_get_metadata_type(const heif_image_handle* handle,
                                                heif_item_id metadata_id)
{
  for (auto& metadata : handle->image->get_metadata()) {
    if (metadata->item_id == metadata_id) {
      return metadata->item_type.c_str();
    }
  }

  return nullptr;
}


const char* heif_image_handle_get_metadata_content_type(const heif_image_handle* handle,
                                                        heif_item_id metadata_id)
{
  for (auto& metadata : handle->image->get_metadata()) {
    if (metadata->item_id == metadata_id) {
      return metadata->content_type.c_str();
    }
  }

  return nullptr;
}


size_t heif_image_handle_get_metadata_size(const heif_image_handle* handle,
                                           heif_item_id metadata_id)
{
  for (auto& metadata : handle->image->get_metadata()) {
    if (metadata->item_id == metadata_id) {
      return metadata->m_data.size();
    }
  }

  return 0;
}


heif_error heif_image_handle_get_metadata(const heif_image_handle* handle,
                                          heif_item_id metadata_id,
                                          void* out_data)
{
  for (auto& metadata : handle->image->get_metadata()) {
    if (metadata->item_id == metadata_id) {
      if (!metadata->m_data.empty()) {
        if (out_data == nullptr) {
          return heif_error_null_pointer_argument;
        }

        memcpy(out_data,
               metadata->m_data.data(),
               metadata->m_data.size());
      }

      return Error::Ok.error_struct(handle->image.get());
    }
  }

  Error err(heif_error_Usage_error,
            heif_suberror_Nonexisting_item_referenced);
  return err.error_struct(handle->image.get());
}


const char* heif_image_handle_get_metadata_item_uri_type(const heif_image_handle* handle,
                                                         heif_item_id metadata_id)
{
  for (auto& metadata : handle->image->get_metadata()) {
    if (metadata->item_id == metadata_id) {
      return metadata->item_uri_type.c_str();
    }
  }

  return nullptr;
}


heif_error heif_context_add_exif_metadata(heif_context* ctx,
                                          const heif_image_handle* image_handle,
                                          const void* data, int size)
{
  Error error = ctx->context->add_exif_metadata(image_handle->image, data, size);
  if (error != Error::Ok) {
    return error.error_struct(ctx->context.get());
  }
  else {
    return heif_error_success;
  }
}


heif_error heif_context_add_XMP_metadata(heif_context* ctx,
                                         const heif_image_handle* image_handle,
                                         const void* data, int size)
{
  return heif_context_add_XMP_metadata2(ctx, image_handle, data, size,
                                        heif_metadata_compression_off);
}


heif_error heif_context_add_XMP_metadata2(heif_context* ctx,
                                          const heif_image_handle* image_handle,
                                          const void* data, int size,
                                          heif_metadata_compression compression)
{
  Error error = ctx->context->add_XMP_metadata(image_handle->image, data, size, compression);
  if (error != Error::Ok) {
    return error.error_struct(ctx->context.get());
  }
  else {
    return heif_error_success;
  }
}


heif_error heif_context_add_generic_metadata(heif_context* ctx,
                                             const heif_image_handle* image_handle,
                                             const void* data, int size,
                                             const char* item_type, const char* content_type)
{
  if (item_type == nullptr || strlen(item_type) != 4) {
    return {
      heif_error_Usage_error,
      heif_suberror_Invalid_parameter_value,
      "called heif_context_add_generic_metadata() with invalid 'item_type'."
    };
  }

  Error error = ctx->context->add_generic_metadata(image_handle->image, data, size,
                                                   fourcc(item_type), content_type, nullptr, heif_metadata_compression_off, nullptr);
  if (error != Error::Ok) {
    return error.error_struct(ctx->context.get());
  }
  else {
    return heif_error_success;
  }
}


heif_error heif_context_add_generic_uri_metadata(heif_context* ctx,
                                                 const heif_image_handle* image_handle,
                                                 const void* data, int size,
                                                 const char* item_uri_type,
                                                 heif_item_id* out_item_id)
{
  Error error = ctx->context->add_generic_metadata(image_handle->image, data, size,
                                                   fourcc("uri "), nullptr, item_uri_type, heif_metadata_compression_off, out_item_id);
  if (error != Error::Ok) {
    return error.error_struct(ctx->context.get());
  }
  else {
    return heif_error_success;
  }
}
