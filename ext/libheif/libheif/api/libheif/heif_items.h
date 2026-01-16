/*
 * HEIF codec.
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

#ifndef LIBHEIF_HEIF_ITEMS_H
#define LIBHEIF_HEIF_ITEMS_H

#include "heif_library.h"
#include "heif_error.h"
#include "heif_metadata.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Gets the number of items.
 *
 * This is not the same as the number of images, since there can be other types of items,
 * such as metadata.
 *
 * @param ctx the file context
 * @return the number of items
 */
LIBHEIF_API
int heif_context_get_number_of_items(const heif_context* ctx);

/**
 * Get the item identifiers.
 *
 * Fills in the item IDs into the user-supplied array {@code ID_array}, preallocated with {@code count} entries.
 *
 * @param ctx the file context
 * @param ID_array the output array.
 * @param count the number of items allocated within {@code ID_array}.
 * @return the total number of IDs filled into the array, which may be less than {@code count}.
 */
LIBHEIF_API
int heif_context_get_list_of_item_IDs(const heif_context* ctx,
                                      heif_item_id* ID_array,
                                      int count);

/**
 * Gets the item type.
 *
 * Usually, this is a four character code (e.g. `mime` or `uri `), but it can theoretically be
 * any 4-byte number. Thus, the type is returned as an integer. You can use {@link heif_fourcc} to map
 * between the two representations.
 *
 * @param ctx the file context
 * @param item_id the item identifier for the item
 * @return the item type
 */
LIBHEIF_API
uint32_t heif_item_get_item_type(const heif_context* ctx, heif_item_id item_id);

#define heif_item_type_mime   heif_fourcc('m','i','m','e')
#define heif_item_type_uri    heif_fourcc('u','r','i',' ')

LIBHEIF_API
int heif_item_is_item_hidden(const heif_context* ctx, heif_item_id item_id);


/**
 * Gets the MIME content_type for an item.
 *
 * Only valid if the item type is `mime`.
 * If the item does not exist, or if it is not a `mime` item, NULL is returned.
 *
 * @param ctx the file context
 * @param item_id the item identifier for the item
 * @return the item content_type
 */
LIBHEIF_API
const char* heif_item_get_mime_item_content_type(const heif_context* ctx, heif_item_id item_id);

/**
 * Gets the content_encoding for a MIME item.
 *
 * Only valid if the item type is `mime`.
 * If the item does not exist, or if it is not a `mime` item, NULL is returned.
 *
 * If the item is not encoded, the returned value will be an empty string (not null).
 *
 * @param ctx the file context
 * @param item_id the item identifier for the item
 * @return the item content_type
 */
LIBHEIF_API
const char* heif_item_get_mime_item_content_encoding(const heif_context* ctx, heif_item_id item_id);

/**
 * Gets the item_uri_type for an item.
 *
 * Only valid if the item type is `uri `.
 * If the item does not exist, or if it is not a `uri ` item, NULL is returned.
 *
 * @param ctx the file context
 * @param item_id the item identifier for the item
 * @return the item item_uri_type
 */
LIBHEIF_API
const char* heif_item_get_uri_item_uri_type(const heif_context* ctx, heif_item_id item_id);

LIBHEIF_API
const char* heif_item_get_item_name(const heif_context* ctx, heif_item_id item_id);

LIBHEIF_API
heif_error heif_item_set_item_name(heif_context* ctx,
                                   heif_item_id item,
                                   const char* item_name);


/**
 * Gets the raw metadata, as stored in the HEIF file.
 *
 * Data in a "mime" item with "content_encoding" can be compressed.
 * When `out_compression_format` is NULL, the decompressed data will be returned.
 * Otherwise, the compressed data is returned and `out_compression_format` will be filled with the
 * compression format.
 * If the compression method is not supported, an error will be returned.
 *
 * It is valid to set `out_data` to NULL. In that case, only the `out_data_size` is filled.
 * Note that it is inefficient to use `out_data=NULL` just to get the size of compressed data.
 * In general, this should be avoided.
 *
 * If there is no data assigned to the item or there is an error, `out_data_size` is set to zero.
 *
 * @param ctx the file context
 * @param item_id the item identifier for the item
 * @param out_compression_format how the data is compressed. If the pointer is NULL, the decompressed data will be returned.
 * @param out_data the corresponding raw metadata
 * @param out_data_size the size of the metadata in bytes
 * @return whether the call succeeded, or there was an error
 */
LIBHEIF_API
heif_error heif_item_get_item_data(const heif_context* ctx,
                                   heif_item_id item_id,
                                   enum heif_metadata_compression* out_compression_format,
                                   uint8_t** out_data, size_t* out_data_size);

/**
 * Free the item data.
 *
 * This is used to free memory associated with the data returned by
 * {@link heif_item_get_item_data} in 'out_data' and set the pointer to NULL.
 *
 * @param ctx the file context
 * @param item_data the data to free
 */
LIBHEIF_API
void heif_release_item_data(const heif_context* ctx, uint8_t** item_data);


// ------------------------- item language -------------------------

/**
 * Get the extended language associated with the item.
 * The item is usually a text item.
 *
 * @param context the heif file context containg the item.
 * @param itemId the identifier for the item
 * @param out_language output parameter with the item's language. Free with heif_string_release().
 * @return heif_error_ok on success, or an error value indicating the problem
 */
LIBHEIF_API
heif_error heif_item_get_property_extended_language(const heif_context* context,
                                                    heif_item_id itemId,
                                                    char** out_language);

LIBHEIF_API
heif_error heif_item_set_property_extended_language(heif_context* context,
                                                    heif_item_id item_id,
                                                    const char* language, heif_property_id* out_optional_propertyId);

// ------------------------- item references -------------------------

/**
 * Get the item ids that reference the given item.
 *
 * @param ctx the file context.
 * @param from_item_id the item identifier for the item.
 * @param index the index of the reference to get.
 * @param out_reference_type_4cc The 4cc of the reference. (e.g dimg, thmb, cdsc, or auxl)
 * @param out_references_to the item references. Use {@link heif_release_item_references} to free the memory.
 * @return the number of items that reference the given item. Returns 0 if the index exceeds the number of references.
 */
LIBHEIF_API
size_t heif_context_get_item_references(const heif_context* ctx,
                                        heif_item_id from_item_id,
                                        int index,
                                        uint32_t* out_reference_type_4cc,
                                        heif_item_id** out_references_to);

LIBHEIF_API
void heif_release_item_references(const heif_context* ctx, heif_item_id** references);

LIBHEIF_API
heif_error heif_context_add_item_reference(heif_context* ctx,
                                           uint32_t reference_type,
                                           heif_item_id from_item,
                                           heif_item_id to_item);

LIBHEIF_API
heif_error heif_context_add_item_references(heif_context* ctx,
                                            uint32_t reference_type,
                                            heif_item_id from_item,
                                            const heif_item_id* to_item,
                                            int num_to_items);

// ------------------------- adding new items -------------------------

LIBHEIF_API
heif_error heif_context_add_item(heif_context* ctx,
                                 const char* item_type,
                                 const void* data, int size,
                                 heif_item_id* out_item_id);

LIBHEIF_API
heif_error heif_context_add_mime_item(heif_context* ctx,
                                      const char* content_type,
                                      enum heif_metadata_compression content_encoding,
                                      const void* data, int size,
                                      heif_item_id* out_item_id);

LIBHEIF_API
heif_error heif_context_add_precompressed_mime_item(heif_context* ctx,
                                                    const char* content_type,
                                                    const char* content_encoding,
                                                    const void* data, int size,
                                                    heif_item_id* out_item_id);

LIBHEIF_API
heif_error heif_context_add_uri_item(heif_context* ctx,
                                     const char* item_uri_type,
                                     const void* data, int size,
                                     heif_item_id* out_item_id);

#ifdef __cplusplus
}
#endif

#endif
