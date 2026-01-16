/*
 * HEIF codec.
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_TAI_TIMESTAMPS_H
#define LIBHEIF_HEIF_TAI_TIMESTAMPS_H

#include <libheif/heif.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct heif_tai_clock_info
{
  uint8_t version;

  // --- version 1

  // Standard deviation for timestamp generation process.
  // May be `heif_tai_clock_info_time_uncertainty_unknown` if unknown.
  uint64_t time_uncertainty;

  // Receptor clock resolution in nanoseconds.
  uint32_t clock_resolution;

  // Clock drift rate in picoseconds/second when synchronization is stopped.
  // Maybe `heif_tai_clock_info_clock_drift_rate_unknown` if unknown.
  int32_t clock_drift_rate;

  // Whether clock is synchronized to an atomic source,
  // see the clock_type defines below.
  uint8_t clock_type;
} heif_tai_clock_info;

#define heif_tai_clock_info_time_uncertainty_unknown UINT64_C(0xFFFFFFFFFFFFFFFF)
#define heif_tai_clock_info_clock_drift_rate_unknown INT32_C(0x7FFFFFFF)
#define heif_tai_clock_info_clock_type_unknown 0
#define heif_tai_clock_info_clock_type_not_synchronized_to_atomic_source 1
#define heif_tai_clock_info_clock_type_synchronized_to_atomic_source 2


/**
 * Allocate a new heif_tai_clock_info object and initialize with default values.
 */
LIBHEIF_API
heif_tai_clock_info* heif_tai_clock_info_alloc(void);

/**
 * Copies the source object into the destination object.
 * Only the fields that are present in both objects are copied.
 * The version property has to be set in both structs.
 */
LIBHEIF_API
void heif_tai_clock_info_copy(heif_tai_clock_info* dst, const heif_tai_clock_info* src);

LIBHEIF_API
void heif_tai_clock_info_release(heif_tai_clock_info* clock_info);


typedef struct heif_tai_timestamp_packet
{
  uint8_t version;

  // --- version 1

  // number of nanoseconds since TAI epoch (1958-01-01T00:00:00.0)
  uint64_t tai_timestamp;

  // whether the remote and receiptor clocks are in sync
  uint8_t synchronization_state;         // bool

  // whether the receptor clock failed to generate a timestamp
  uint8_t timestamp_generation_failure;  // bool

  // whether the original clock value has been modified
  uint8_t timestamp_is_modified;         // bool
} heif_tai_timestamp_packet;

/**
 * Allocate a new heif_tai_timestamp_packet object and initialize with default values.
 */
LIBHEIF_API
heif_tai_timestamp_packet* heif_tai_timestamp_packet_alloc(void);

/**
 * Copies the source object into the destination object.
 * Only the fields that are present in both objects are copied.
 * The version property has to be set in both structs.
 */
LIBHEIF_API
void heif_tai_timestamp_packet_copy(heif_tai_timestamp_packet* dst, const heif_tai_timestamp_packet* src);

LIBHEIF_API
void heif_tai_timestamp_packet_release(heif_tai_timestamp_packet*);



/**
 * Creates a new clock info property if it doesn't exist yet.
 * You can only add one tai_clock_info to an image.
 *
 * @param clock_info The TAI clock info to set for the item. This object will be copied.
 * @param out_optional_propertyId Output parameter for the property ID of the tai_clock_info.
 *                                This parameter may be NULL if the info is not required.
 */
LIBHEIF_API
heif_error heif_item_set_property_tai_clock_info(heif_context* ctx,
                                                 heif_item_id itemId,
                                                 const heif_tai_clock_info* clock_info,
                                                 heif_property_id* out_optional_propertyId);

/**
 * Get the heif_tai_clock_info attached to the item.
 * This function allocates a new heif_tai_clock_info and returns it through out_clock.
 *
 * @param out_clock This parameter must not be nullptr. The object returned through this parameter must
 *                  be released with heif_tai_clock_info_release().
 *                  If no tai_clock_info property exists for the item, out_clock is set to NULL and
 *                  no error is returned.
 */
LIBHEIF_API
heif_error heif_item_get_property_tai_clock_info(const heif_context* ctx,
                                                 heif_item_id itemId,
                                                 heif_tai_clock_info** out_clock);


/**
 * Creates a new TAI timestamp property if it doesn't exist yet.
 * You can only add one tai_timestamp to an image.
 *
 * @param timestamp The TAI timestamp to set for the item. This object will be copied.
 * @param out_optional_propertyId Output parameter for the property ID of the TAI timestamp.
 *                                This parameter may be NULL if the info is not required.
 */
LIBHEIF_API
heif_error heif_item_set_property_tai_timestamp(heif_context* ctx,
                                                heif_item_id itemId,
                                                const heif_tai_timestamp_packet* timestamp,
                                                heif_property_id* out_optional_propertyId);

/**
 * Get the heif_tai_timestamp_packet attached to the item.
 * This function allocates a new heif_tai_timestamp_packet and returns it through out_timestamp.
 *
 * @param out_timestamp This parameter must not be NULL. The object returned through this parameter must
 *                  be released with heif_tai_timestamp_packet_release().
 *                  If no tai_timestamp_packet property exists for the item, *out_timestamp is set to NULL and
 *                  no error is returned.
 */
LIBHEIF_API
heif_error heif_item_get_property_tai_timestamp(const heif_context* ctx,
                                                heif_item_id itemId,
                                                heif_tai_timestamp_packet** out_timestamp);

/**
 * Attach a TAI timestamp to the image.
 * The main use of this function is for image sequences, but it can also be used for still images.
 * If used for still images, note that you also have to set the heif_tai_clock_info to the image item
 * through heif_item_set_property_tai_clock_info().
 *
 * @param timestamp The TAI timestamp to set to the image. This object will be copied.
 */
LIBHEIF_API
heif_error heif_image_set_tai_timestamp(heif_image* img,
                                        const heif_tai_timestamp_packet* timestamp);

/**
 * Get the heif_tai_timestamp_packet attached to the image.
 * The main use of this function is for image sequences, but it can also be used for still images.
 * This function allocates a new heif_tai_timestamp_packet and returns it through out_timestamp.
 *
 * @param out_timestamp This parameter must not be NULL. The object returned through this parameter must
 *                  be released with heif_tai_timestamp_packet_release().
 *                  If no tai_timestamp_packet property exists for the image, *out_timestamp is set to NULL and
 *                  no error is returned.
 */
LIBHEIF_API
heif_error heif_image_get_tai_timestamp(const heif_image* img,
                                        heif_tai_timestamp_packet** out_timestamp);

#ifdef __cplusplus
}
#endif

#endif //LIBHEIF_HEIF_TAI_TIMESTAMPS_H
