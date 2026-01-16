/*
 * HEIF JPEG codec.
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

#ifndef LIBHEIF_JPEG_BOXES_H
#define LIBHEIF_JPEG_BOXES_H

#include "box.h"
#include <string>
#include <vector>
#include "image-items/image_item.h"
#include <memory>
#include "sequences/seq_boxes.h"


class Box_jpgC : public Box
{
public:
  Box_jpgC()
  {
    set_short_type(fourcc("jpgC"));
  }

  const std::vector<uint8_t>& get_data() const { return m_data; }

  void set_data(const std::vector<uint8_t>& data) { m_data = data; }

  std::string dump(Indent&) const override;

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits* limits) override;

private:
  std::vector<uint8_t> m_data;
};


class Box_mjpg : public Box_VisualSampleEntry
{
public:
  Box_mjpg()
  {
    set_short_type(fourcc("mjpg"));
  }
};


#endif // LIBHEIF_JPEG_BOXES_H
