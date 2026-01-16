/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_LOGGING_H
#define LIBHEIF_LOGGING_H

#include <cinttypes>
#include <cstddef>

#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <istream>


class Indent
{
public:
  Indent() = default;

  int get_indent() const { return m_indent; }

  void operator++(int) { m_indent++; }

  void operator--(int)
  {
    m_indent--;
    if (m_indent < 0) m_indent = 0;
  }

  std::string get_string() const;

private:
  int m_indent = 0;
};


inline std::ostream& operator<<(std::ostream& ostr, const Indent& indent)
{
  ostr << indent.get_string();
  return ostr;
}

std::string write_raw_data_as_hex(const uint8_t* data, size_t len,
                                  const std::string& firstLineIndent,
                                  const std::string& remainingLinesIndent);

#endif
