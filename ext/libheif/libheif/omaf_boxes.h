/*
 * libheif OMAF (ISO/IEC 23090-2)
 *
 * Copyright (c) 2026 Brad Hards <bradh@frogmouth.net>
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

#ifndef LIBHEIF_OMAF_BOXES_H
#define LIBHEIF_OMAF_BOXES_H

#include "libheif/heif.h"
#include "box.h"

#include <memory>
#include <string>

// Projection format for OMAF
// See ISO/IEC 23090-2:2023 Section 7.9.3
class Box_prfr : public FullBox
{
public:
  Box_prfr()
  {
    set_short_type(fourcc("prfr"));
  }

  heif_omaf_image_projection get_omaf_image_projection() const { return m_projection; }

  Error set_image_projection(heif_omaf_image_projection projection);

  std::string dump(Indent&) const override;

  const char* debug_box_name() const override { return "Projection Format"; }

  [[nodiscard]] parse_error_fatality get_parse_error_fatality() const override { return parse_error_fatality::optional; }

  Error write(StreamWriter& writer) const override;

protected:
  Error parse(BitstreamRange& range, const heif_security_limits*) override;

private:
  heif_omaf_image_projection m_projection = heif_omaf_image_projection_equirectangular;
};

#endif