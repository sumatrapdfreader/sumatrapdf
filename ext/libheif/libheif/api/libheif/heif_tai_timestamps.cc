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

#include <libheif/heif_tai_timestamps.h>
#include "api_structs.h"
#include "box.h"
#include "file.h"
#include <memory>


void initialize_heif_tai_clock_info(heif_tai_clock_info* taic)
{
  taic->version = 1;
  taic->time_uncertainty = heif_tai_clock_info_time_uncertainty_unknown;
  taic->clock_resolution = 0;
  taic->clock_drift_rate = heif_tai_clock_info_clock_drift_rate_unknown;
  taic->clock_type = heif_tai_clock_info_clock_type_unknown;
}

heif_tai_clock_info* heif_tai_clock_info_alloc()
{
  auto* taic = new heif_tai_clock_info;
  initialize_heif_tai_clock_info(taic);

  return taic;
}


void initialize_heif_tai_timestamp_packet(heif_tai_timestamp_packet* itai)
{
  itai->version = 1;
  itai->tai_timestamp = 0;
  itai->synchronization_state = false;
  itai->timestamp_generation_failure = false;
  itai->timestamp_is_modified = false;
}

heif_tai_timestamp_packet* heif_tai_timestamp_packet_alloc()
{
  auto* itai = new heif_tai_timestamp_packet;
  initialize_heif_tai_timestamp_packet(itai);

  return itai;
}


void heif_tai_timestamp_packet_copy(heif_tai_timestamp_packet* dst, const heif_tai_timestamp_packet* src)
{
  if (dst->version >= 1 && src->version >= 1) {
    dst->tai_timestamp = src->tai_timestamp;
    dst->synchronization_state = src->synchronization_state;
    dst->timestamp_is_modified = src->timestamp_is_modified;
    dst->timestamp_generation_failure = src->timestamp_generation_failure;
  }

  // in the future when copying with "src->version > dst->version",
  // the remaining dst fields have to be filled with defaults
}

void heif_tai_clock_info_copy(heif_tai_clock_info* dst, const heif_tai_clock_info* src)
{
  if (dst->version >= 1 && src->version >= 1) {
    dst->time_uncertainty = src->time_uncertainty;
    dst->clock_resolution = src->clock_resolution;
    dst->clock_drift_rate = src->clock_drift_rate;
    dst->clock_type = src->clock_type;
  }

  // in the future when copying with "src->version > dst->version",
  // the remaining dst fields have to be filled with defaults
}


heif_error heif_item_set_property_tai_clock_info(heif_context* ctx,
                                                 heif_item_id itemId,
                                                 const heif_tai_clock_info* clock,
                                                 heif_property_id* out_propertyId)
{
  if (!ctx || !clock) {
    return heif_error_null_pointer_argument;
  }

  // Check if itemId exists
  auto file = ctx->context->get_heif_file();
  if (!file->item_exists(itemId)) {
    return {heif_error_Input_does_not_exist, heif_suberror_Invalid_parameter_value, "itemId does not exist"};
  }

  // make sure that we do not add two taic boxes to one image

  if (auto img = ctx->context->get_image(itemId, false)) {
    auto existing_taic = img->get_property<Box_taic>();
    if (existing_taic) {
      return {heif_error_Usage_error, heif_suberror_Invalid_parameter_value, "item already has an taic property"};
    }
  }

  // Create new taic (it will be deduplicated automatically in add_property())

  auto taic = std::make_shared<Box_taic>();
  taic->set_from_tai_clock_info(clock);

  heif_property_id id = ctx->context->add_property(itemId, taic, false);

  if (out_propertyId) {
    *out_propertyId = id;
  }

  return heif_error_success;
}


heif_error heif_item_get_property_tai_clock_info(const heif_context* ctx,
                                                 heif_item_id itemId,
                                                 heif_tai_clock_info** out_clock)
{
  if (!ctx || !out_clock) {
    return heif_error_null_pointer_argument;
  }

  *out_clock = nullptr;

  // Check if itemId exists
  auto file = ctx->context->get_heif_file();
  if (!file->item_exists(itemId)) {
    return {heif_error_Input_does_not_exist, heif_suberror_Invalid_parameter_value, "item ID does not exist"};
  }

  // Check if taic exists for itemId
  auto taic = file->get_property_for_item<Box_taic>(itemId);
  if (!taic) {
    // return NULL heif_tai_clock_info
    return heif_error_success;
  }

  *out_clock = new heif_tai_clock_info;
  **out_clock = *taic->get_tai_clock_info();

  return heif_error_success;
}


void heif_tai_clock_info_release(heif_tai_clock_info* clock_info)
{
  delete clock_info;
}


void heif_tai_timestamp_packet_release(heif_tai_timestamp_packet* tai)
{
  delete tai;
}


heif_error heif_item_set_property_tai_timestamp(heif_context* ctx,
                                                heif_item_id itemId,
                                                const heif_tai_timestamp_packet* timestamp,
                                                heif_property_id* out_propertyId)
{
  if (!ctx) {
    return heif_error_null_pointer_argument;
  }

  // Check if itemId exists
  auto file = ctx->context->get_heif_file();
  if (!file->item_exists(itemId)) {
    return {heif_error_Input_does_not_exist, heif_suberror_Invalid_parameter_value, "item does not exist"};
  }

  // make sure that we do not add two TAI timestamps to one image

  if (auto img = ctx->context->get_image(itemId, false)) {
    auto existing_itai = img->get_property<Box_itai>();
    if (existing_itai) {
      return {heif_error_Usage_error, heif_suberror_Invalid_parameter_value, "item already has an itai property"};
    }
  }

  // Create new itai (it will be deduplicated automatically in add_property())

  auto itai = std::make_shared<Box_itai>();
  itai->set_from_tai_timestamp_packet(timestamp);

  heif_property_id id = ctx->context->add_property(itemId, itai, false);

  if (out_propertyId) {
    *out_propertyId = id;
  }

  return heif_error_success;
}


heif_error heif_item_get_property_tai_timestamp(const heif_context* ctx,
                                                heif_item_id itemId,
                                                heif_tai_timestamp_packet** out_timestamp)
{
  if (!ctx || !out_timestamp) {
    return heif_error_null_pointer_argument;
  }

  *out_timestamp = nullptr;

  // Check if itemId exists
  auto file = ctx->context->get_heif_file();
  if (!file->item_exists(itemId)) {
    return {heif_error_Input_does_not_exist, heif_suberror_Invalid_parameter_value, "item does not exist"};
  }

  // Check if itai exists for itemId
  auto itai = file->get_property_for_item<Box_itai>(itemId);
  if (!itai) {
    // return NULL heif_tai_timestamp_packet;
    return heif_error_success;
  }

  *out_timestamp = new heif_tai_timestamp_packet;
  **out_timestamp = *itai->get_tai_timestamp_packet();

  return heif_error_success;
}


heif_error heif_image_set_tai_timestamp(heif_image* img,
                                        const heif_tai_timestamp_packet* timestamp)
{
  Error err = img->image->set_tai_timestamp(timestamp);
  if (err) {
    return err.error_struct(img->image.get());
  }
  else {
    return heif_error_success;
  }
}


heif_error heif_image_get_tai_timestamp(const heif_image* img,
                                        heif_tai_timestamp_packet** out_timestamp)
{
  if (!out_timestamp) {
    return heif_error_null_pointer_argument;
  }

  *out_timestamp = nullptr;

  auto* tai = img->image->get_tai_timestamp();
  if (!tai) {
    *out_timestamp = nullptr;
    return heif_error_success;
  }

  *out_timestamp = new heif_tai_timestamp_packet;
  **out_timestamp = *tai;

  return heif_error_success;
}
