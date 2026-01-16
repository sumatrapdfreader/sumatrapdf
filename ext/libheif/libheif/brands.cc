/*
 * HEIF codec.
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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

#include "brands.h"
#include "file.h"
#include "sequences/track_visual.h"
#include <utility>


static bool check_mif1(const HeifContext* ctx)
{
  auto file = ctx->get_heif_file();

  auto meta = file->get_meta_box();
  if (!meta || meta->get_version() != 0) {
    return false;
  }

  auto hdlr = meta->get_child_box<Box_hdlr>();
  if (!hdlr || hdlr->get_version() != 0) {
    return false;
  }

  auto iloc = meta->get_child_box<Box_iloc>();
  if (!iloc || iloc->get_version() > 2) {
    return false;
  }

  auto iinf = meta->get_child_box<Box_iinf>();
  if (!iinf || iinf->get_version() > 1) {
    return false;
  }

  auto infe = iinf->get_child_box<Box_infe>();
  if (!infe || infe->get_version() < 2 || infe->get_version() > 3) {
    return false;
  }

  auto pitm = meta->get_child_box<Box_pitm>();
  if (!pitm || pitm->get_version() > 1) {
    return false;
  }

  auto iprp = meta->get_child_box<Box_iprp>();
  if (!iprp) {
    return false;
  }

  return true;
}


std::vector<std::shared_ptr<const ImageItem>> get_primary_and_alternative_images(const HeifContext* ctx)
{
  auto img = ctx->get_primary_image(false);
  if (img) {
    return {std::move(img)};
  }
  else {
    return {};
  }
}


std::vector<heif_brand2> compute_compatible_brands(const HeifContext* ctx, heif_brand2* out_main_brand)
{
  std::vector<heif_brand2> compatible_brands;

  heif_brand2 dummy;
  if (out_main_brand == nullptr) {
    out_main_brand = &dummy; // so that we do not have to check for NULL out_main_brand in the following
  }

  *out_main_brand = 0;

  // --- "mif" brands

  bool is_mif1 = check_mif1(ctx);

  if (is_mif1) {
    compatible_brands.push_back(heif_brand2_mif1);
    *out_main_brand = heif_brand2_mif1;
  }

  bool is_structural_image = is_mif1;


  // --- image brand

  std::vector<std::shared_ptr<const ImageItem>> images = get_primary_and_alternative_images(ctx);

  bool miaf_compatible = true;

  for (auto& img : images) {
    heif_brand2 brand = img->get_compatible_brand();
    if (brand != 0 &&
        is_structural_image &&
        std::find(compatible_brands.begin(),
                  compatible_brands.end(),
                  brand) == compatible_brands.end()) {
      compatible_brands.push_back(brand);
    }

    if (!img->is_miaf_compatible()) {
      miaf_compatible = false;
    }
  }

  // --- "miaf"

  if (miaf_compatible && is_structural_image) {
    compatible_brands.push_back(heif_brand2_miaf);
  }


  // --- main brand is first image brand

  if (!images.empty()) {
    heif_brand2 brand = images[0]->get_compatible_brand();
    if (brand != 0) {
      *out_main_brand = brand;
    }
  }

  // --- --- sequences

  if (ctx->has_sequence()) {
    compatible_brands.push_back(heif_brand2_msf1);
    compatible_brands.push_back(heif_brand2_iso8);

    auto track_result = ctx->get_track(0);
    assert(track_result);

    std::shared_ptr<const Track> track = *track_result;
    std::shared_ptr<const Track_Visual> visual_track = std::dynamic_pointer_cast<const Track_Visual>(track);

    heif_brand2 track_brand = visual_track->get_compatible_brand();
    if (track_brand != 0) {
      compatible_brands.push_back(track_brand);

      // overwrite any image brand
      *out_main_brand = track_brand;
    }

    // if we don't have a track brand, use at least the sequence structural brand
    if (*out_main_brand == 0) {
      *out_main_brand = heif_brand2_msf1;
    }
  }

  return compatible_brands;
}

