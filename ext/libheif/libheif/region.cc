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

#include "region.h"
#include "error.h"
#include "file.h"
#include "box.h"
#include "libheif/heif_regions.h"
#include <algorithm>
#include <utility>


Error RegionItem::parse(const std::vector<uint8_t>& data,
                        const heif_security_limits* limits)
{
  if (data.size() < 8) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Less than 8 bytes of data");
  }

  uint8_t version = data[0];
  (void) version; // version is unused

  uint8_t flags = data[1];
  int field_size = ((flags & 1) ? 32 : 16);

  unsigned int dataOffset;
  if (field_size == 32) {
    if (data.size() < 12) {
      return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                   "Region data incomplete");
    }
    reference_width = four_bytes_to_uint32(data[2], data[3], data[4], data[5]);
    reference_height = four_bytes_to_uint32(data[6], data[7], data[8], data[9]);
    dataOffset = 10;
  }
  else {
    reference_width = four_bytes_to_uint32(0, 0, data[2], data[3]);
    reference_height = four_bytes_to_uint32(0, 0, data[4], data[5]);
    dataOffset = 6;
  }

  uint8_t region_count = data[dataOffset];
  dataOffset += 1;
  for (int i = 0; i < region_count; i++) {
    if (data.size() <= dataOffset) {
      return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                   "Region data incomplete");
    }

    uint8_t geometry_type = data[dataOffset];
    dataOffset += 1;

    std::shared_ptr<RegionGeometry> region;

    if (geometry_type == heif_region_type_point) {
      region = std::make_shared<RegionGeometry_Point>();
    }
    else if (geometry_type == heif_region_type_rectangle) {
      region = std::make_shared<RegionGeometry_Rectangle>();
    }
    else if (geometry_type == heif_region_type_ellipse) {
      region = std::make_shared<RegionGeometry_Ellipse>();
    }
    else if (geometry_type == heif_region_type_polygon) {
      auto polygon = std::make_shared<RegionGeometry_Polygon>();
      polygon->closed = true;
      region = polygon;
    }
    else if (geometry_type == heif_region_type_referenced_mask) {
      region = std::make_shared<RegionGeometry_ReferencedMask>();
    }
    else if (geometry_type == heif_region_type_inline_mask) {
      region = std::make_shared<RegionGeometry_InlineMask>();
    }
    else if (geometry_type == heif_region_type_polyline) {
      auto polygon = std::make_shared<RegionGeometry_Polygon>();
      polygon->closed = false;
      region = polygon;
    }
    else {
      //     // TODO: this isn't going to work - we can only exit here.
      //   std::cout << "ignoring unsupported region geometry type: "
      //             << (int)geometry_type << std::endl;

      continue;
    }

    Error error = region->parse(data, field_size, &dataOffset, limits);
    if (error) {
      return error;
    }

    mRegions.push_back(region);
  }
  return Error::Ok;
}

Error RegionItem::encode(std::vector<uint8_t>& result) const
{
  StreamWriter writer;

  writer.write8(0);

  // --- compute required field size

  int field_size_bytes = 2;

  if (reference_width <= 0xFFFF &&
      reference_height <= 0xFFFF) {
    field_size_bytes = 4;
  }

  if (field_size_bytes != 4) {
    for (auto& region : mRegions) {
      if (region->encode_needs_32bit()) {
        field_size_bytes = 4;
        break;
      }
    }
  }

  // --- write flags

  uint8_t flags = 0;

  if (field_size_bytes == 4) {
    flags |= 1;
  }

  writer.write8(flags);

  // --- write reference size

  writer.write(field_size_bytes, reference_width);
  writer.write(field_size_bytes, reference_height);

  // --- write regions

  if (mRegions.size() >= 256) {
    return Error(heif_error_Encoding_error, heif_suberror_Too_many_regions);
  }

  writer.write8((uint8_t) mRegions.size());

  for (auto& region : mRegions) {
    region->encode(writer, field_size_bytes);
  }

  result = writer.get_data();

  return Error::Ok;
}


uint32_t RegionGeometry::parse_unsigned(const std::vector<uint8_t>& data,
                                        int field_size,
                                        unsigned int* dataOffset)
{
  uint32_t x;
  if (field_size == 32) {
    x = four_bytes_to_uint32(data[*dataOffset + 0],
                             data[*dataOffset + 1],
                             data[*dataOffset + 2],
                             data[*dataOffset + 3]);
    *dataOffset = *dataOffset + 4;
  }
  else {
    x = four_bytes_to_uint32(0, 0, data[*dataOffset], data[*dataOffset + 1]);
    *dataOffset = *dataOffset + 2;
  }
  return x;
}

int32_t RegionGeometry::parse_signed(const std::vector<uint8_t>& data,
                                     int field_size,
                                     unsigned int* dataOffset)
{
  if (field_size == 32) {
    return (int32_t)parse_unsigned(data, field_size, dataOffset);
  } else {
    return (int16_t)parse_unsigned(data, field_size, dataOffset);
  }
}

Error RegionGeometry_Point::parse(const std::vector<uint8_t>& data,
                                  int field_size,
                                  unsigned int* dataOffset,
                                  const heif_security_limits* limits)
{
  unsigned int bytesRequired = (field_size / 8) * 2;
  if (data.size() - *dataOffset < bytesRequired) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for point region");
  }
  x = parse_signed(data, field_size, dataOffset);
  y = parse_signed(data, field_size, dataOffset);

  return Error::Ok;
}


static bool exceeds_s16(int32_t v)
{
  return (v > 32767 || v < -32768);
}

static bool exceeds_u16(uint32_t v)
{
  return v > 0xFFFF;
}


bool RegionGeometry_Point::encode_needs_32bit() const
{
  return exceeds_s16(x) || exceeds_s16(y);
}


void RegionGeometry_Point::encode(StreamWriter& writer, int field_size_bytes) const
{
  writer.write8(heif_region_type_point);
  writer.write(field_size_bytes, x);
  writer.write(field_size_bytes, y);
}


Error RegionGeometry_Rectangle::parse(const std::vector<uint8_t>& data,
                                      int field_size,
                                      unsigned int* dataOffset,
                                      const heif_security_limits* limits)
{
  unsigned int bytesRequired = (field_size / 8) * 4;
  if (data.size() - *dataOffset < bytesRequired) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for rectangle region");
  }
  x = parse_signed(data, field_size, dataOffset);
  y = parse_signed(data, field_size, dataOffset);
  width = parse_unsigned(data, field_size, dataOffset);
  height = parse_unsigned(data, field_size, dataOffset);
  return Error::Ok;
}


bool RegionGeometry_Rectangle::encode_needs_32bit() const
{
  return exceeds_s16(x) || exceeds_s16(y) || exceeds_u16(width) || exceeds_u16(height);
}


void RegionGeometry_Rectangle::encode(StreamWriter& writer, int field_size_bytes) const
{
  writer.write8(heif_region_type_rectangle);
  writer.write(field_size_bytes, x);
  writer.write(field_size_bytes, y);
  writer.write(field_size_bytes, width);
  writer.write(field_size_bytes, height);
}

Error RegionGeometry_Ellipse::parse(const std::vector<uint8_t>& data,
                                    int field_size,
                                    unsigned int* dataOffset,
                                    const heif_security_limits* limits)
{
  unsigned int bytesRequired = (field_size / 8) * 4;
  if (data.size() - *dataOffset < bytesRequired) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for ellipse region");
  }
  x = parse_signed(data, field_size, dataOffset);
  y = parse_signed(data, field_size, dataOffset);
  radius_x = parse_unsigned(data, field_size, dataOffset);
  radius_y = parse_unsigned(data, field_size, dataOffset);
  return Error::Ok;
}

bool RegionGeometry_Ellipse::encode_needs_32bit() const
{
  return exceeds_s16(x) || exceeds_s16(y) || exceeds_u16(radius_x) || exceeds_u16(radius_y);
}


void RegionGeometry_Ellipse::encode(StreamWriter& writer, int field_size_bytes) const
{
  writer.write8(heif_region_type_ellipse);
  writer.write(field_size_bytes, x);
  writer.write(field_size_bytes, y);
  writer.write(field_size_bytes, radius_x);
  writer.write(field_size_bytes, radius_y);
}



Error RegionGeometry_Polygon::parse(const std::vector<uint8_t>& data,
                                    int field_size,
                                    unsigned int* dataOffset,
                                    const heif_security_limits* limits)
{
  uint32_t bytesRequired1 = (field_size / 8) * 1;
  if (data.size() - *dataOffset < bytesRequired1) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for polygon");
  }

  // Note: we need to do the calculation in uint64_t because numPoints may be any 32-bit number
  // and it is multiplied by (at most) 8.

  uint32_t numPoints = parse_unsigned(data, field_size, dataOffset);
  uint64_t bytesRequired2 = (field_size / 8) * uint64_t(numPoints) * 2;
  if (data.size() - *dataOffset < bytesRequired2) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for polygon");
  }

  if (closed) {
    if (numPoints < 3) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Region polygon with less than 3 points."
      };
    }
  }
  else {
    if (numPoints < 2) {
      return {
        heif_error_Invalid_input,
        heif_suberror_Unspecified,
        "Region polyline with less than 2 points."
      };
    }
  }

  if (UINT32_MAX / numPoints < sizeof(Point)) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Unspecified,
      "Region polygon size exceeds integer range."
    };
  }

  if (auto err = m_memory_handle.alloc(numPoints * sizeof(Point), limits, "region polygon")) {
    return err;
  }

  for (uint32_t i = 0; i < numPoints; i++) {
    Point p;
    p.x = parse_signed(data, field_size, dataOffset);
    p.y = parse_signed(data, field_size, dataOffset);
    points.push_back(p);
  }

  return Error::Ok;
}


Error RegionGeometry_ReferencedMask::parse(const std::vector<uint8_t>& data,
                                          int field_size,
                                          unsigned int* dataOffset,
                                          const heif_security_limits* limits)
{
  unsigned int bytesRequired = (field_size / 8) * 4;
  if (data.size() - *dataOffset < bytesRequired) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for referenced mask region");
  }
  x = parse_signed(data, field_size, dataOffset);
  y = parse_signed(data, field_size, dataOffset);
  width = parse_unsigned(data, field_size, dataOffset);
  height = parse_unsigned(data, field_size, dataOffset);
  return Error::Ok;
}


void RegionGeometry_ReferencedMask::encode(StreamWriter& writer, int field_size_bytes) const
{
  writer.write8(heif_region_type_referenced_mask);
  writer.write(field_size_bytes, x);
  writer.write(field_size_bytes, y);
  writer.write(field_size_bytes, width);
  writer.write(field_size_bytes, height);
}

bool RegionGeometry_Polygon::encode_needs_32bit() const
{
  if (exceeds_u16((uint32_t)points.size())) {
    return true;
  }

  for (auto& p : points) {
    if (exceeds_s16(p.x) || exceeds_s16(p.y)) {
      return true;
    }
  }

  return false;
}


void RegionGeometry_Polygon::encode(StreamWriter& writer, int field_size_bytes) const
{
  writer.write8(closed ? heif_region_type_polygon : heif_region_type_polyline);

  writer.write(field_size_bytes, points.size());

  for (auto& p : points) {
    writer.write(field_size_bytes, p.x);
    writer.write(field_size_bytes, p.y);
  }
}


Error RegionGeometry_InlineMask::parse(const std::vector<uint8_t>& data,
                                       int field_size,
                                       unsigned int* dataOffset,
                                       const heif_security_limits* limits)
{
  unsigned int bytesRequired = (field_size / 8) * 4 + 1;
  if (data.size() - *dataOffset < bytesRequired) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for inline mask region");
  }
  x = parse_signed(data, field_size, dataOffset);
  y = parse_signed(data, field_size, dataOffset);
  width = parse_unsigned(data, field_size, dataOffset);
  height = parse_unsigned(data, field_size, dataOffset);
  uint8_t mask_coding_method = data[*dataOffset];
  *dataOffset = *dataOffset + 1;

  if (mask_coding_method != 0) {
    return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Deflate compressed inline mask is not yet supported");
  }

  if (width==0 || height==0) {
    return {
      heif_error_Invalid_input,
      heif_suberror_Unspecified,
      "Zero size mask image."
    };
  }

  if (width / 8 + 1 > UINT32_MAX / height) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Unspecified,
      "Mask image size exceeds maximum integer range."
    };
  }

  if (limits->max_image_size_pixels / width < height) {
    return {
      heif_error_Memory_allocation_error,
      heif_suberror_Security_limit_exceeded,
      "Inline mask image exceeds maximum image size."
    };
  }

  unsigned int additionalBytesRequired = width * height / 8;
  if (data.size() - *dataOffset < additionalBytesRequired) {
        return Error(heif_error_Invalid_input, heif_suberror_Invalid_region_data,
                 "Insufficient data remaining for inline mask region data[]");
  }

  if (auto err = m_memory_handle.alloc(additionalBytesRequired, limits, "region mask")) {
    return err;
  }

  mask_data.resize(additionalBytesRequired);
  std::copy(data.begin() + *dataOffset, data.begin() + *dataOffset + additionalBytesRequired, mask_data.begin());
  return Error::Ok;
}


void RegionGeometry_InlineMask::encode(StreamWriter& writer, int field_size_bytes) const
{
  writer.write8(heif_region_type_inline_mask);
  writer.write(field_size_bytes, x);
  writer.write(field_size_bytes, y);
  writer.write(field_size_bytes, width);
  writer.write(field_size_bytes, height);
  writer.write8(0); // coding method
  // if using some other coding method, there are parameters to write out here.
  writer.write(mask_data);
}


RegionCoordinateTransform RegionCoordinateTransform::create(std::shared_ptr<HeifFile> file,
                                                            heif_item_id item_id,
                                                            int reference_width, int reference_height)
{
  std::vector<std::shared_ptr<Box>> properties;

  Error err = file->get_properties(item_id, properties);
  if (err) {
    return {};
  }

  uint32_t image_width = 0, image_height = 0;

  for (auto& property : properties) {
    if (auto ispe = std::dynamic_pointer_cast<Box_ispe>(property)) {
      image_width = ispe->get_width();
      image_height = ispe->get_height();
      break;
    }
  }

  if (image_width == 0 || image_height == 0) {
    return {};
  }

  RegionCoordinateTransform transform;
  transform.a = image_width / (double) reference_width;
  transform.d = image_height / (double) reference_height;

  for (auto& property : properties) {
    switch (property->get_short_type()) {
      case fourcc("imir"): {
        auto imir = std::dynamic_pointer_cast<Box_imir>(property);
        if (imir->get_mirror_direction() == heif_transform_mirror_direction_horizontal) {
          transform.a = -transform.a;
          transform.b = -transform.b;
          transform.tx = image_width - 1 - transform.tx;
        }
        else {
          transform.c = -transform.c;
          transform.d = -transform.d;
          transform.ty = image_height - 1 - transform.ty;
        }
        break;
      }
      case fourcc("irot"): {
        auto irot = std::dynamic_pointer_cast<Box_irot>(property);
        RegionCoordinateTransform tmp;
        switch (irot->get_rotation_ccw()) {
          case 90:
            tmp.a = transform.c;
            tmp.b = transform.d;
            tmp.c = -transform.a;
            tmp.d = -transform.b;
            tmp.tx = transform.ty;
            tmp.ty = -transform.tx + image_width - 1;
            transform = tmp;
            std::swap(image_width, image_height);
            break;
          case 180:
            transform.a = -transform.a;
            transform.b = -transform.b;
            transform.tx = image_width - 1 - transform.tx;
            transform.c = -transform.c;
            transform.d = -transform.d;
            transform.ty = image_height - 1 - transform.ty;
            break;
          case 270:
            tmp.a = -transform.c;
            tmp.b = -transform.d;
            tmp.c = transform.a;
            tmp.d = transform.b;
            tmp.tx = -transform.ty + image_height - 1;
            tmp.ty = transform.tx;
            transform = tmp;
            std::swap(image_width, image_height);
            break;
          default:
            break;
        }
        break;
      }
      case fourcc("clap"): {
        auto clap = std::dynamic_pointer_cast<Box_clap>(property);
        int left = clap->left_rounded(image_width);
        int top = clap->top_rounded(image_height);
        transform.tx -= left;
        transform.ty -= top;
        image_width = clap->get_width_rounded();
        image_height = clap->get_height_rounded();
        break;
      }
      default:
        break;
    }
  }

  return transform;
}


RegionCoordinateTransform::Point RegionCoordinateTransform::transform_point(Point p)
{
  Point newp;
  newp.x = p.x * a + p.x * b + tx;
  newp.y = p.x * c + p.y * d + ty;
  return newp;
}


RegionCoordinateTransform::Extent RegionCoordinateTransform::transform_extent(Extent e)
{
  Extent newe;
  newe.x = e.x * a + e.y * b;
  newe.y = e.x * c + e.y * d;
  return newe;
}



