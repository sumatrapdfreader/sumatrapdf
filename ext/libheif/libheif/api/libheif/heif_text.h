/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 * Copyright (c) 2025 Brad Hards <bradh@frogmouth.net>
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

#ifndef LIBHEIF_HEIF_TEXT_H
#define LIBHEIF_HEIF_TEXT_H

#include "heif_image_handle.h"
#include "heif_library.h"
#include "heif_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- text items and fonts

// See ISO/IEC 23008-12:2025 Section 6.10 "Text and font items"

typedef struct heif_text_item heif_text_item;

/**
 * Get the number of text items that are attached to an image.
 *
 * @param image_handle the image handle for the image to query.
 * @return the number of text items, which can be zero.
 */
LIBHEIF_API
int heif_image_handle_get_number_of_text_items(const heif_image_handle* image_handle);

/**
 * Get the text item identifiers for the text items attached to an image.
 *
 * Possible usage (in C++):
 * @code
 *  int numTextItems = heif_image_handle_get_number_of_text_items(handle);
 *  if (numTextItems > 0) {
 *      std::vector<heif_item_id> text_item_ids(numTextItems);
 *      heif_image_handle_get_list_of_text_item_ids(handle, text_item_ids.data(), numTextItems);
 *      // use text item ids
 *  }
 * @endcode
 *
 * @param image_handle the image handle for the parent image to query
 * @param text_item_ids_array array to put the item identifiers into
 * @param max_count the maximum number of text identifiers
 * @return the number of text item identifiers that were returned.
 */
LIBHEIF_API
int heif_image_handle_get_list_of_text_item_ids(const heif_image_handle* image_handle,
                                                heif_item_id* text_item_ids_array,
                                                int max_count);


/**
 * Get the text item.
 *
 * Caller is responsible for release of the output heif_text_item with heif_text_item_release().
 *
 * @param context the context to get the text item from, usually from a file operation
 * @param text_item_id the identifier for the text item
 * @param out pointer to pointer to the resulting text item
 * @return heif_error_ok on success, or an error value indicating the problem
 */
LIBHEIF_API
heif_error heif_context_get_text_item(const heif_context* context,
                                      heif_item_id text_item_id,
                                      heif_text_item** out);

/**
 * Get the item identifier for a text item.
 *
 * @param text_item the text item to query
 * @return the text item identifier (or 0 if the text_item is null)
 */
LIBHEIF_API
heif_item_id heif_text_item_get_id(heif_text_item* text_item);

/**
 * Get the item content for a text item.
 *
 * This is the payload text, in the format given by the associated content_type.
 *
 * @param text_item the text item to query
 * @return the text item content (or null if the text_item is null). The returned string shall be released
 * with heif_string_release().
 */
LIBHEIF_API
const char* heif_text_item_get_content(heif_text_item* text_item);

/**
 * This function is similar to heif_item_get_property_extended_language(), but
 * takes a `heif_text_item` as parameter.
 *
 * @param text_item The text item for which we are requesting the language.
 * @param out_language Output parameter for the text language. Free with heif_string_release().
 * @return
 */
LIBHEIF_API
heif_error heif_text_item_get_property_extended_language(const heif_text_item* text_item,
                                                         char** out_language);

// --- adding text items

/**
 * Add a text item to an image.
*/
LIBHEIF_API
heif_error heif_image_handle_add_text_item(heif_image_handle *image_handle,
                                           const char *content_type,
                                           const char *text,
                                           heif_text_item** out_text_item);

/**
 * Release a text item.
 *
 * This should be called on items from heif_context_add_text_item().
 *
 * @param text_item the item to release.
 */
LIBHEIF_API
void heif_text_item_release(heif_text_item* text_item);

/**
 * Set the extended language property to the text item.
 *
 * This adds an RFC 5346 (IETF BCP 47) extended language tag, such as "en-AU".
 *
 * @param text_item the text item to query
 * @param language the language to set
 * @param out_optional_propertyId Output parameter for the property ID of the language property.
 *                                This parameter may be NULL if the info is not required.
 * @return heif_error_ok on success, or an error value indicating the problem
 */
LIBHEIF_API
heif_error heif_text_item_set_extended_language(heif_text_item* text_item,
                                                const char *language,
                                                heif_property_id* out_optional_propertyId);

#ifdef __cplusplus
}
#endif

#endif
