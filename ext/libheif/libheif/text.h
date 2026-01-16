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

#ifndef LIBHEIF_TEXT_H
#define LIBHEIF_TEXT_H

#include <cstring>
#include <string>
#include <vector>
#include "error.h"
#include "libheif/heif_text.h"

class TextItem
{
public:
  TextItem() = default;

  TextItem(heif_item_id itemId, const char *text) : m_item_id(itemId), m_text(text) {}

  Error parse(const std::vector<uint8_t> &data);

  Result<std::vector<uint8_t>> encode() const;

  heif_item_id get_item_id() const {
    return m_item_id;
  }

  void set_item_id(heif_item_id id) {
    m_item_id = id;
  }

  std::string get_item_text() const { return m_text; }

private:
  heif_item_id m_item_id = 0;
  std::string m_text;
};

#endif // LIBHEIF_TEXT_H
