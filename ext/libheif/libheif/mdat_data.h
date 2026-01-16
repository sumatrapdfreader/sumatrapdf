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

#ifndef LIBHEIF_MDAT_DATA_H
#define LIBHEIF_MDAT_DATA_H

#include <cstdint>
#include <vector>
#include <cassert>
#include <algorithm>


class MdatData
{
public:
  virtual ~MdatData() = default;

  // returns the start position of the appended data
  virtual size_t append_data(const std::vector<uint8_t>& data) = 0;

  virtual size_t get_data_size() const = 0;

  virtual Error write(StreamWriter&) = 0;
};


class MdatData_Memory : public MdatData
{
public:
  size_t append_data(const std::vector<uint8_t>& data) override {
    size_t startPos = m_data.size();
    m_data.insert(m_data.end(), data.begin(), data.end());
    return startPos;
  }

  size_t get_data_size() const override { return m_data.size(); }

  Error write(StreamWriter& writer) override
  {
    writer.write(m_data);
    return Error::Ok;
  }

private:
  std::vector<uint8_t> m_data;
};

#endif //LIBHEIF_MDAT_DATA_H
