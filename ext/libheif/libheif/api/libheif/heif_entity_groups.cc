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

#include "heif_entity_groups.h"
#include "box.h"
#include "api_structs.h"
#include "file.h"

#include <memory>
#include <vector>


heif_entity_group* heif_context_get_entity_groups(const heif_context* ctx,
                                                  uint32_t type_filter,
                                                  heif_item_id item_filter,
                                                  int* out_num_groups)
{
  std::shared_ptr<Box_grpl> grplBox = ctx->context->get_heif_file()->get_grpl_box();
  if (!grplBox) {
    *out_num_groups = 0;
    return nullptr;
  }

  std::vector<std::shared_ptr<Box> > all_entity_group_boxes = grplBox->get_all_child_boxes();
  if (all_entity_group_boxes.empty()) {
    *out_num_groups = 0;
    return nullptr;
  }

  // --- filter groups

  std::vector<std::shared_ptr<Box_EntityToGroup> > entity_group_boxes;
  for (auto& group : all_entity_group_boxes) {
    if (type_filter != 0 && group->get_short_type() != type_filter) {
      continue;
    }

    auto groupBox = std::dynamic_pointer_cast<Box_EntityToGroup>(group);
    const std::vector<heif_item_id>& items = groupBox->get_item_ids();

    if (item_filter != 0 && std::all_of(items.begin(), items.end(), [item_filter](heif_item_id item) {
      return item != item_filter;
    })) {
      continue;
    }

    entity_group_boxes.emplace_back(groupBox);
  }

  // --- convert to C structs

  auto* groups = new heif_entity_group[entity_group_boxes.size()];
  for (size_t i = 0; i < entity_group_boxes.size(); i++) {
    const auto& groupBox = entity_group_boxes[i];
    const std::vector<heif_item_id>& items = groupBox->get_item_ids();

    groups[i].entity_group_id = groupBox->get_group_id();
    groups[i].entity_group_type = groupBox->get_short_type();
    groups[i].entities = (items.empty() ? nullptr : new heif_item_id[items.size()]);
    groups[i].num_entities = static_cast<uint32_t>(items.size());

    if (groups[i].entities) {
      // avoid clang static analyzer false positive
      for (size_t k = 0; k < items.size(); k++) {
        groups[i].entities[k] = items[k];
      }
    }
  }

  *out_num_groups = static_cast<int>(entity_group_boxes.size());
  return groups;
}


void heif_entity_groups_release(heif_entity_group* grp, int num_groups)
{
  for (int i = 0; i < num_groups; i++) {
    delete[] grp[i].entities;
  }

  delete[] grp;
}
