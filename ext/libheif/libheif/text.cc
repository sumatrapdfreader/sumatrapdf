/*
 * HEIF codec.
 * Copyright (c) 2023 Dirk Farin <dirk.farin@gmail.com>
 * Copyright (c) 2025 Brad Hards <bradh@frogmouth.net>
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

#include <string>
#include "text.h"
#include "error.h"
#include <iostream>


Result<std::vector<uint8_t>> TextItem::encode() const
{
  std::vector<uint8_t> data;
  data.assign(m_text.begin(), m_text.end());
  return data;
}

Error TextItem::parse(const std::vector<uint8_t> &data)
{
  m_text.assign(data.begin(), data.end());
  return Error::Ok;
}