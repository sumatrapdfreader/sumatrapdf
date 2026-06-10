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


#include "image_description.h"

#include "codecs/uncompressed/unc_types.h"

#if WITH_UNCOMPRESSED_CODEC
#include "codecs/uncompressed/unc_boxes.h"
#endif

#include <algorithm>


heif_channel map_uncompressed_component_to_channel(uint16_t component_type)
{
  switch (component_type) {
    case heif_cmpd_component_type_monochrome:
    case heif_cmpd_component_type_Y:
      return heif_channel_Y;
    case heif_cmpd_component_type_Cb:
      return heif_channel_Cb;
    case heif_cmpd_component_type_Cr:
      return heif_channel_Cr;
    case heif_cmpd_component_type_red:
      return heif_channel_R;
    case heif_cmpd_component_type_green:
      return heif_channel_G;
    case heif_cmpd_component_type_blue:
      return heif_channel_B;
    case heif_cmpd_component_type_alpha:
      return heif_channel_Alpha;
    case heif_cmpd_component_type_filter_array:
      return heif_channel_filter_array;
    case heif_cmpd_component_type_depth:
      return heif_channel_depth;
    case heif_cmpd_component_type_disparity:
      return heif_channel_disparity;

    case heif_cmpd_component_type_padded:
    default:
      return heif_channel_unknown;
  }
}


BayerPatternCmpd BayerPattern::resolve_to_cmpd(std::map<uint32_t, uint32_t> comp_id_to_cmpd) const
{
  BayerPatternCmpd cpat;
  cpat.pattern_width = pattern_width;
  cpat.pattern_height = pattern_height;

  for (auto p : pixels) {
    assert(comp_id_to_cmpd.find(p.component_id) != comp_id_to_cmpd.end());
    cpat.pixels.push_back({comp_id_to_cmpd[p.component_id], p.component_gain});
  }

  return cpat;
}


static void remove_duplicates(std::vector<uint32_t>& v)
{
  std::sort(v.begin(), v.end());
  v.erase(std::unique(v.begin(), v.end()), v.end());
}


std::vector<uint32_t> map_component_ids_to_cmpd(const std::vector<uint32_t>& component_ids, const std::map<uint32_t, uint32_t>& comp_id_to_cmpd)
{
  std::vector<uint32_t> cmpd_indices;

  for (uint32_t comp_id : component_ids) {
    assert(comp_id_to_cmpd.find(comp_id) != comp_id_to_cmpd.end());
    cmpd_indices.push_back(comp_id_to_cmpd.find(comp_id)->second);
  }

  remove_duplicates(cmpd_indices);

  return cmpd_indices;
}

std::vector<uint32_t> map_cmpd_to_component_ids(const std::vector<uint32_t>& cmpd_indices, const std::vector<std::vector<uint32_t>>& cmpd_to_comp_ids)
{
  std::vector<uint32_t> component_ids;

  for (uint32_t idx : cmpd_indices) {
    assert(idx < cmpd_to_comp_ids.size());
    component_ids.insert(component_ids.end(), cmpd_to_comp_ids[idx].begin(), cmpd_to_comp_ids[idx].end());
  }

  remove_duplicates(component_ids);

  return component_ids;
}


ImageDescription::~ImageDescription()
{
  heif_tai_timestamp_packet_release(m_tai_timestamp);
}


void ImageDescription::copy_metadata_from(const ImageDescription& other)
{
  m_premultiplied_alpha = other.m_premultiplied_alpha;
  m_color_profile_nclx = other.m_color_profile_nclx;
  m_color_profile_icc = other.m_color_profile_icc;

  m_PixelAspectRatio_h = other.m_PixelAspectRatio_h;
  m_PixelAspectRatio_v = other.m_PixelAspectRatio_v;

  m_clli = other.m_clli;
  m_mdcv = other.m_mdcv;
  m_amve = other.m_amve;
  m_nominal_diffuse_white_luminance = other.m_nominal_diffuse_white_luminance;

  heif_tai_timestamp_packet_release(m_tai_timestamp);
  m_tai_timestamp = nullptr;
  if (other.m_tai_timestamp) {
    m_tai_timestamp = heif_tai_timestamp_packet_alloc();
    heif_tai_timestamp_packet_copy(m_tai_timestamp, other.m_tai_timestamp);
  }

  m_gimi_sample_content_id = other.m_gimi_sample_content_id;

  m_bayer_pattern = other.m_bayer_pattern;
  m_polarization_patterns = other.m_polarization_patterns;
  m_sensor_bad_pixels_maps = other.m_sensor_bad_pixels_maps;
  m_sensor_nuc = other.m_sensor_nuc;

  m_chroma_location = other.m_chroma_location;

  m_sample_duration = other.m_sample_duration;

  m_omaf_image_projection = other.m_omaf_image_projection;
}


bool ImageDescription::has_nclx_color_profile() const
{
  return m_color_profile_nclx != nclx_profile::undefined();
}


nclx_profile ImageDescription::get_color_profile_nclx_with_fallback() const
{
  if (has_nclx_color_profile()) {
    return get_color_profile_nclx();
  }
  else {
    return nclx_profile::defaults();
  }
}


std::shared_ptr<Box_clli> ImageDescription::create_clli_box() const
{
  if (!has_clli()) {
    return {};
  }

  auto clli = std::make_shared<Box_clli>();
  clli->clli = get_clli();

  return clli;
}


std::shared_ptr<Box_mdcv> ImageDescription::create_mdcv_box() const
{
  if (!has_mdcv()) {
    return {};
  }

  auto mdcv = std::make_shared<Box_mdcv>();
  mdcv->mdcv = get_mdcv();

  return mdcv;
}


std::shared_ptr<Box_amve> ImageDescription::create_amve_box() const
{
  if (!has_amve()) {
    return {};
  }

  auto amve = std::make_shared<Box_amve>();
  amve->amve = get_amve();

  return amve;
}


std::shared_ptr<Box_ndwt> ImageDescription::create_ndwt_box() const
{
  if (!has_nominal_diffuse_white()) {
    return {};
  }

  auto ndwt = std::make_shared<Box_ndwt>();
  ndwt->set_diffuse_white_luminance(get_nominal_diffuse_white_luminance());

  return ndwt;
}


std::shared_ptr<Box_pasp> ImageDescription::create_pasp_box() const
{
  if (!has_nonsquare_pixel_ratio()) {
    return {};
  }

  auto pasp = std::make_shared<Box_pasp>();
  pasp->hSpacing = m_PixelAspectRatio_h;
  pasp->vSpacing = m_PixelAspectRatio_v;

  return pasp;
}


std::shared_ptr<Box_colr> ImageDescription::create_colr_box_nclx() const
{
  if (!has_nclx_color_profile()) {
    return {};
  }

  auto colr = std::make_shared<Box_colr>();
  colr->set_color_profile(std::make_shared<color_profile_nclx>(get_color_profile_nclx()));
  return colr;
}


std::shared_ptr<Box_colr> ImageDescription::create_colr_box_icc() const
{
  if (!has_icc_color_profile()) {
    return {};
  }

  auto colr = std::make_shared<Box_colr>();
  colr->set_color_profile(get_color_profile_icc());
  return colr;
}

std::shared_ptr<Box_prfr> ImageDescription::create_prfr_box() const
{
  if (!has_omaf_image_projection()) {
    return {};
  }

  auto prfr = std::make_shared<Box_prfr>();
  if (prfr->set_image_projection(get_omaf_image_projection())) {
    return {};
  }

  return prfr;
}

std::vector<std::shared_ptr<Box>> ImageDescription::generate_property_boxes(bool generate_colr_boxes) const
{
  std::vector<std::shared_ptr<Box>> properties;

  // --- write PASP property

  if (has_nonsquare_pixel_ratio()) {
    auto pasp = std::make_shared<Box_pasp>();
    get_pixel_ratio(&pasp->hSpacing, &pasp->vSpacing);

    properties.push_back(pasp);
  }


  // --- write CLLI property

  if (has_clli()) {
    properties.push_back(create_clli_box());
  }


  // --- write MDCV property

  if (has_mdcv()) {
    auto mdcv = std::make_shared<Box_mdcv>();
    mdcv->mdcv = get_mdcv();

    properties.push_back(mdcv);
  }


  // --- write AMVE property

  if (has_amve()) {
    properties.push_back(create_amve_box());
  }


  // --- write NDWT property

  if (has_nominal_diffuse_white()) {
    properties.push_back(create_ndwt_box());
  }


  // --- write TAI property

  if (auto* tai = get_tai_timestamp()) {
    auto itai = std::make_shared<Box_itai>();
    itai->set_from_tai_timestamp_packet(tai);

    properties.push_back(itai);
  }

  // --- write GIMI content ID property

  if (has_gimi_sample_content_id()) {
    auto gimi = std::make_shared<Box_gimi_content_id>();
    gimi->set_content_id(get_gimi_sample_content_id());
    properties.push_back(gimi);
  }

  if (generate_colr_boxes) {
    // --- colr (nclx)

    if (has_nclx_color_profile()) {
      properties.push_back(create_colr_box_nclx());
    }

    // --- colr (icc)

    if (has_icc_color_profile()) {
      properties.push_back(create_colr_box_icc());
    }
  }

  if (has_omaf_image_projection()) {
    auto prfr = std::make_shared<Box_prfr>();
    prfr->set_image_projection(get_omaf_image_projection());
    properties.push_back(prfr);
  }

#if WITH_UNCOMPRESSED_CODEC
  if (has_component_content_ids()) {
    auto ccid = std::make_shared<Box_gimi_component_content_ids>();
    ccid->set_content_ids(get_component_content_ids());
    properties.push_back(ccid);
  }
#endif

  return properties;
}
