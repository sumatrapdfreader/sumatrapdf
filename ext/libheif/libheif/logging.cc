/*
 * HEIF codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#include "logging.h"
#include <sstream>
#include <iomanip>


std::string Indent::get_string() const
{
  std::stringstream sstr;

  for (int i = 0; i < get_indent(); i++) {
    sstr << "| ";
  }

  return sstr.str();
}


std::string write_raw_data_as_hex(const uint8_t* data, size_t len,
                                  const std::string& firstLineIndent,
                                  const std::string& remainingLinesIndent)
{
  std::stringstream sstr;

  sstr << std::hex << std::setfill('0');

  for (size_t i = 0; i < len; i++) {
    if (i % 16 == 0) {
      // start of line

      if (i == 0) {
        sstr << firstLineIndent;
      }
      else {
        sstr << remainingLinesIndent;
      }
      sstr << std::setw(4) << i << ": "; // address
    }
    else if (i % 16 == 8) {
      // space in middle
      sstr << "  ";
    }
    else {
      // space between bytes
      sstr << " ";
    }

    sstr << std::setw(2) << ((int) data[i]);

    if (i % 16 == 15 || i == len - 1) {
      sstr << "\n";
    }
  }

  return sstr.str();
}
