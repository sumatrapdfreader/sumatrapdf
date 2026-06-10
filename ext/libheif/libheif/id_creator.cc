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

#include "id_creator.h"

Result<uint32_t> IDCreator::get_new_id(Namespace ns)
{
  if (m_unif) {
    if (m_next_id_global == 0) {
      return Error(heif_error_Usage_error,
                   heif_suberror_Unspecified,
                   "ID namespace overflow");
    }
    return m_next_id_global++;
  }

  uint32_t* counter = nullptr;
  switch (ns) {
    case Namespace::item:
      counter = &m_next_id_item;
      break;
    case Namespace::track:
      counter = &m_next_id_track;
      break;
    case Namespace::entity_group:
      counter = &m_next_id_entity_group;
      break;
  }

  if (*counter == 0) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Unspecified,
                 "ID namespace overflow");
  }

  return (*counter)++;
}
