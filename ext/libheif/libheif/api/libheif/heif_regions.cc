/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 * Copyright (c) 2023 Brad Hards <bradh@frogmouth.net>
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

#include "heif_plugin.h"
#include "region.h"
#include "heif_regions.h"
#include "file.h"
#include "api_structs.h"
#include "context.h"
#include <cstring>
#include <memory>
#include <vector>
#include <utility>
#include <algorithm>


int heif_image_handle_get_number_of_region_items(const heif_image_handle* handle)
{
  return (int) handle->image->get_region_item_ids().size();
}

int heif_image_handle_get_list_of_region_item_ids(const heif_image_handle* handle,
                                                  heif_item_id* item_ids,
                                                  int max_count)
{
  auto region_item_ids = handle->image->get_region_item_ids();
  int num = std::min((int) region_item_ids.size(), max_count);

  memcpy(item_ids, region_item_ids.data(), num * sizeof(heif_item_id));

  return num;
}


heif_error heif_context_get_region_item(const heif_context* context,
                                        heif_item_id region_item_id,
                                        heif_region_item** out)
{
  if (out == nullptr) {
    return heif_error_null_pointer_argument;
  }

  auto r = context->context->get_region_item(region_item_id);

  if (r == nullptr) {
    return {heif_error_Usage_error, heif_suberror_Nonexisting_item_referenced, "Region item does not exist"};
  }

  heif_region_item* item = new heif_region_item();
  item->context = context->context;
  item->region_item = std::move(r);
  *out = item;

  return heif_error_success;
}


heif_item_id heif_region_item_get_id(heif_region_item* region_item)
{
  if (region_item == nullptr) {
    return 0;
  }

  return region_item->region_item->item_id;
}


void heif_region_item_release(heif_region_item* region_item)
{
  delete region_item;
}


void heif_region_item_get_reference_size(heif_region_item* region_item, uint32_t* width, uint32_t* height)
{
  auto r = region_item->context->get_region_item(region_item->region_item->item_id);

  if (width) *width = r->reference_width;
  if (height) *height = r->reference_height;
}


int heif_region_item_get_number_of_regions(const heif_region_item* region_item)
{
  return region_item->region_item->get_number_of_regions();
}

int heif_region_item_get_list_of_regions(const heif_region_item* region_item,
                                         heif_region** out_regions,
                                         int max_count)
{
  auto regions = region_item->region_item->get_regions();
  int num = std::min(max_count, (int) regions.size());

  for (int i = 0; i < num; i++) {
    auto region = new heif_region();
    region->context = region_item->context;
    //region->parent_region_item_id = region_item->region_item->item_id;
    region->region_item = region_item->region_item;
    region->region = regions[i];

    out_regions[i] = region;
  }

  return num;
}


void heif_region_release(const heif_region* region)
{
  delete region;
}


void heif_region_release_many(const heif_region* const* regions, int num)
{
  for (int i = 0; i < num; i++) {
    delete regions[i];
  }
}


heif_region_type heif_region_get_type(const heif_region* region)
{
  return region->region->getRegionType();
}


heif_error heif_region_get_point(const heif_region* region, int32_t* x, int32_t* y)
{
  if (!x || !y) {
    return heif_error_null_pointer_argument;
  }

  const std::shared_ptr<RegionGeometry_Point> point = std::dynamic_pointer_cast<RegionGeometry_Point>(region->region);
  if (point) {
    *x = point->x;
    *y = point->y;
    return heif_error_success;
  }

  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_point_transformed(const struct heif_region* region, heif_item_id image_id, double* x, double* y)
{
  if (!x || !y) {
    return heif_error_null_pointer_argument;
  }

  const std::shared_ptr<RegionGeometry_Point> point = std::dynamic_pointer_cast<RegionGeometry_Point>(region->region);
  if (point) {
    auto t = RegionCoordinateTransform::create(region->context->get_heif_file(), image_id,
                                               region->region_item->reference_width,
                                               region->region_item->reference_height);
    RegionCoordinateTransform::Point p = t.transform_point({(double) point->x, (double) point->y});
    *x = p.x;
    *y = p.y;

    return heif_error_success;
  }

  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_rectangle(const heif_region* region,
                                     int32_t* x, int32_t* y,
                                     uint32_t* width, uint32_t* height)
{
  const std::shared_ptr<RegionGeometry_Rectangle> rect = std::dynamic_pointer_cast<RegionGeometry_Rectangle>(region->region);
  if (rect) {
    *x = rect->x;
    *y = rect->y;
    *width = rect->width;
    *height = rect->height;
    return heif_error_success;
  }

  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_rectangle_transformed(const heif_region* region,
                                                 heif_item_id image_id,
                                                 double* x, double* y,
                                                 double* width, double* height)
{
  const std::shared_ptr<RegionGeometry_Rectangle> rect = std::dynamic_pointer_cast<RegionGeometry_Rectangle>(region->region);
  if (rect) {
    auto t = RegionCoordinateTransform::create(region->context->get_heif_file(), image_id,
                                               region->region_item->reference_width,
                                               region->region_item->reference_height);

    RegionCoordinateTransform::Point p = t.transform_point({(double) rect->x, (double) rect->y});
    RegionCoordinateTransform::Extent e = t.transform_extent({(double) rect->width, (double) rect->height});

    *x = p.x;
    *y = p.y;
    *width = e.x;
    *height = e.y;
    return heif_error_success;
  }

  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_ellipse(const heif_region* region,
                                   int32_t* x, int32_t* y,
                                   uint32_t* radius_x, uint32_t* radius_y)
{
  const std::shared_ptr<RegionGeometry_Ellipse> ellipse = std::dynamic_pointer_cast<RegionGeometry_Ellipse>(region->region);
  if (ellipse) {
    *x = ellipse->x;
    *y = ellipse->y;
    *radius_x = ellipse->radius_x;
    *radius_y = ellipse->radius_y;
    return heif_error_success;
  }

  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_ellipse_transformed(const heif_region* region,
                                               heif_item_id image_id,
                                               double* x, double* y,
                                               double* radius_x, double* radius_y)
{
  const std::shared_ptr<RegionGeometry_Ellipse> ellipse = std::dynamic_pointer_cast<RegionGeometry_Ellipse>(region->region);
  if (ellipse) {
    auto t = RegionCoordinateTransform::create(region->context->get_heif_file(), image_id,
                                               region->region_item->reference_width,
                                               region->region_item->reference_height);

    RegionCoordinateTransform::Point p = t.transform_point({(double) ellipse->x, (double) ellipse->y});
    RegionCoordinateTransform::Extent e = t.transform_extent({(double) ellipse->radius_x, (double) ellipse->radius_y});

    *x = p.x;
    *y = p.y;
    *radius_x = e.x;
    *radius_y = e.y;
    return heif_error_success;
  }

  return heif_error_invalid_parameter_value;
}


static int heif_region_get_poly_num_points(const heif_region* region)
{
  const std::shared_ptr<RegionGeometry_Polygon> polygon = std::dynamic_pointer_cast<RegionGeometry_Polygon>(region->region);
  if (polygon) {
    return (int) polygon->points.size();
  }
  return 0;
}


int heif_region_get_polygon_num_points(const heif_region* region)
{
  return heif_region_get_poly_num_points(region);
}


static heif_error heif_region_get_poly_points(const heif_region* region, int32_t* pts)
{
  if (pts == nullptr) {
    return heif_error_invalid_parameter_value;
  }

  const std::shared_ptr<RegionGeometry_Polygon> poly = std::dynamic_pointer_cast<RegionGeometry_Polygon>(region->region);
  if (poly) {
    for (int i = 0; i < (int) poly->points.size(); i++) {
      pts[2 * i + 0] = poly->points[i].x;
      pts[2 * i + 1] = poly->points[i].y;
    }
    return heif_error_success;
  }
  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_polygon_points(const heif_region* region, int32_t* pts)
{
  return heif_region_get_poly_points(region, pts);
}


static heif_error heif_region_get_poly_points_scaled(const heif_region* region, double* pts, heif_item_id image_id)
{
  if (pts == nullptr) {
    return heif_error_invalid_parameter_value;
  }

  const std::shared_ptr<RegionGeometry_Polygon> poly = std::dynamic_pointer_cast<RegionGeometry_Polygon>(region->region);
  if (poly) {
    auto t = RegionCoordinateTransform::create(region->context->get_heif_file(), image_id,
                                               region->region_item->reference_width,
                                               region->region_item->reference_height);

    for (int i = 0; i < (int) poly->points.size(); i++) {
      RegionCoordinateTransform::Point p = t.transform_point({
                                                               (double) poly->points[i].x,
                                                               (double) poly->points[i].y
                                                             });

      pts[2 * i + 0] = p.x;
      pts[2 * i + 1] = p.y;
    }
    return heif_error_success;
  }
  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_polygon_points_transformed(const heif_region* region, heif_item_id image_id, double* pts)
{
  return heif_region_get_poly_points_scaled(region, pts, image_id);
}


int heif_region_get_polyline_num_points(const heif_region* region)
{
  return heif_region_get_poly_num_points(region);
}


heif_error heif_region_get_polyline_points(const heif_region* region, int32_t* pts)
{
  return heif_region_get_poly_points(region, pts);
}


heif_error heif_region_get_polyline_points_transformed(const heif_region* region, heif_item_id image_id, double* pts)
{
  return heif_region_get_poly_points_scaled(region, pts, image_id);
}


heif_error heif_region_get_referenced_mask_ID(const heif_region* region,
                                              int32_t* x, int32_t* y,
                                              uint32_t* width, uint32_t* height,
                                              heif_item_id* mask_item_id)
{
  if ((x == nullptr) || (y == nullptr) || (width == nullptr) || (height == nullptr) || (mask_item_id == nullptr)) {
    return heif_error_null_pointer_argument;
  }

  const std::shared_ptr<RegionGeometry_ReferencedMask> mask = std::dynamic_pointer_cast<RegionGeometry_ReferencedMask>(region->region);
  if (mask) {
    *x = mask->x;
    *y = mask->y;
    *width = mask->width;
    *height = mask->height;
    *mask_item_id = mask->referenced_item;
    return heif_error_success;
  }
  return heif_error_invalid_parameter_value;
}


size_t heif_region_get_inline_mask_data_len(const heif_region* region)
{
  const std::shared_ptr<RegionGeometry_InlineMask> mask = std::dynamic_pointer_cast<RegionGeometry_InlineMask>(region->region);
  if (mask) {
    return mask->mask_data.size();
  }
  return 0;
}


static heif_region* create_region(const std::shared_ptr<RegionGeometry>& r,
                                  heif_region_item* item)
{
  auto region = new heif_region();
  region->region = r;
  region->region_item = item->region_item;
  region->context = item->context;
  return region;
}


heif_error heif_region_item_add_region_inline_mask_data(heif_region_item* item,
                                                        int32_t x, int32_t y,
                                                        uint32_t width, uint32_t height,
                                                        const uint8_t* mask_data,
                                                        size_t mask_data_len,
                                                        heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_InlineMask>();
  region->x = x;
  region->y = y;
  region->width = width;
  region->height = height;
  region->mask_data.resize(mask_data_len);
  std::memcpy(region->mask_data.data(), mask_data, region->mask_data.size());

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}


static heif_error heif_region_get_inline_mask_image(const heif_region* region,
                                                    int32_t* out_x0, int32_t* out_y0,
                                                    uint32_t* out_width, uint32_t* out_height,
                                                    heif_image** out_mask_image)
{
  if ((out_x0 == nullptr) || (out_y0 == nullptr) || (out_width == nullptr) || (out_height == nullptr)) {
    return heif_error_null_pointer_argument;
  }

  const std::shared_ptr<RegionGeometry_InlineMask> mask = std::dynamic_pointer_cast<RegionGeometry_InlineMask>(region->region);
  if (mask) {
    *out_x0 = mask->x;
    *out_y0 = mask->y;
    uint32_t width = *out_width = mask->width;
    uint32_t height = *out_height = mask->height;
    uint8_t* mask_data = mask->mask_data.data();

    heif_error err = heif_image_create(width, height, heif_colorspace_monochrome, heif_chroma_monochrome, out_mask_image);
    if (err.code) {
      return err;
    }
    err = heif_image_add_plane(*out_mask_image, heif_channel_Y, width, height, 8);
    if (err.code) {
      heif_image_release(*out_mask_image);
      return err;
    }
    size_t stride;
    uint8_t* p = heif_image_get_plane2(*out_mask_image, heif_channel_Y, &stride);
    uint64_t pixel_index = 0;

    for (uint32_t y = 0; y < height; y++) {
      for (uint32_t x = 0; x < width; x++) {
        uint64_t mask_byte = pixel_index / 8;
        uint8_t pixel_bit = uint8_t(0x80U >> (pixel_index % 8));

        p[y * stride + x] = (mask_data[mask_byte] & pixel_bit) ? 255 : 0;

        pixel_index++;
      }
    }
    return heif_error_success;
  }
  return heif_error_invalid_parameter_value;
}


heif_error heif_region_get_mask_image(const heif_region* region,
                                      int32_t* x, int32_t* y,
                                      uint32_t* width, uint32_t* height,
                                      heif_image** mask_image)
{
  if (region->region->getRegionType() == heif_region_type_inline_mask) {
    return heif_region_get_inline_mask_image(region, x, y, width, height, mask_image);
  }
  else if (region->region->getRegionType() == heif_region_type_referenced_mask) {
    heif_item_id referenced_item_id = 0;
    heif_error err = heif_region_get_referenced_mask_ID(region, x, y, width, height, &referenced_item_id);
    if (err.code != heif_error_Ok) {
      return err;
    }

    heif_context ctx;
    ctx.context = region->context;

    heif_image_handle* mski_handle_in;
    err = heif_context_get_image_handle(&ctx, referenced_item_id, &mski_handle_in);
    if (err.code != heif_error_Ok) {
      assert(mski_handle_in == nullptr);
      return err;
    }

    err = heif_decode_image(mski_handle_in, mask_image, heif_colorspace_monochrome, heif_chroma_monochrome, NULL);

    heif_image_handle_release(mski_handle_in);

    return err;
  }

  return heif_error_invalid_parameter_value;
}


heif_error heif_image_handle_add_region_item(heif_image_handle* image_handle,
                                             uint32_t reference_width, uint32_t reference_height,
                                             heif_region_item** out_region_item)
{
  std::shared_ptr<RegionItem> regionItem = image_handle->context->add_region_item(reference_width, reference_height);
  image_handle->image->add_region_item_id(regionItem->item_id);

  if (out_region_item) {
    heif_region_item* item = new heif_region_item();
    item->context = image_handle->context;
    item->region_item = std::move(regionItem);

    *out_region_item = item;
  }

  return heif_error_success;
}


heif_error heif_region_item_add_region_point(heif_region_item* item,
                                             int32_t x, int32_t y,
                                             heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_Point>();
  region->x = x;
  region->y = y;

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}


heif_error heif_region_item_add_region_rectangle(heif_region_item* item,
                                                 int32_t x, int32_t y,
                                                 uint32_t width, uint32_t height,
                                                 heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_Rectangle>();
  region->x = x;
  region->y = y;
  region->width = width;
  region->height = height;

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}


heif_error heif_region_item_add_region_ellipse(heif_region_item* item,
                                               int32_t x, int32_t y,
                                               uint32_t radius_x, uint32_t radius_y,
                                               heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_Ellipse>();
  region->x = x;
  region->y = y;
  region->radius_x = radius_x;
  region->radius_y = radius_y;

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}


heif_error heif_region_item_add_region_polygon(heif_region_item* item,
                                               const int32_t* pts, int nPoints,
                                               heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_Polygon>();
  region->points.resize(nPoints);

  for (int i = 0; i < nPoints; i++) {
    region->points[i].x = pts[2 * i + 0];
    region->points[i].y = pts[2 * i + 1];
  }

  region->closed = true;

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}


heif_error heif_region_item_add_region_polyline(heif_region_item* item,
                                                const int32_t* pts, int nPoints,
                                                heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_Polygon>();
  region->points.resize(nPoints);

  for (int i = 0; i < nPoints; i++) {
    region->points[i].x = pts[2 * i + 0];
    region->points[i].y = pts[2 * i + 1];
  }

  region->closed = false;

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}


heif_error heif_region_item_add_region_referenced_mask(heif_region_item* item,
                                                       int32_t x, int32_t y,
                                                       uint32_t width, uint32_t height,
                                                       heif_item_id mask_item_id,
                                                       heif_region** out_region)
{
  auto region = std::make_shared<RegionGeometry_ReferencedMask>();
  region->x = x;
  region->y = y;
  region->width = width;
  region->height = height;
  region->referenced_item = mask_item_id;

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  /* When the geometry 'mask' of a region is represented by a mask stored in
   * another image item the image item containing the mask shall be identified
   * by an item reference of type 'mask' from the region item to the image item
   * containing the mask. */
  std::shared_ptr<HeifContext> ctx = item->context;
  ctx->add_region_referenced_mask_ref(item->region_item->item_id, mask_item_id);

  return heif_error_success;
}


heif_error heif_region_get_inline_mask_data(const heif_region* region,
                                            int32_t* x, int32_t* y,
                                            uint32_t* width, uint32_t* height,
                                            uint8_t* data)
{
  if ((x == nullptr) || (y == nullptr) || (width == nullptr) || (height == nullptr)) {
    return heif_error_null_pointer_argument;
  }

  const std::shared_ptr<RegionGeometry_InlineMask> mask = std::dynamic_pointer_cast<RegionGeometry_InlineMask>(region->region);
  if (mask) {
    *x = mask->x;
    *y = mask->y;
    *width = mask->width;
    *height = mask->height;
    memcpy(data, mask->mask_data.data(), mask->mask_data.size());
    return heif_error_success;
  }
  return heif_error_invalid_parameter_value;
}


heif_error heif_region_item_add_region_inline_mask(heif_region_item* item,
                                                   int32_t x0, int32_t y0,
                                                   uint32_t width, uint32_t height,
                                                   heif_image* mask_image,
                                                   heif_region** out_region)
{
  if (!heif_image_has_channel(mask_image, heif_channel_Y)) {
    return {heif_error_Usage_error, heif_suberror_Nonexisting_image_channel_referenced, "Inline mask image must have a Y channel"};
  }
  auto region = std::make_shared<RegionGeometry_InlineMask>();
  region->x = x0;
  region->y = y0;
  region->width = width;
  region->height = height;
  region->mask_data.resize((width * height + 7) / 8);
  memset(region->mask_data.data(), 0, region->mask_data.size());

  uint32_t mask_height = mask_image->image->get_height();
  uint32_t mask_width = mask_image->image->get_width();
  size_t stride;
  uint8_t* p = heif_image_get_plane2(mask_image, heif_channel_Y, &stride);
  uint64_t pixel_index = 0;

  for (uint32_t y = 0; y < mask_height; y++) {
    for (uint32_t x = 0; x < mask_width; x++) {
      uint8_t mask_bit = p[y * stride + x] & 0x80; // use high-order bit of the 8-bit mask value as binary mask value
      region->mask_data.data()[pixel_index / 8] |= uint8_t(mask_bit >> (pixel_index % 8));

      pixel_index++;
    }
  }

  item->region_item->add_region(region);

  if (out_region) {
    *out_region = create_region(region, item);
  }

  return heif_error_success;
}
