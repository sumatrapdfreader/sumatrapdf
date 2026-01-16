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

#ifndef LIBHEIF_REGION_H
#define LIBHEIF_REGION_H

#include <cstdint>
#include <vector>
#include <memory>
#include "pixelimage.h"
#include "libheif/heif_regions.h"


class RegionGeometry;

class RegionItem
{
public:
  RegionItem() = default;

  RegionItem(heif_item_id itemId, uint32_t ref_width, uint32_t ref_height)
      : item_id(itemId), reference_width(ref_width), reference_height(ref_height) {}

  Error parse(const std::vector<uint8_t>& data, const heif_security_limits*);

  Error encode(std::vector<uint8_t>& result) const;

  int get_number_of_regions() { return (int) mRegions.size(); }

  std::vector<std::shared_ptr<RegionGeometry>> get_regions() { return mRegions; }

  void add_region(const std::shared_ptr<RegionGeometry>& region)
  {
    mRegions.push_back(region);
  }

  heif_item_id item_id = 0;
  uint32_t reference_width = 0;
  uint32_t reference_height = 0;

private:
  std::vector<std::shared_ptr<RegionGeometry>> mRegions;
};


class RegionGeometry
{
public:
  virtual ~RegionGeometry() = default;

  virtual heif_region_type getRegionType() = 0;

  virtual Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
                      const heif_security_limits* limits) = 0;

  virtual bool encode_needs_32bit() const { return false; }

  virtual void encode(StreamWriter&, int field_size_bytes) const {}

protected:
  uint32_t parse_unsigned(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset);

  int32_t parse_signed(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset);
};

class RegionGeometry_Point : public RegionGeometry
{
public:
  Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
              const heif_security_limits* limits) override;

  bool encode_needs_32bit() const override;

  void encode(StreamWriter&, int field_size_bytes) const override;

  heif_region_type getRegionType() override { return heif_region_type_point; }

  int32_t x, y;
};

class RegionGeometry_Rectangle : public RegionGeometry
{
public:
  Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
              const heif_security_limits* limits) override;

  bool encode_needs_32bit() const override;

  void encode(StreamWriter&, int field_size_bytes) const override;

  heif_region_type getRegionType() override { return heif_region_type_rectangle; }

  int32_t x, y;
  uint32_t width, height;
};

class RegionGeometry_Ellipse : public RegionGeometry
{
public:
  Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
              const heif_security_limits* limits) override;

  bool encode_needs_32bit() const override;

  void encode(StreamWriter&, int field_size_bytes) const override;

  heif_region_type getRegionType() override { return heif_region_type_ellipse; }

  int32_t x, y;
  uint32_t radius_x, radius_y;
};

class RegionGeometry_Polygon : public RegionGeometry
{
public:
  Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
              const heif_security_limits* limits) override;

  bool encode_needs_32bit() const override;

  void encode(StreamWriter&, int field_size_bytes) const override;

  heif_region_type getRegionType() override
  {
    return closed ? heif_region_type_polygon : heif_region_type_polyline;
  }

  struct Point
  {
    int32_t x, y;
  };

  bool closed = true;
  std::vector<Point> points;
  MemoryHandle m_memory_handle;
};

class RegionGeometry_ReferencedMask : public RegionGeometry
{
public:
  Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
              const heif_security_limits* limits) override;

  void encode(StreamWriter&, int field_size_bytes) const override;

  heif_region_type getRegionType() override { return heif_region_type_referenced_mask; }

  int32_t x,y;
  uint32_t width, height;
  heif_item_id referenced_item;
};

class RegionGeometry_InlineMask : public RegionGeometry
{
public:
  Error parse(const std::vector<uint8_t>& data, int field_size, unsigned int* dataOffset,
              const heif_security_limits* limits) override;

  void encode(StreamWriter&, int field_size_bytes) const override;

  int32_t x,y;
  uint32_t width, height;
  std::vector<uint8_t> mask_data;
  MemoryHandle m_memory_handle;

  heif_region_type getRegionType() override { return heif_region_type_inline_mask; }
};

class HeifFile;

class RegionCoordinateTransform
{
public:
  static RegionCoordinateTransform create(std::shared_ptr<HeifFile> file,
                                          heif_item_id item_id,
                                          int reference_width, int reference_height);

  struct Point
  {
    double x, y;
  };

  struct Extent
  {
    double x, y;
  };

  Point transform_point(Point);

  Extent transform_extent(Extent);

private:
  double a = 1.0, b = 0.0, c = 0.0, d = 1.0, tx = 0.0, ty = 0.0;
};

#endif //LIBHEIF_REGION_H
