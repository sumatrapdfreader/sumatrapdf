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

#include "jpeg_boxes.h"
#include <string>
#include "security_limits.h"
#include <cstring>
#include "libheif/heif_experimental.h"


// returns 0 if the marker_type was not found
size_t find_jpeg_marker_start(const std::vector<uint8_t>& data, uint8_t marker_type)
{
  for (size_t i = 0; i < data.size() - 1; i++) {
    if (data[i] == 0xFF && data[i + 1] == marker_type) {
      return i;
    }
  }

  return 0;
}


std::string Box_jpgC::dump(Indent& indent) const
{
  std::ostringstream sstr;
  sstr << Box::dump(indent);

  sstr << indent << "num bytes: " << m_data.size() << "\n";

  return sstr.str();
}


Error Box_jpgC::write(StreamWriter& writer) const
{
  size_t box_start = reserve_box_header_space(writer);

  writer.write(m_data);

  prepend_header(writer, box_start);

  return Error::Ok;
}


Error Box_jpgC::parse(BitstreamRange& range, const heif_security_limits* limits)
{
  if (!has_fixed_box_size()) {
    return Error{heif_error_Unsupported_feature, heif_suberror_Unspecified, "jpgC with unspecified size are not supported"};
  }

  size_t nBytes = range.get_remaining_bytes();
  if (limits->max_memory_block_size && nBytes > limits->max_memory_block_size) {
    return Error{heif_error_Invalid_input, heif_suberror_Unspecified, "jpgC block exceeds maximum size"};
  }

  m_data.resize(nBytes);
  range.read(m_data.data(), nBytes);
  return range.get_error();
}
