/*
 * HEIF codec.
 * Copyright (c) 2017-2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_ENTITY_GROUPS_H
#define LIBHEIF_HEIF_ENTITY_GROUPS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "libheif/heif_library.h"


// ------------------------- entity groups ------------------------

typedef uint32_t heif_entity_group_id;

typedef struct heif_entity_group
{
  heif_entity_group_id entity_group_id;
  uint32_t entity_group_type;  // this is a FourCC constant
  heif_item_id* entities;
  uint32_t num_entities;
} heif_entity_group;

// Use 0 for `type_filter` or `item_filter` to disable the filter.
// Returns an array of heif_entity_group structs with *out_num_groups entries.
LIBHEIF_API
heif_entity_group* heif_context_get_entity_groups(const heif_context*,
                                                  uint32_t type_filter,
                                                  heif_item_id item_filter,
                                                  int* out_num_groups);

// Release an array of entity groups returned by heif_context_get_entity_groups().
LIBHEIF_API
void heif_entity_groups_release(heif_entity_group*, int num_groups);


#ifdef __cplusplus
}
#endif

#endif
