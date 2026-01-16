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

#ifndef LIBHEIF_HEIF_REGIONS_H
#define LIBHEIF_HEIF_REGIONS_H

#include "heif_image_handle.h"
#include "heif_library.h"
#include "heif_error.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- region items and annotations

// See ISO/IEC 23008-12:2022 Section 6.10 "Region items and region annotations"

typedef struct heif_region_item heif_region_item;

/**
 * Region type.
 *
 * Each region item will contain zero or more regions, which may have different geometry or
 * mask representations.
*/
enum heif_region_type
{
  /**
   * Point geometry.
   *
   * The region is represented by a single point.
   */
  heif_region_type_point = 0,

  /**
   * Rectangle geometry.
   *
   * The region is represented by a top left position, and a size defined
   * by a width and height. All of the interior points and the edge are
   * part of the region.
   */
  heif_region_type_rectangle = 1,

  /**
   * Ellipse geometry.
   *
   * The region is represented by a centre point, and radii in the X and
   * Y directions. All of the interior points and the edge are part of the
   * region.
   */
  heif_region_type_ellipse = 2,

  /**
   * Polygon geometry.
   *
   * The region is represented by a sequence of points, which is considered
   * implicitly closed. All of the interior points and the edge are part
   * of the region.
   */
  heif_region_type_polygon = 3,

  /**
   * Reference mask.
   *
   * The region geometry is described by the pixels in another image item,
   * which has a item reference of type `mask` from the region item to the
   * image item containing the mask.
   *
   * The image item containing the mask is one of:
   *
   * - a mask item (see ISO/IEC 23008-12:2022 Section 6.10.2), or a derived
   * image from a mask item
   *
   * - an image item in monochrome format (4:0:0 chroma)
   *
   * - an image item in colour format with luma and chroma planes (e.g. 4:2:0)
   *
   * If the pixel value is equal to the minimum sample value (e.g. 0 for unsigned
   * integer), the pixel is not part of the region. If the pixel value is equal
   * to the maximum sample value (e.g. 255 for 8 bit unsigned integer), the pixel
   * is part of the region. If the pixel value is between the minimum sample value
   * and maximum sample value, the pixel value represents an (application defined)
   * probability that the pixel is part of the region, where higher pixel values
   * correspond to higher probability values.
  */
  heif_region_type_referenced_mask = 4,

  /**
   * Inline mask.
   *
   * The region geometry is described by a sequence of bits stored in inline
   * in the region, one bit per pixel. If the bit value is `1`, the pixel is
   * part of the region. If the bit value is `0`, the pixel is not part of the
   * region.
   */
  heif_region_type_inline_mask = 5,

  /**
   * Polyline geometry.
   *
   * The region is represented by a sequence of points, which are not
   * considered to form a closed surface. Only the edge is part of the region.
  */
  heif_region_type_polyline = 6
};

typedef struct heif_region heif_region;

/**
 * Get the number of region items that are attached to an image.
 *
 * @param image_handle the image handle for the image to query.
 * @return the number of region items, which can be zero.
 */
LIBHEIF_API
int heif_image_handle_get_number_of_region_items(const heif_image_handle* image_handle);

/**
 * Get the region item identifiers for the region items attached to an image.
 *
 * Possible usage (in C++):
 * @code
 *  int numRegionItems = heif_image_handle_get_number_of_region_items(handle);
 *  if (numRegionItems > 0) {
 *      std::vector<heif_item_id> region_item_ids(numRegionItems);
 *      heif_image_handle_get_list_of_region_item_ids(handle, region_item_ids.data(), numRegionItems);
 *      // use region item ids
 *  }
 * @endcode
 *
 * @param image_handle the image handle for the parent image to query
 * @param region_item_ids_array array to put the item identifiers into
 * @param max_count the maximum number of region identifiers
 * @return the number of region item identifiers that were returned.
 */
LIBHEIF_API
int heif_image_handle_get_list_of_region_item_ids(const heif_image_handle* image_handle,
                                                  heif_item_id* region_item_ids_array,
                                                  int max_count);

/**
 * Get the region item.
 *
 * Caller is responsible for release of the output heif_region_item with heif_region_item_release().
 *
 * @param context the context to get the region item from, usually from a file operation
 * @param region_item_id the identifier for the region item
 * @param out pointer to pointer to the resulting region item
 * @return heif_error_ok on success, or an error value indicating the problem
 */
LIBHEIF_API
heif_error heif_context_get_region_item(const heif_context* context,
                                        heif_item_id region_item_id,
                                        heif_region_item** out);

/**
 * Get the item identifier for a region item.
 *
 * @param region_item the region item to query
 * @return the region item identifier (or 0 if the region_item is null)
 */
LIBHEIF_API
heif_item_id heif_region_item_get_id(heif_region_item* region_item);

/**
 * Release a region item.
 *
 * This should be called on items from heif_context_get_region_item().
 *
 * @param region_item the item to release.
 */
LIBHEIF_API
void heif_region_item_release(heif_region_item* region_item);

/**
 * Get the reference size for a region item.
 *
 * The reference size specifies the coordinate space used for the region items.
 * When the reference size does not match the image size, the regions need to be
 * scaled to correspond.
 *
 * @param out_width the return value for the reference width (before any transformation)
 * @param out_height the return value for the reference height (before any transformation)
 */
LIBHEIF_API
void heif_region_item_get_reference_size(heif_region_item*, uint32_t* out_width, uint32_t* out_height);

/**
 * Get the number of regions within a region item.
 *
 * @param region_item the region item to query.
 * @return the number of regions
*/
LIBHEIF_API
int heif_region_item_get_number_of_regions(const heif_region_item* region_item);

/**
 * Get the regions that are part of a region item.
 *
 * Caller is responsible for releasing the returned `heif_region` objects, using heif_region_release()
 * on each region, or heif_region_release_many() on the returned array.
 *
 * Possible usage (in C++):
 * @code
 *  int num_regions = heif_image_handle_get_number_of_regions(region_item);
 *  if (num_regions > 0) {
 *      std::vector<heif_region*> regions(num_regions);
 *      int n = heif_region_item_get_list_of_regions(region_item, regions.data(), (int)regions.size());
 *      // use regions
 *      heif_region_release_many(regions.data(), n);
 *  }
 * @endcode
 *
 * @param region_item the region_item to query
 * @param out_regions_array array to put the region pointers into
 * @param max_count the maximum number of regions, which needs to correspond to the size of the out_regions_array
 * @return the number of regions that were returned.
 */
LIBHEIF_API
int heif_region_item_get_list_of_regions(const heif_region_item* region_item,
                                         heif_region** out_regions_array,
                                         int max_count);

/**
 * Release a region.
 *
 * This should be called on regions from heif_region_item_get_list_of_regions().
 *
 * @param region the region to release.
 *
 * \sa heif_region_release_many() to release the whole list
 */
LIBHEIF_API
void heif_region_release(const heif_region* region);

/**
 * Release a list of regions.
 *
 * This should be called on the list of regions from heif_region_item_get_list_of_regions().
 *
 * @param regions_array the regions to release.
 * @param num_items the number of items in the array
 *
 * \sa heif_region_release() to release a single region
 */
LIBHEIF_API
void heif_region_release_many(const heif_region* const* regions_array, int num_items);

/**
 * Get the region type for a specified region. 
 *
 * @param region the region to query
 * @return the corresponding region type as an enumeration value
 */
LIBHEIF_API
enum heif_region_type heif_region_get_type(const heif_region* region);

// When querying the region geometry, there is a version without and a version with "_transformed" suffix.
// The version without returns the coordinates in the reference coordinate space.
// The version with "_transformed" suffix give the coordinates in pixels after all transformative properties have been applied.

/**
 * Get the values for a point region.
 *
 * This returns the coordinates in the reference coordinate space (from the parent region item).
 *
 * @param region the region to query, which must be of type #heif_region_type_point.
 * @param out_x the X coordinate, where 0 is the left-most column.
 * @param out_y the Y coordinate, where 0 is the top-most row.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_point_transformed() for a version in pixels after all transformative properties have been applied.
 */
LIBHEIF_API
heif_error heif_region_get_point(const heif_region* region, int32_t* out_x, int32_t* out_y);

/**
 * Get the transformed values for a point region.
 *
 * This returns the coordinates in pixels after all transformative properties have been applied.
 *
 * @param region the region to query, which must be of type #heif_region_type_point.
 * @param image_id the identifier for the image to transform / scale the region to
 * @param out_x the X coordinate, where 0 is the left-most column.
 * @param out_y the Y coordinate, where 0 is the top-most row.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_point() for a version that returns the values in the reference coordinate space.
 */
LIBHEIF_API
heif_error heif_region_get_point_transformed(const heif_region* region, heif_item_id image_id, double* out_x, double* out_y);

/**
 * Get the values for a rectangle region.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 * The rectangle is represented by a top left corner position, and a size defined
 * by a width and height. All of the interior points and the edge are
 * part of the region.
 *
 * @param region the region to query, which must be of type #heif_region_type_rectangle.
 * @param out_x the X coordinate for the top left corner, where 0 is the left-most column.
 * @param out_y the Y coordinate for the top left corner, where 0 is the top-most row.
 * @param out_width the width of the rectangle
 * @param out_height the height of the rectangle
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_rectangle_transformed() for a version in pixels after all transformative properties have been applied.
 */
LIBHEIF_API
heif_error heif_region_get_rectangle(const heif_region* region,
                                     int32_t* out_x, int32_t* out_y,
                                     uint32_t* out_width, uint32_t* out_height);

/**
 * Get the transformed values for a rectangle region.
 *
 * This returns the coordinates in pixels after all transformative properties have been applied.
 * The rectangle is represented by a top left corner position, and a size defined
 * by a width and height. All of the interior points and the edge are
 * part of the region.
 *
 * @param region the region to query, which must be of type #heif_region_type_rectangle.
 * @param image_id the identifier for the image to transform / scale the region to
 * @param out_x the X coordinate for the top left corner, where 0 is the left-most column.
 * @param out_y the Y coordinate for the top left corner, where 0 is the top-most row.
 * @param out_width the width of the rectangle
 * @param out_height the height of the rectangle
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_rectangle() for a version that returns the values in the reference coordinate space.
 */
LIBHEIF_API
heif_error heif_region_get_rectangle_transformed(const heif_region* region,
                                                 heif_item_id image_id,
                                                 double* out_x, double* out_y,
                                                 double* out_width, double* out_height);

/**
 * Get the values for an ellipse region.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 * The ellipse is represented by a centre position, and a size defined
 * by radii in the X and Y directions. All of the interior points and the edge are
 * part of the region.
 *
 * @param region the region to query, which must be of type #heif_region_type_ellipse.
 * @param out_x the X coordinate for the centre point, where 0 is the left-most column.
 * @param out_y the Y coordinate for the centre point, where 0 is the top-most row.
 * @param out_radius_x the radius value in the X direction.
 * @param out_radius_y the radius value in the Y direction
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_ellipse_transformed() for a version in pixels after all transformative properties have been applied.
 */
LIBHEIF_API
heif_error heif_region_get_ellipse(const heif_region* region,
                                   int32_t* out_x, int32_t* out_y,
                                   uint32_t* out_radius_x, uint32_t* out_radius_y);


/**
 * Get the transformed values for an ellipse region.
 *
 * This returns the coordinates in pixels after all transformative properties have been applied.
 * The ellipse is represented by a centre position, and a size defined
 * by radii in the X and Y directions. All of the interior points and the edge are
 * part of the region.
 *
 * @param region the region to query, which must be of type #heif_region_type_ellipse.
 * @param image_id the identifier for the image to transform / scale the region to
 * @param out_x the X coordinate for the centre point, where 0 is the left-most column.
 * @param out_y the Y coordinate for the centre point, where 0 is the top-most row.
 * @param out_radius_x the radius value in the X direction.
 * @param out_radius_y the radius value in the Y direction
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_ellipse() for a version that returns the values in the reference coordinate space.
 */
LIBHEIF_API
heif_error heif_region_get_ellipse_transformed(const heif_region* region,
                                               heif_item_id image_id,
                                               double* out_x, double* out_y,
                                               double* out_radius_x, double* out_radius_y);

/**
 * Get the number of points in a polygon.
 *
 * @param region the region to query, which must be of type #heif_region_type_polygon
 * @return the number of points, or -1 on error.
 */
LIBHEIF_API
int heif_region_get_polygon_num_points(const heif_region* region);

/**
 * Get the points in a polygon region.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 *
 * A polygon is a sequence of points that form a closed shape. The first point does
 * not need to be repeated as the last point. All of the interior points and the edge are
 * part of the region.
 * The points are returned as pairs of X,Y coordinates, in the order X<sub>1</sub>,
 * Y<sub>1</sub>, X<sub>2</sub>, Y<sub>2</sub>, ..., X<sub>n</sub>, Y<sub>n</sub>.
 *
 * @param region the region to query, which must be of type #heif_region_type_polygon
 * @param out_pts_array the array to return the points in, which must have twice as many entries as there are points
 * in the polygon.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_polygon_points_transformed() for a version in pixels after all transformative properties have been applied.
 */
LIBHEIF_API
heif_error heif_region_get_polygon_points(const heif_region* region,
                                          int32_t* out_pts_array);

/**
 * Get the transformed points in a polygon region.
 *
 * This returns the coordinates in pixels after all transformative properties have been applied.
 *
 * A polygon is a sequence of points that form a closed shape. The first point does
 * not need to be repeated as the last point. All of the interior points and the edge are
 * part of the region.
 * The points are returned as pairs of X,Y coordinates, in the order X<sub>1</sub>,
 * Y<sub>1</sub>, X<sub>2</sub>, Y<sub>2</sub>, ..., X<sub>n</sub>, Y<sub>n</sub>.
 *
 * @param region the region to query, which must be of type #heif_region_type_polygon
 * @param image_id the identifier for the image to transform / scale the region to
 * @param out_pts_array the array to return the points in, which must have twice as many entries as there are points
 * in the polygon.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_polygon_points() for a version that returns the values in the reference coordinate space.
 */
LIBHEIF_API
heif_error heif_region_get_polygon_points_transformed(const heif_region* region,
                                                      heif_item_id image_id,
                                                      double* out_pts_array);
/**
 * Get the number of points in a polyline.
 *
 * @param region the region to query, which must be of type #heif_region_type_polyline
 * @return the number of points, or -1 on error.
 */
LIBHEIF_API
int heif_region_get_polyline_num_points(const heif_region* region);

/**
 * Get the points in a polyline region.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 *
 * A polyline is a sequence of points that does not form a closed shape. Even if the
 * polyline is closed, the only points that are part of the region are those that 
 * intersect (even minimally) a one-pixel line drawn along the polyline.
 * The points are provided as pairs of X,Y coordinates, in the order X<sub>1</sub>,
 * Y<sub>1</sub>, X<sub>2</sub>, Y<sub>2</sub>, ..., X<sub>n</sub>, Y<sub>n</sub>.
 *
 * Possible usage (in C++):
 * @code
 * int num_polyline_points = heif_region_get_polyline_num_points(region);
 * if (num_polyline_points > 0) {
 *     std::vector<int32_t> polyline(num_polyline_points * 2);
 *     heif_region_get_polyline_points(region, polyline.data());
 *     // do something with points ...
 * }
 * @endcode
 *
 * @param region the region to query, which must be of type #heif_region_type_polyline
 * @param out_pts_array the array to return the points in, which must have twice as many entries as there are points
 * in the polyline.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_polyline_points_transformed() for a version in pixels after all transformative properties have been applied.
 */
LIBHEIF_API
heif_error heif_region_get_polyline_points(const heif_region* region,
                                           int32_t* out_pts_array);

/**
 * Get the transformed points in a polyline region.
 *
 * This returns the coordinates in pixels after all transformative properties have been applied.
 *
 * A polyline is a sequence of points that does not form a closed shape. Even if the
 * polyline is closed, the only points that are part of the region are those that 
 * intersect (even minimally) a one-pixel line drawn along the polyline.
 * The points are provided as pairs of X,Y coordinates, in the order X<sub>1</sub>,
 * Y<sub>1</sub>, X<sub>2</sub>, Y<sub>2</sub>, ..., X<sub>n</sub>, Y<sub>n</sub>.
 *
 * @param region the region to query, which must be of type #heif_region_type_polyline
 * @param image_id the identifier for the image to transform / scale the region to
 * @param out_pts_array the array to return the points in, which must have twice as many entries as there are points
 * in the polyline.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \sa heif_region_get_polyline_points() for a version that returns the values in the reference coordinate space.
 */
LIBHEIF_API
heif_error heif_region_get_polyline_points_transformed(const heif_region* region,
                                                       heif_item_id image_id,
                                                       double* out_pts_array);

/**
 * Get a referenced item mask region.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 * The mask location is represented by a top left corner position, and a size defined
 * by a width and height. The value of each sample in that mask identifies whether the
 * corresponding pixel is part of the region.
 *
 * The mask is provided as an image in another item. The image item containing the mask
 * is one of:
 *
 * - a mask item (see ISO/IEC 23008-12:2022 Section 6.10.2), or a derived
 * image from a mask item
 *
 * - an image item in monochrome format (4:0:0 chroma)
 *
 * - an image item in colour format with luma and chroma planes (e.g. 4:2:0)
 *
 * If the pixel value is equal to the minimum sample value (e.g. 0 for unsigned
 * integer), the pixel is not part of the region. If the pixel value is equal
 * to the maximum sample value (e.g. 255 for 8 bit unsigned integer), the pixel
 * is part of the region. If the pixel value is between the minimum sample value
 * and maximum sample value, the pixel value represents an (application defined)
 * probability that the pixel is part of the region, where higher pixel values
 * correspond to higher probability values.
 *
 * @param region the region to query, which must be of type #heif_region_type_referenced_mask.
 * @param out_x the X coordinate for the top left corner, where 0 is the left-most column.
 * @param out_y the Y coordinate for the top left corner, where 0 is the top-most row.
 * @param out_width the width of the mask region
 * @param out_height the height of the mask region
 * @param out_mask_item_id the item identifier for the image that provides the mask.
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 */
LIBHEIF_API
heif_error heif_region_get_referenced_mask_ID(const heif_region* region,
                                              int32_t* out_x, int32_t* out_y,
                                              uint32_t* out_width, uint32_t* out_height,
                                              heif_item_id* out_mask_item_id);

/**
 * Get the length of the data in an inline mask region.
 *
 * @param region the region to query, which must be of type #heif_region_type_inline_mask.
 * @return the number of bytes in the mask data, or 0 on error.
 */
LIBHEIF_API
size_t heif_region_get_inline_mask_data_len(const heif_region* region);


/**
 * Get data for an inline mask region.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 * The mask location is represented by a top left corner position, and a size defined
 * by a width and height.
 *
 * The mask is held as inline data on the region, one bit per pixel, most significant
 * bit first pixel, no padding. If the bit value is `1`, the corresponding pixel is
 * part of the region. If the bit value is `0`, the corresponding pixel is not part of the
 * region.
 *
 * Possible usage (in C++):
 * @code
 * long unsigned int data_len = heif_region_get_inline_mask_data_len(region);
 * int32_t x, y;
 * uint32_t width, height;
 * std::vector<uint8_t> mask_data(data_len);
 * err = heif_region_get_inline_mask(region, &x, &y, &width, &height, mask_data.data());
 * @endcode
 *
 * @param region the region to query, which must be of type #heif_region_type_inline_mask.
 * @param out_x the X coordinate for the top left corner, where 0 is the left-most column.
 * @param out_y the Y coordinate for the top left corner, where 0 is the top-most row.
 * @param out_width the width of the mask region
 * @param out_height the height of the mask region
 * @param out_mask_data the location to return the mask data
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 */
LIBHEIF_API
heif_error heif_region_get_inline_mask_data(const heif_region* region,
                                            int32_t* out_x, int32_t* out_y,
                                            uint32_t* out_width, uint32_t* out_height,
                                            uint8_t* out_mask_data);

/**
 * Get a mask region image.
 *
 * This returns the values in the reference coordinate space (from the parent region item).
 * The mask location is represented by a top left corner position, and a size defined
 * by a width and height.
 *
 * This function works when the passed region is either a heif_region_type_referenced_mask or
 * a heif_region_type_inline_mask.
 * The returned image is a monochrome image where each pixel represents the (scaled) probability
 * of the pixel being part of the mask.
 *
 * If the region type is an inline mask, which always holds a binary mask, this function
 * converts the binary inline mask to an 8-bit monochrome image with the values '0' and '255'.
 * The pixel value is set to `255` where the pixel is part of the region, and `0` where the
 * pixel is not part of the region.
 *
 * @param region the region to query, which must be of type #heif_region_type_inline_mask.
 * @param out_x the X coordinate for the top left corner, where 0 is the left-most column.
 * @param out_y the Y coordinate for the top left corner, where 0 is the top-most row.
 * @param out_width the width of the mask region
 * @param out_height the height of the mask region
 * @param out_mask_image the returned mask image
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 *
 * \note the caller is responsible for releasing the mask image
 */
LIBHEIF_API
heif_error heif_region_get_mask_image(const heif_region* region,
                                      int32_t* out_x, int32_t* out_y,
                                      uint32_t* out_width, uint32_t* out_height,
                                      heif_image** out_mask_image);

// --- adding region items

/**
 * Add a region item to an image.
 *
 * The region item is a collection of regions (point, polyline, polygon, rectangle, ellipse or mask)
 * along with a reference size (width and height) that forms the coordinate basis for the regions.
 *
 * The concept is to add the region item, then add one or more regions to the region item.
 *
 * @param image_handle the image to attach the region item to.
 * @param reference_width the width of the reference size.
 * @param reference_height the height of the reference size.
 * @param out_region_item the resulting region item
 * @return heif_error_ok on success, or an error indicating the problem on failure
*/
LIBHEIF_API
heif_error heif_image_handle_add_region_item(heif_image_handle* image_handle,
                                             uint32_t reference_width, uint32_t reference_height,
                                             heif_region_item** out_region_item);

/**
 * Add a point region to the region item.
 *
 * @param region_item the region item that holds this point region
 * @param x the x value for the point location
 * @param y the y value for the point location
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error indicating the problem on failure
 *
 * @note The `out_region` parameter is optional, and can be set to `NULL` if not needed.
 */
LIBHEIF_API
heif_error heif_region_item_add_region_point(heif_region_item* region_item,
                                             int32_t x, int32_t y,
                                             heif_region** out_region);

/**
 * Add a rectangle region to the region item.
 *
 * @param region_item the region item that holds this rectangle region
 * @param x the x value for the top-left corner of this rectangle region
 * @param y the y value for the top-left corner of this rectangle region
 * @param width the width of this rectangle region
 * @param height the height of this rectangle region
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error indicating the problem on failure
 *
 * @note The `out_region` parameter is optional, and can be set to `NULL` if not needed.
 */
LIBHEIF_API
heif_error heif_region_item_add_region_rectangle(heif_region_item* region_item,
                                                 int32_t x, int32_t y,
                                                 uint32_t width, uint32_t height,
                                                 heif_region** out_region);

/**
 * Add a ellipse region to the region item.
 *
 * @param region_item the region item that holds this ellipse region
 * @param x the x value for the centre of this ellipse region
 * @param y the y value for the centre of this ellipse region
 * @param radius_x the radius of the ellipse in the X (horizontal) direction
 * @param radius_y the radius of the ellipse in the Y (vertical) direction
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error indicating the problem on failure
 *
 * @note The `out_region` parameter is optional, and can be set to `NULL` if not needed.
 */
LIBHEIF_API
heif_error heif_region_item_add_region_ellipse(heif_region_item* region_item,
                                               int32_t x, int32_t y,
                                               uint32_t radius_x, uint32_t radius_y,
                                               heif_region** out_region);

/**
 * Add a polygon region to the region item.
 *
 * A polygon is a sequence of points that form a closed shape. The first point does
 * not need to be repeated as the last point.
 * The points are provided as pairs of X,Y coordinates, in the order X<sub>1</sub>,
 * Y<sub>1</sub>, X<sub>2</sub>, Y<sub>2</sub>, ..., X<sub>n</sub>, Y<sub>n</sub>.
 *
 * @param region_item the region item that holds this polygon region
 * @param pts_array the array of points in X,Y order (see above)
 * @param nPoints the number of points
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error indicating the problem on failure
 *
 * @note `nPoints` is the number of points, not the number of elements in the array
 * @note The `out_region` parameter is optional, and can be set to `NULL` if not needed.
 */
LIBHEIF_API
heif_error heif_region_item_add_region_polygon(heif_region_item* region_item,
                                               const int32_t* pts_array, int nPoints,
                                               heif_region** out_region);

/**
 * Add a polyline region to the region item.
 *
 * A polyline is a sequence of points that does not form a closed shape. Even if the
 * polyline is closed, the only points that are part of the region are those that
 * intersect (even minimally) a one-pixel line drawn along the polyline.
 * The points are provided as pairs of X,Y coordinates, in the order X<sub>1</sub>,
 * Y<sub>1</sub>, X<sub>2</sub>, Y<sub>2</sub>, ..., X<sub>n</sub>, Y<sub>n</sub>.
 *
 * @param region_item the region item that holds this polyline region
 * @param pts_array the array of points in X,Y order (see above)
 * @param nPoints the number of points
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error indicating the problem on failure
 *
 * @note `nPoints` is the number of points, not the number of elements in the array
 * @note The `out_region` parameter is optional, and can be set to `NULL` if not needed.
 */
LIBHEIF_API
heif_error heif_region_item_add_region_polyline(heif_region_item* region_item,
                                                const int32_t* pts_array, int nPoints,
                                                heif_region** out_region);


/**
 * Add a referenced mask region to the region item.
 *
 * The region geometry is described by the pixels in another image item,
 * which has a item reference of type `mask` from the region item to the
 * image item containing the mask.
 *
 * The image item containing the mask is one of:
 *
 * - a mask item (see ISO/IEC 23008-12:2022 Section 6.10.2), or a derived
 * image from a mask item
 *
 * - an image item in monochrome format (4:0:0 chroma)
 *
 * - an image item in colour format with luma and chroma planes (e.g. 4:2:0)
 *
 * If the pixel value is equal to the minimum sample value (e.g. 0 for unsigned
 * integer), the pixel is not part of the region. If the pixel value is equal
 * to the maximum sample value (e.g. 255 for 8 bit unsigned integer), the pixel
 * is part of the region. If the pixel value is between the minimum sample value
 * and maximum sample value, the pixel value represents an (application defined)
 * probability that the pixel is part of the region, where higher pixel values
 * correspond to higher probability values.
 *
 * @param region_item the region item that holds this mask region
 * @param x the x value for the top-left corner of this mask region
 * @param y the y value for the top-left corner of this mask region
 * @param width the width of this mask region
 * @param height the height of this mask region
 * @param mask_item_id the item identifier for the mask that is referenced
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error indicating the problem on failure
 *
 * @note The `out_region` parameter is optional, and can be set to `NULL` if not needed.
 */
LIBHEIF_API
heif_error heif_region_item_add_region_referenced_mask(heif_region_item* region_item,
                                                       int32_t x, int32_t y,
                                                       uint32_t width, uint32_t height,
                                                       heif_item_id mask_item_id,
                                                       heif_region** out_region);


/**
 * Add an inline mask region to the region item.
 *
 * The region geometry is described by a top left corner position, and a size defined
 * by a width and height.
 *
 * The mask is held as inline data on the region, one bit per pixel, most significant
 * bit first pixel, no padding. If the bit value is `1`, the corresponding pixel is
 * part of the region. If the bit value is `0`, the corresponding pixel is not part of the
 * region.
 *
 * @param region_item the region item that holds this mask region
 * @param x the x value for the top-left corner of this mask region
 * @param y the y value for the top-left corner of this mask region
 * @param width the width of this mask region
 * @param height the height of this mask region
 * @param mask_data the location to return the mask data
 * @param mask_data_len the length of the mask data, in bytes
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 */
LIBHEIF_API
heif_error heif_region_item_add_region_inline_mask_data(heif_region_item* region_item,
                                                        int32_t x, int32_t y,
                                                        uint32_t width, uint32_t height,
                                                        const uint8_t* mask_data,
                                                        size_t mask_data_len,
                                                        heif_region** out_region);

/**
 * Add an inline mask region image to the region item.
 *
 * The region geometry is described by a top left corner position, and a size defined
 * by a width and height.
 *
 * The mask data is held as inline data on the region, one bit per pixel. The provided
 * image is converted to inline data, where any pixel with a value >= 0x80 becomes part of the
 * mask region. If the image width is less that the specified width, it is expanded
 * to match the width of the region (zero fill on the right). If the image height is
 * less than the specified height, it is expanded to match the height of the region
 * (zero fill on the bottom). If the image width or height is greater than the
 * width or height (correspondingly) of the region, the image is cropped.
 *
 * @param region_item the region item that holds this mask region
 * @param x the x value for the top-left corner of this mask region
 * @param y the y value for the top-left corner of this mask region
 * @param width the width of this mask region
 * @param height the height of this mask region
 * @param image the image to convert to an inline mask
 * @param out_region pointer to pointer to the returned region (optional, see below)
 * @return heif_error_ok on success, or an error value indicating the problem on failure
 */
LIBHEIF_API
heif_error heif_region_item_add_region_inline_mask(heif_region_item* region_item,
                                                   int32_t x, int32_t y,
                                                   uint32_t width, uint32_t height,
                                                   heif_image* image,
                                                   heif_region** out_region);
#ifdef __cplusplus
}
#endif

#endif
