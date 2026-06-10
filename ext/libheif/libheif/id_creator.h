/*
 * HEIF codec.
 * Copyright (c) 2026 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_ID_CREATOR_H
#define LIBHEIF_ID_CREATOR_H

#include "error.h"
#include <cstdint>

class IDCreator
{
public:
  enum class Namespace { item, track, entity_group };

  void set_unif(bool flag) { m_unif = flag; }

  bool get_unif() const { return m_unif; }

  // Returns a new unique ID for the given namespace.
  // In non-unif mode: separate counters per namespace.
  // In unif mode: single global counter shared across all namespaces.
  // Returns error on overflow (counter would exceed 0xFFFFFFFF).
  Result<uint32_t> get_new_id(Namespace ns);

private:
  bool m_unif = false;
  uint32_t m_next_id_item = 1;
  uint32_t m_next_id_track = 1;
  uint32_t m_next_id_entity_group = 1;
  uint32_t m_next_id_global = 1;  // used in unif mode
};

#endif
