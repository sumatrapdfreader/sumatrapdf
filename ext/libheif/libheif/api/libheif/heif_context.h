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

#ifndef LIBHEIF_HEIF_CONTEXT_H
#define LIBHEIF_HEIF_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#include <libheif/heif_library.h>
#include <libheif/heif_error.h>


/**
 * libheif known compression formats.
 */
enum heif_compression_format
{
  /**
   * Unspecified / undefined compression format.
   *
   * This is used to mean "no match" or "any decoder" for some parts of the
   * API. It does not indicate a specific compression format.
   */
  heif_compression_undefined = 0,
  /**
   * HEVC compression, used for HEIC images.
   *
   * This is equivalent to H.265.
  */
  heif_compression_HEVC = 1,
  /**
   * AVC compression. (Currently unused in libheif.)
   *
   * The compression is defined in ISO/IEC 14496-10. This is equivalent to H.264.
   *
   * The encapsulation is defined in ISO/IEC 23008-12:2022 Annex E.
   */
  heif_compression_AVC = 2,
  /**
   * JPEG compression.
   *
   * The compression format is defined in ISO/IEC 10918-1. The encapsulation
   * of JPEG is specified in ISO/IEC 23008-12:2022 Annex H.
  */
  heif_compression_JPEG = 3,
  /**
   * AV1 compression, used for AVIF images.
   *
   * The compression format is provided at https://aomediacodec.github.io/av1-spec/
   *
   * The encapsulation is defined in https://aomediacodec.github.io/av1-avif/
   */
  heif_compression_AV1 = 4,
  /**
   * VVC compression.
   *
   * The compression format is defined in ISO/IEC 23090-3. This is equivalent to H.266.
   *
   * The encapsulation is defined in ISO/IEC 23008-12:2022 Annex L.
   */
  heif_compression_VVC = 5,
  /**
   * EVC compression. (Currently unused in libheif.)
   *
   * The compression format is defined in ISO/IEC 23094-1.
   *
   * The encapsulation is defined in ISO/IEC 23008-12:2022 Annex M.
   */
  heif_compression_EVC = 6,
  /**
   * JPEG 2000 compression.
   *
   * The encapsulation of JPEG 2000 is specified in ISO/IEC 15444-16:2021.
   * The core encoding is defined in ISO/IEC 15444-1, or ITU-T T.800.
  */
  heif_compression_JPEG2000 = 7,
  /**
   * Uncompressed encoding.
   *
   * This is defined in ISO/IEC 23001-17:2024.
  */
  heif_compression_uncompressed = 8,
  /**
   * Mask image encoding.
   *
   * See ISO/IEC 23008-12:2022 Section 6.10.2
   */
  heif_compression_mask = 9,
  /**
   * High Throughput JPEG 2000 (HT-J2K) compression.
   *
   * The encapsulation of HT-J2K is specified in ISO/IEC 15444-16:2021.
   * The core encoding is defined in ISO/IEC 15444-15, or ITU-T T.814.
  */
  heif_compression_HTJ2K = 10
};


// ========================= heif_context =========================
// A heif_context represents a HEIF file that has been read.
// In the future, you will also be able to add pictures to a heif_context
// and write it into a file again.


// Allocate a new context for reading HEIF files.
// Has to be freed again with heif_context_free().
LIBHEIF_API
heif_context* heif_context_alloc(void);

// Free a previously allocated HEIF context. You should not free a context twice.
LIBHEIF_API
void heif_context_free(heif_context*);


typedef struct heif_reading_options heif_reading_options;

enum heif_reader_grow_status
{
  heif_reader_grow_status_size_reached,    // requested size has been reached, we can read until this point
  heif_reader_grow_status_timeout,         // size has not been reached yet, but it may still grow further (deprecated)
  heif_reader_grow_status_size_beyond_eof, // size has not been reached and never will. The file has grown to its full size
  heif_reader_grow_status_error            // an error has occurred
};


typedef struct heif_reader_range_request_result
{
  enum heif_reader_grow_status status; // should not return 'heif_reader_grow_status_timeout'

  // Indicates up to what position the file has been read.
  // If we cannot read the whole file range (status == 'heif_reader_grow_status_size_beyond_eof'), this is the actual end position.
  // On the other hand, it may be that the reader was reading more data than requested. In that case, it should indicate the full size here
  // and libheif may decide to make use of the additional data (e.g. for filling 'tili' offset tables).
  uint64_t range_end;

  // for status == 'heif_reader_grow_status_error'
  int reader_error_code;        // a reader specific error code
  const char* reader_error_msg; // libheif will call heif_reader.release_error_msg on this if it is not NULL
} heif_reader_range_request_result;


typedef struct heif_reader
{
  // API version supported by this reader
  int reader_api_version;

  // --- version 1 functions ---
  int64_t (* get_position)(void* userdata);

  // The functions read(), and seek() return 0 on success.
  // Generally, libheif will make sure that we do not read past the file size.
  int (* read)(void* data,
               size_t size,
               void* userdata);

  int (* seek)(int64_t position,
               void* userdata);

  // When calling this function, libheif wants to make sure that it can read the file
  // up to 'target_size'. This is useful when the file is currently downloaded and may
  // grow with time. You may, for example, extract the image sizes even before the actual
  // compressed image data has been completely downloaded.
  //
  // Even if your input files will not grow, you will have to implement at least
  // detection whether the target_size is above the (fixed) file length
  // (in this case, return 'size_beyond_eof').
  enum heif_reader_grow_status (* wait_for_file_size)(int64_t target_size, void* userdata);

  // --- version 2 functions ---

  // These two functions are for applications that want to stream HEIF files on demand.
  // For example, a large HEIF file that is served over HTTPS and we only want to download
  // it partially to decode individual tiles.
  // If you do not have this use case, you do not have to implement these functions and
  // you can set them to NULL. For simple linear loading, you may use the 'wait_for_file_size'
  // function above instead.

  // If this function is defined, libheif will often request a file range before accessing it.
  // The purpose of this function is that libheif will usually read very small chunks of data with the
  // read() callback above. However, it is inefficient to request such a small chunk of data over a network
  // and the network delay will significantly increase the decoding time.
  // Thus, libheif will call request_range() with a larger block of data that should be preloaded and the
  // subsequent read() calls will work within the requested ranges.
  //
  // Note: `end_pos` is one byte after the last position to be read.
  // You should return
  // - 'heif_reader_grow_status_size_reached' if the requested range is available, or
  // - 'heif_reader_grow_status_size_beyond_eof' if the requested range exceeds the file size
  //   (the valid part of the range has been read).
  heif_reader_range_request_result (* request_range)(uint64_t start_pos, uint64_t end_pos, void* userdata);

  // libheif might issue hints when it assumes that a file range might be needed in the future.
  // This may happen, for example, when your are doing selective tile accesses and libheif proposes
  // to preload offset pointer tables.
  // Another difference to request_file_range() is that this call should be non-blocking.
  // If you preload any data, do this in a background thread.
  void (* preload_range_hint)(uint64_t start_pos, uint64_t end_pos, void* userdata);

  // If libheif does not need access to a file range anymore, it may call this function to
  // give a hint to the reader that it may release the range from a cache.
  // If you do not maintain a file cache that wants to reduce its size dynamically, you do not
  // need to implement this function.
  void (* release_file_range)(uint64_t start_pos, uint64_t end_pos, void* userdata);

  // Release an error message that was returned by heif_reader in an earlier call.
  // If this function is NULL, the error message string will not be released.
  // This is a viable option if you are only returning static strings.
  void (* release_error_msg)(const char* msg);
} heif_reader;


// Read a HEIF file from a named disk file.
// The heif_reading_options should currently be set to NULL.
LIBHEIF_API
heif_error heif_context_read_from_file(heif_context*, const char* filename,
                                       const heif_reading_options*);

// Read a HEIF file stored completely in memory.
// The heif_reading_options should currently be set to NULL.
// DEPRECATED: use heif_context_read_from_memory_without_copy() instead.
LIBHEIF_API
heif_error heif_context_read_from_memory(heif_context*,
                                         const void* mem, size_t size,
                                         const heif_reading_options*);

// Same as heif_context_read_from_memory() except that the provided memory is not copied.
// That means, you will have to keep the memory area alive as long as you use the heif_context.
LIBHEIF_API
heif_error heif_context_read_from_memory_without_copy(heif_context*,
                                                      const void* mem, size_t size,
                                                      const heif_reading_options*);

LIBHEIF_API
heif_error heif_context_read_from_reader(heif_context*,
                                         const heif_reader* reader,
                                         void* userdata,
                                         const heif_reading_options*);

// Number of top-level images in the HEIF file. This does not include the thumbnails or the
// tile images that are composed to an image grid. You can get access to the thumbnails via
// the main image handle.
LIBHEIF_API
int heif_context_get_number_of_top_level_images(heif_context* ctx);

LIBHEIF_API
int heif_context_is_top_level_image_ID(heif_context* ctx, heif_item_id id);

// Fills in image IDs into the user-supplied int-array 'ID_array', preallocated with 'count' entries.
// Function returns the total number of IDs filled into the array.
LIBHEIF_API
int heif_context_get_list_of_top_level_image_IDs(heif_context* ctx,
                                                 heif_item_id* ID_array,
                                                 int count);

LIBHEIF_API
heif_error heif_context_get_primary_image_ID(heif_context* ctx, heif_item_id* id);

// Get a handle to the primary image of the HEIF file.
// This is the image that should be displayed primarily when there are several images in the file.
LIBHEIF_API
heif_error heif_context_get_primary_image_handle(heif_context* ctx,
                                                 heif_image_handle**);

// Get the image handle for a known image ID.
LIBHEIF_API
heif_error heif_context_get_image_handle(heif_context* ctx,
                                         heif_item_id id,
                                         heif_image_handle**);

// Print information about the boxes of a HEIF file to file descriptor.
// This is for debugging and informational purposes only. You should not rely on
// the output having a specific format. At best, you should not use this at all.
LIBHEIF_API
void heif_context_debug_dump_boxes_to_file(heif_context* ctx, int fd);

// ====================================================================================================
//   Write the heif_context to a HEIF file

LIBHEIF_API
heif_error heif_context_write_to_file(heif_context*,
                                      const char* filename);

typedef struct heif_writer
{
  // API version supported by this writer
  int writer_api_version;

  // --- version 1 functions ---

  // On success, the returned heif_error may have a NULL message. It will automatically be replaced with a "Success" string.
  heif_error (* write)(heif_context* ctx, // TODO: why do we need this parameter?
                       const void* data,
                       size_t size,
                       void* userdata);
} heif_writer;


LIBHEIF_API
heif_error heif_context_write(heif_context*,
                              heif_writer* writer,
                              void* userdata);

#ifdef __cplusplus
}
#endif

#endif
