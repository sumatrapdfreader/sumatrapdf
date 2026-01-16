/*
 * HEIF codec.
 * Copyright (c) 2017-2023 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_METADATA_H
#define LIBHEIF_HEIF_METADATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libheif/heif_library.h>


enum heif_metadata_compression
{
  heif_metadata_compression_off = 0,
  heif_metadata_compression_auto = 1,
  heif_metadata_compression_unknown = 2, // only used when reading unknown method from input file
  heif_metadata_compression_deflate = 3,
  heif_metadata_compression_zlib = 4,    // do not use for header data
  heif_metadata_compression_brotli = 5
};

LIBHEIF_API
int heif_metadata_compression_method_supported(enum heif_metadata_compression method);

// ------------------------- metadata (Exif / XMP) -------------------------

// How many metadata blocks are attached to an image. If you only want to get EXIF data,
// set the type_filter to "Exif". Otherwise, set the type_filter to NULL.
LIBHEIF_API
int heif_image_handle_get_number_of_metadata_blocks(const heif_image_handle* handle,
                                                    const char* type_filter);

// 'type_filter' can be used to get only metadata of specific types, like "Exif".
// If 'type_filter' is NULL, it will return all types of metadata IDs.
LIBHEIF_API
int heif_image_handle_get_list_of_metadata_block_IDs(const heif_image_handle* handle,
                                                     const char* type_filter,
                                                     heif_item_id* ids, int count);

// Return a string indicating the type of the metadata, as specified in the HEIF file.
// Exif data will have the type string "Exif".
// This string will be valid until the next call to a libheif function.
// You do not have to free this string.
LIBHEIF_API
const char* heif_image_handle_get_metadata_type(const heif_image_handle* handle,
                                                heif_item_id metadata_id);

// For EXIF, the content type is empty.
// For XMP, the content type is "application/rdf+xml".
LIBHEIF_API
const char* heif_image_handle_get_metadata_content_type(const heif_image_handle* handle,
                                                        heif_item_id metadata_id);

// Get the size of the raw metadata, as stored in the HEIF file.
LIBHEIF_API
size_t heif_image_handle_get_metadata_size(const heif_image_handle* handle,
                                           heif_item_id metadata_id);

// 'out_data' must point to a memory area of the size reported by heif_image_handle_get_metadata_size().
// The data is returned exactly as stored in the HEIF file.
// For Exif data, you probably have to skip the first four bytes of the data, since they
// indicate the offset to the start of the TIFF header of the Exif data.
LIBHEIF_API
heif_error heif_image_handle_get_metadata(const heif_image_handle* handle,
                                          heif_item_id metadata_id,
                                          void* out_data);

// Only valid for item type == "uri ", an absolute URI
LIBHEIF_API
const char* heif_image_handle_get_metadata_item_uri_type(const heif_image_handle* handle,
                                                         heif_item_id metadata_id);

// --- writing Exif / XMP metadata ---

// Add EXIF metadata to an image.
LIBHEIF_API
heif_error heif_context_add_exif_metadata(heif_context*,
                                          const heif_image_handle* image_handle,
                                          const void* data, int size);

// Add XMP metadata to an image.
LIBHEIF_API
heif_error heif_context_add_XMP_metadata(heif_context*,
                                         const heif_image_handle* image_handle,
                                         const void* data, int size);

// New version of heif_context_add_XMP_metadata() with data compression (experimental).
LIBHEIF_API
heif_error heif_context_add_XMP_metadata2(heif_context*,
                                          const heif_image_handle* image_handle,
                                          const void* data, int size,
                                          enum heif_metadata_compression compression);

// Add generic, proprietary metadata to an image. You have to specify an 'item_type' that will
// identify your metadata. 'content_type' can be an additional type, or it can be NULL.
// For example, this function can be used to add IPTC metadata (IIM stream, not XMP) to an image.
// Although not standard, we propose to store IPTC data with item type="iptc", content_type=NULL.
LIBHEIF_API
heif_error heif_context_add_generic_metadata(heif_context* ctx,
                                             const heif_image_handle* image_handle,
                                             const void* data, int size,
                                             const char* item_type, const char* content_type);

// Add generic metadata with item_type "uri ". Items with this type do not have a content_type, but
// an item_uri_type and they have no content_encoding (they are always stored uncompressed).
LIBHEIF_API
heif_error heif_context_add_generic_uri_metadata(heif_context* ctx,
                                                 const heif_image_handle* image_handle,
                                                 const void* data, int size,
                                                 const char* item_uri_type,
                                                 heif_item_id* out_item_id);

#ifdef __cplusplus
}
#endif

#endif
