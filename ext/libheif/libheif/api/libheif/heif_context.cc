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

#include "heif_context.h"
#include "api_structs.h"
#include "context.h"
#include "init.h"
#include "file.h"

#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#ifdef _WIN32
// for _write
#include <io.h>
#else
#include <unistd.h>
#endif


heif_context* heif_context_alloc()
{
  load_plugins_if_not_initialized_yet();

  heif_context* ctx = new heif_context;
  ctx->context = std::make_shared<HeifContext>();

  return ctx;
}

void heif_context_free(heif_context* ctx)
{
  delete ctx;
}

heif_error heif_context_read_from_file(heif_context* ctx, const char* filename,
                                       const heif_reading_options*)
{
  Error err = ctx->context->read_from_file(filename);
  return err.error_struct(ctx->context.get());
}

heif_error heif_context_read_from_memory(heif_context* ctx, const void* mem, size_t size,
                                         const heif_reading_options*)
{
  Error err = ctx->context->read_from_memory(mem, size, true);
  return err.error_struct(ctx->context.get());
}

heif_error heif_context_read_from_memory_without_copy(heif_context* ctx, const void* mem, size_t size,
                                                      const heif_reading_options*)
{
  Error err = ctx->context->read_from_memory(mem, size, false);
  return err.error_struct(ctx->context.get());
}

heif_error heif_context_read_from_reader(heif_context* ctx,
                                         const heif_reader* reader_func_table,
                                         void* userdata,
                                         const heif_reading_options*)
{
  auto reader = std::make_shared<StreamReader_CApi>(reader_func_table, userdata);

  Error err = ctx->context->read(reader);
  return err.error_struct(ctx->context.get());
}

// TODO: heif_error heif_context_read_from_file_descriptor(heif_context*, int fd);

int heif_context_get_number_of_top_level_images(heif_context* ctx)
{
  return (int) ctx->context->get_top_level_images(true).size();
}


int heif_context_is_top_level_image_ID(heif_context* ctx, heif_item_id id)
{
  const std::vector<std::shared_ptr<ImageItem> > images = ctx->context->get_top_level_images(true);

  for (const auto& img : images) {
    if (img->get_id() == id) {
      return true;
    }
  }

  return false;
}


int heif_context_get_list_of_top_level_image_IDs(heif_context* ctx,
                                                 heif_item_id* ID_array,
                                                 int count)
{
  if (ID_array == nullptr || count == 0 || ctx == nullptr) {
    return 0;
  }


  // fill in ID values into output array

  const std::vector<std::shared_ptr<ImageItem> > imgs = ctx->context->get_top_level_images(true);
  int n = (int) std::min(count, (int) imgs.size());
  for (int i = 0; i < n; i++) {
    ID_array[i] = imgs[i]->get_id();
  }

  return n;
}


heif_error heif_context_get_primary_image_ID(heif_context* ctx, heif_item_id* id)
{
  if (!id) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Null_pointer_argument).error_struct(ctx->context.get());
  }

  std::shared_ptr<ImageItem> primary = ctx->context->get_primary_image(true);
  if (!primary) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_No_or_invalid_primary_item).error_struct(ctx->context.get());
  }

  *id = primary->get_id();

  return Error::Ok.error_struct(ctx->context.get());
}


heif_error heif_context_get_primary_image_handle(heif_context* ctx, heif_image_handle** img)
{
  if (!img) {
    Error err(heif_error_Usage_error,
              heif_suberror_Null_pointer_argument);
    return err.error_struct(ctx->context.get());
  }

  std::shared_ptr<ImageItem> primary_image = ctx->context->get_primary_image(true);

  // It is a requirement of an HEIF file there is always a primary image.
  // If there is none, an error is generated when loading the file.
  if (!primary_image) {
    Error err(heif_error_Invalid_input,
              heif_suberror_No_or_invalid_primary_item);
    return err.error_struct(ctx->context.get());
  }

  if (auto errImage = std::dynamic_pointer_cast<ImageItem_Error>(primary_image)) {
    Error error = errImage->get_item_error();
    return error.error_struct(ctx->context.get());
  }

  *img = new heif_image_handle();
  (*img)->image = std::move(primary_image);
  (*img)->context = ctx->context;

  return Error::Ok.error_struct(ctx->context.get());
}


heif_error heif_context_get_image_handle(heif_context* ctx,
                                         heif_item_id id,
                                         heif_image_handle** imgHdl)
{
  if (!imgHdl) {
    return {heif_error_Usage_error, heif_suberror_Null_pointer_argument, ""};
  }

  auto image = ctx->context->get_image(id, true);

  if (auto errImage = std::dynamic_pointer_cast<ImageItem_Error>(image)) {
    Error error = errImage->get_item_error();
    return error.error_struct(ctx->context.get());
  }

  if (!image) {
    *imgHdl = nullptr;

    return {heif_error_Usage_error, heif_suberror_Nonexisting_item_referenced, ""};
  }

  *imgHdl = new heif_image_handle();
  (*imgHdl)->image = std::move(image);
  (*imgHdl)->context = ctx->context;

  return heif_error_success;
}


void heif_context_debug_dump_boxes_to_file(heif_context* ctx, int fd)
{
  if (!ctx) {
    return;
  }

  std::string dump = ctx->context->debug_dump_boxes();
  // TODO(fancycode): Should we return an error if writing fails?
#ifdef _WIN32
  auto written = _write(fd, dump.c_str(), static_cast<unsigned int>(dump.size()));
#else
  auto written = write(fd, dump.c_str(), dump.size());
#endif
  (void) written;
}


// ====================================================================================================
//   Write the heif_context to a HEIF file


static heif_error heif_file_writer_write(heif_context* ctx,
                                         const void* data, size_t size, void* userdata)
{
  const char* filename = static_cast<const char*>(userdata);

#if defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)
  std::ofstream ostr(HeifFile::convert_utf8_path_to_utf16(filename).c_str(), std::ios_base::binary);
#else
  std::ofstream ostr(filename, std::ios_base::binary);
#endif
  ostr.write(static_cast<const char*>(data), size);
  // TODO: handle write errors
  return Error::Ok.error_struct(ctx->context.get());
}


heif_error heif_context_write_to_file(heif_context* ctx,
                                      const char* filename)
{
  heif_writer writer;
  writer.writer_api_version = 1;
  writer.write = heif_file_writer_write;
  return heif_context_write(ctx, &writer, (void*) filename);
}


heif_error heif_context_write(heif_context* ctx,
                              heif_writer* writer,
                              void* userdata)
{
  if (!writer) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Null_pointer_argument).error_struct(ctx->context.get());
  }

  if (writer->writer_api_version != 1) {
    Error err(heif_error_Usage_error, heif_suberror_Unsupported_writer_version);
    return err.error_struct(ctx->context.get());
  }

  StreamWriter swriter;
  ctx->context->write(swriter);

  const auto& data = swriter.get_data();
  heif_error writer_error = writer->write(ctx, data.data(), data.size(), userdata);
  if (!writer_error.message) {
    // It is now allowed to return a NULL error message on success. It will be replaced by "Success". An error message is still required when there is an error.
    if (writer_error.code == heif_error_Ok) {
      writer_error.message = Error::kSuccess;
      return writer_error;
    }
    else {
      return heif_error{heif_error_Usage_error, heif_suberror_Null_pointer_argument, "heif_writer callback returned a null error text"};
    }
  }
  else {
    return writer_error;
  }
}
