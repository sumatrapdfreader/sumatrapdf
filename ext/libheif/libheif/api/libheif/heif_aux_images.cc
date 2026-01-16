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

#include "heif_aux_images.h"
#include "api_structs.h"

#include <cassert>
#include <cstring>
#include <memory>
#include <algorithm>


// ------------------------- depth images -------------------------


int heif_image_handle_has_depth_image(const heif_image_handle* handle)
{
  return handle->image->get_depth_channel() != nullptr;
}


int heif_image_handle_get_number_of_depth_images(const heif_image_handle* handle)
{
  auto depth_image = handle->image->get_depth_channel();

  if (depth_image) {
    return 1;
  }
  else {
    return 0;
  }
}


int heif_image_handle_get_list_of_depth_image_IDs(const heif_image_handle* handle,
                                                  heif_item_id* ids, int count)
{
  auto depth_image = handle->image->get_depth_channel();

  if (count == 0) {
    return 0;
  }

  if (depth_image) {
    ids[0] = depth_image->get_id();
    return 1;
  }
  else {
    return 0;
  }
}


heif_error heif_image_handle_get_depth_image_handle(const heif_image_handle* handle,
                                                    heif_item_id depth_id,
                                                    heif_image_handle** out_depth_handle)
{
  if (out_depth_handle == nullptr) {
    return heif_error_null_pointer_argument;
  }

  auto depth_image = handle->image->get_depth_channel();

  if (depth_image->get_id() != depth_id) {
    *out_depth_handle = nullptr;

    Error err(heif_error_Usage_error, heif_suberror_Nonexisting_item_referenced);
    return err.error_struct(handle->image.get());
  }

  *out_depth_handle = new heif_image_handle();
  (*out_depth_handle)->image = depth_image;
  (*out_depth_handle)->context = handle->context;

  return Error::Ok.error_struct(handle->image.get());
}


void heif_depth_representation_info_free(const heif_depth_representation_info* info)
{
  delete info;
}


int heif_image_handle_get_depth_image_representation_info(const heif_image_handle* handle,
                                                          heif_item_id depth_image_id,
                                                          const heif_depth_representation_info** out)
{
  std::shared_ptr<ImageItem> depth_image;

  if (out) {
    if (handle->image->is_depth_channel()) {
      // Because of an API bug before v1.11.0, the input handle may be the depth image (#422).
      depth_image = handle->image;
    }
    else {
      depth_image = handle->image->get_depth_channel();
    }

    if (depth_image->has_depth_representation_info()) {
      auto info = new heif_depth_representation_info;
      *info = depth_image->get_depth_representation_info();
      *out = info;
      return true;
    }
    else {
      *out = nullptr;
    }
  }

  return false;
}


// ------------------------- thumbnails -------------------------


int heif_image_handle_get_number_of_thumbnails(const heif_image_handle* handle)
{
  return (int) handle->image->get_thumbnails().size();
}


int heif_image_handle_get_list_of_thumbnail_IDs(const heif_image_handle* handle,
                                                heif_item_id* ids, int count)
{
  if (ids == nullptr) {
    return 0;
  }

  auto thumbnails = handle->image->get_thumbnails();
  int n = (int) std::min(count, (int) thumbnails.size());

  for (int i = 0; i < n; i++) {
    ids[i] = thumbnails[i]->get_id();
  }

  return n;
}


heif_error heif_image_handle_get_thumbnail(const heif_image_handle* handle,
                                           heif_item_id thumbnail_id,
                                           heif_image_handle** out_thumbnail_handle)
{
  if (!out_thumbnail_handle) {
    return heif_error_null_pointer_argument;
  }

  auto thumbnails = handle->image->get_thumbnails();
  for (const auto& thumb : thumbnails) {
    if (thumb->get_id() == thumbnail_id) {
      *out_thumbnail_handle = new heif_image_handle();
      (*out_thumbnail_handle)->image = thumb;
      (*out_thumbnail_handle)->context = handle->context;

      return Error::Ok.error_struct(handle->image.get());
    }
  }

  Error err(heif_error_Usage_error, heif_suberror_Nonexisting_item_referenced);
  return err.error_struct(handle->image.get());
}


heif_error heif_context_encode_thumbnail(heif_context* ctx,
                                         const heif_image* image,
                                         const heif_image_handle* image_handle,
                                         heif_encoder* encoder,
                                         const heif_encoding_options* input_options,
                                         int bbox_size,
                                         heif_image_handle** out_image_handle)
{
  heif_encoding_options* options = heif_encoding_options_alloc();
  heif_encoding_options_copy(options, input_options);

  auto encodingResult = ctx->context->encode_thumbnail(image->image,
                                                       encoder,
                                                       *options,
                                                       bbox_size);
  heif_encoding_options_free(options);

  if (!encodingResult) {
    return encodingResult.error_struct(ctx->context.get());
  }

  std::shared_ptr<ImageItem> thumbnail_image = *encodingResult;

  if (!thumbnail_image) {
    Error err(heif_error_Usage_error,
              heif_suberror_Invalid_parameter_value,
              "Thumbnail images must be smaller than the original image.");
    return err.error_struct(ctx->context.get());
  }

  Error error = ctx->context->assign_thumbnail(image_handle->image, thumbnail_image);
  if (error != Error::Ok) {
    return error.error_struct(ctx->context.get());
  }


  if (out_image_handle) {
    *out_image_handle = new heif_image_handle;
    (*out_image_handle)->image = thumbnail_image;
    (*out_image_handle)->context = ctx->context;
  }

  return heif_error_success;
}


heif_error heif_context_assign_thumbnail(heif_context* ctx,
                                         const heif_image_handle* master_image,
                                         const heif_image_handle* thumbnail_image)
{
  Error error = ctx->context->assign_thumbnail(thumbnail_image->image, master_image->image);
  return error.error_struct(ctx->context.get());
}


// ------------------------- auxiliary images -------------------------


int heif_image_handle_get_number_of_auxiliary_images(const heif_image_handle* handle,
                                                     int include_alpha_image)
{
  return (int) handle->image->get_aux_images(include_alpha_image).size();
}


int heif_image_handle_get_list_of_auxiliary_image_IDs(const heif_image_handle* handle,
                                                      int include_alpha_image,
                                                      heif_item_id* ids, int count)
{
  if (ids == nullptr) {
    return 0;
  }

  auto auxImages = handle->image->get_aux_images(include_alpha_image);
  int n = (int) std::min(count, (int) auxImages.size());

  for (int i = 0; i < n; i++) {
    ids[i] = auxImages[i]->get_id();
  }

  return n;
}


heif_error heif_image_handle_get_auxiliary_type(const heif_image_handle* handle,
                                                const char** out_type)
{
  if (out_type == nullptr) {
    return heif_error_null_pointer_argument;
  }

  *out_type = nullptr;

  const auto& auxType = handle->image->get_aux_type();

  char* buf = (char*) malloc(auxType.length() + 1);

  if (buf == nullptr) {
    return Error(heif_error_Memory_allocation_error,
                 heif_suberror_Unspecified,
                 "Failed to allocate memory for the type string").error_struct(handle->image.get());
  }

  strcpy(buf, auxType.c_str());
  *out_type = buf;

  return heif_error_success;
}


void heif_image_handle_release_auxiliary_type(const heif_image_handle* handle,
                                              const char** out_type)
{
  if (out_type && *out_type) {
    free((void*) *out_type);
    *out_type = nullptr;
  }
}


heif_error heif_image_handle_get_auxiliary_image_handle(const heif_image_handle* main_image_handle,
                                                        heif_item_id auxiliary_id,
                                                        heif_image_handle** out_auxiliary_handle)
{
  if (!out_auxiliary_handle) {
    return heif_error_null_pointer_argument;
  }

  *out_auxiliary_handle = nullptr;

  auto auxImages = main_image_handle->image->get_aux_images();
  for (const auto& aux : auxImages) {
    if (aux->get_id() == auxiliary_id) {
      *out_auxiliary_handle = new heif_image_handle();
      (*out_auxiliary_handle)->image = aux;
      (*out_auxiliary_handle)->context = main_image_handle->context;

      return Error::Ok.error_struct(main_image_handle->image.get());
    }
  }

  Error err(heif_error_Usage_error, heif_suberror_Nonexisting_item_referenced);
  return err.error_struct(main_image_handle->image.get());
}


// ===================== DEPRECATED =====================

// DEPRECATED (typo)
void heif_image_handle_free_auxiliary_types(const heif_image_handle* handle,
                                            const char** out_type)
{
  heif_image_handle_release_auxiliary_type(handle, out_type);
}
