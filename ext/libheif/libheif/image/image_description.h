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

#ifndef LIBHEIF_IMAGE_DESCRIPTION_H
#define LIBHEIF_IMAGE_DESCRIPTION_H

#include "error.h"
#include "nclx.h"
#include <libheif/heif_experimental.h>
#include <libheif/heif_uncompressed.h>
#include "omaf_boxes.h"

#include <cassert>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <utility>


// === === Bayer pattern

// --- Bayer pattern expressed as ISO 23001-17 cmpd component indices

struct BayerPatternPixelCmpd
{
  uint32_t cmpd_index;
  float component_gain;
};

struct BayerPatternCmpd
{
  uint16_t pattern_width = 0;
  uint16_t pattern_height = 0;
  std::vector<BayerPatternPixelCmpd> pixels;
};

// --- Bayer pattern expressed as libheif components IDs

struct BayerPattern
{
  uint16_t pattern_width = 0;
  uint16_t pattern_height = 0;
  std::vector<heif_bayer_pattern_pixel> pixels;

  [[nodiscard]] BayerPatternCmpd resolve_to_cmpd(std::map<uint32_t, uint32_t> comp_id_to_cmpd) const;
};


// === Polarization pattern (ISO 23001-17)

struct PolarizationPattern
{
  std::vector<uint32_t> component_ids;  // empty = applies to all components. Either cmpd-index or component-id, depending on context.
  uint16_t pattern_width = 0;
  uint16_t pattern_height = 0;
  std::vector<float> polarization_angles;   // pattern_width * pattern_height entries
                                            // 0xFFFFFFFF bit-pattern (NaN) = no polarization filter
};


// === Sensor bad pixels map (ISO 23001-17)

struct SensorBadPixelsMap
{
  struct BadPixelCoordinate
  {
    uint32_t row, column;
  };

  std::vector<uint32_t> component_ids;  // empty = applies to all components. Either cmpd-index or component-id, depending on context.
  bool correction_applied = false;
  std::vector<uint32_t> bad_rows;
  std::vector<uint32_t> bad_columns;
  std::vector<BadPixelCoordinate> bad_pixels;
};


// === Sensor non-uniformity correction (ISO 23001-17)

struct SensorNonUniformityCorrection
{
  std::vector<uint32_t> component_ids;  // empty = applies to all components. Either cmpd-index or component-id, depending on context.
  bool nuc_is_applied = false;
  uint32_t image_width = 0;
  uint32_t image_height = 0;
  std::vector<float> nuc_gains;    // image_width * image_height entries
  std::vector<float> nuc_offsets;  // image_width * image_height entries
};



// === Image components

// Given a list of libheif component IDs, map them to a list of cmpd IDs through the given map.
// The output does not contain duplicates and is in no particular order.
std::vector<uint32_t> map_component_ids_to_cmpd(const std::vector<uint32_t>& component_ids, const std::map<uint32_t, uint32_t>& comp_id_to_cmpd);

// Map a list of unci component indices to libheif components.
// Use to find the libheif components for a metadata box that assigns it through cmpd indices.
// The output does not contain duplicates and is in no particular order.
std::vector<uint32_t> map_cmpd_to_component_ids(const std::vector<uint32_t>& cmpd_indices, const std::vector<std::vector<uint32_t>>& cmpd_to_comp_ids);

// Find the closest matching heif_channel for an ISO 23001-17 component type.
// Returns heif_channel_unknown if no good mapping exists.
heif_channel map_uncompressed_component_to_channel(uint16_t component_type);


// Per-component description, independent of pixel data.
// Lives on ImageDescription so both ImageItem (handle side, before decoding)
// and HeifPixelImage (decoded side) can carry the same structural view.
struct ComponentDescription
{
  uint32_t component_id = 0;             // stable id, matches HeifPixelImage::m_storage ids

  heif_channel channel = heif_channel_unknown;
  uint16_t component_type = 0;           // heif_cmpd_component_type_*

  // The numeric values of heif_component_datatype are aligned with the
  // ISO 23001-17 Table 2 component_format byte (from the uncC box), so on
  // the unci read path this field directly holds the file byte and on the
  // write path it is emitted as the file byte. For non-unci codecs
  // (HEVC/AVIF/JPEG/...) the codec sets this to the appropriate value
  // (typically heif_component_datatype_unsigned_integer).
  heif_component_datatype datatype = heif_component_datatype_undefined;

  uint16_t bit_depth = 0; // logical bit depth (1..256)
  uint32_t width = 0;
  uint32_t height = 0;
  bool has_data_plane = true; // false for cpat reference colors

  // Empty string means "no content id assigned".
  std::string gimi_content_id;
};


class ImageDescription
{
public:
  virtual ~ImageDescription();

  // TODO: Decide who is responsible for writing the colr boxes.
  //       Currently it is distributed over various places.
  //       Either here, in image_item.cc or in grid.cc.
  std::vector<std::shared_ptr<Box>> generate_property_boxes(bool generate_colr_boxes) const;


  // --- color profile

  bool has_nclx_color_profile() const;

  virtual void set_color_profile_nclx(const nclx_profile& profile) { m_color_profile_nclx = profile; }

  nclx_profile get_color_profile_nclx() const { return m_color_profile_nclx; }

  // get the stored nclx fallback or return the default nclx if none is stored
  nclx_profile get_color_profile_nclx_with_fallback() const;

  virtual void set_color_profile_icc(const std::shared_ptr<const color_profile_raw>& profile) { m_color_profile_icc = profile; }

  bool has_icc_color_profile() const { return m_color_profile_icc != nullptr; }

  const std::shared_ptr<const color_profile_raw>& get_color_profile_icc() const { return m_color_profile_icc; }

  void set_color_profile(const std::shared_ptr<const color_profile>& profile)
  {
    auto icc = std::dynamic_pointer_cast<const color_profile_raw>(profile);
    if (icc) {
      set_color_profile_icc(icc);
    }

    auto nclx = std::dynamic_pointer_cast<const color_profile_nclx>(profile);
    if (nclx) {
      set_color_profile_nclx(nclx->get_nclx_color_profile());
    }
  }


  // --- premultiplied alpha

  bool is_premultiplied_alpha() const { return m_premultiplied_alpha; }

  virtual void set_premultiplied_alpha(bool flag) { m_premultiplied_alpha = flag; }


  // --- pixel aspect ratio

  bool has_nonsquare_pixel_ratio() const { return m_PixelAspectRatio_h != m_PixelAspectRatio_v; }

  void get_pixel_ratio(uint32_t* h, uint32_t* v) const
  {
    *h = m_PixelAspectRatio_h;
    *v = m_PixelAspectRatio_v;
  }

  virtual void set_pixel_ratio(uint32_t h, uint32_t v)
  {
    m_PixelAspectRatio_h = h;
    m_PixelAspectRatio_v = v;
  }

  // --- clli

  bool has_clli() const { return m_clli.max_content_light_level != 0 || m_clli.max_pic_average_light_level != 0; }

  heif_content_light_level get_clli() const { return m_clli; }

  virtual void set_clli(const heif_content_light_level& clli) { m_clli = clli; }

  // --- mdcv

  bool has_mdcv() const { return m_mdcv.has_value(); }

  heif_mastering_display_colour_volume get_mdcv() const { return *m_mdcv; }

  virtual void set_mdcv(const heif_mastering_display_colour_volume& mdcv)
  {
    m_mdcv = mdcv;
  }

  void unset_mdcv() { m_mdcv.reset(); }

  // --- amve (ambient viewing environment)

  bool has_amve() const { return m_amve.has_value(); }

  heif_ambient_viewing_environment get_amve() const { return *m_amve; }

  virtual void set_amve(const heif_ambient_viewing_environment& amve)
  {
    m_amve = amve;
  }

  void unset_amve() { m_amve.reset(); }

  // --- ndwt (nominal diffuse white)

  // Note: a luminance of 0 is a valid value (it selects the ISO/TS 22028-5
  // default), so presence is tracked separately from the value via std::optional.

  bool has_nominal_diffuse_white() const { return m_nominal_diffuse_white_luminance.has_value(); }

  // Nominal diffuse white luminance in units of 0.0001 cd/m^2.
  uint32_t get_nominal_diffuse_white_luminance() const { return m_nominal_diffuse_white_luminance.value_or(0); }

  virtual void set_nominal_diffuse_white_luminance(uint32_t luminance)
  {
    m_nominal_diffuse_white_luminance = luminance;
  }

  void unset_nominal_diffuse_white() { m_nominal_diffuse_white_luminance.reset(); }

  virtual Error set_tai_timestamp(const heif_tai_timestamp_packet* tai) {
    delete m_tai_timestamp;

    m_tai_timestamp = heif_tai_timestamp_packet_alloc();
    heif_tai_timestamp_packet_copy(m_tai_timestamp, tai);
    return Error::Ok;
  }

  [[nodiscard]] const heif_tai_timestamp_packet* get_tai_timestamp() const {
    return m_tai_timestamp;
  }

  // --- GIMI content ID

  virtual void set_gimi_sample_content_id(std::string id) { m_gimi_sample_content_id = std::move(id); }

  [[nodiscard]] bool has_gimi_sample_content_id() const { return !m_gimi_sample_content_id.empty(); }

  [[nodiscard]] const std::string& get_gimi_sample_content_id() const { return m_gimi_sample_content_id; }


  [[nodiscard]] bool has_component_content_ids() const
  {
    return std::any_of(m_components.begin(), m_components.end(),
                       [](const ComponentDescription& c) { return !c.gimi_content_id.empty(); });
  }

  // Returns a positional vector: entry i is m_components[i].gimi_content_id
  // (empty string if unset). Returned by value because it is reconstructed.
  [[nodiscard]] std::vector<std::string> get_component_content_ids() const
  {
    std::vector<std::string> ids;
    ids.reserve(m_components.size());
    for (const auto& c : m_components) {
      ids.push_back(c.gimi_content_id);
    }
    return ids;
  }


  // --- per-component descriptions (id-based)
  // These describe each component independent of pixel storage. Both ImageItem
  // (before decoding) and HeifPixelImage (after decoding) carry the same list.

  [[nodiscard]] const std::vector<ComponentDescription>& get_component_descriptions() const { return m_components; }

  // Append a ComponentDescription. The caller is expected to have set
  // component_id, either from a fresh mint_component_id() call or by
  // matching an id already used by a parallel structure (e.g.
  // HeifPixelImage::ComponentStorage::m_component_ids).
  void add_component_description(ComponentDescription desc)
  {
    m_components.push_back(std::move(desc));
  }

  // Bulk-replace the component descriptions and sync the next-id allocator.
  // Used by clone/apply paths that rebuild the whole list at once.
  void set_component_descriptions(std::vector<ComponentDescription> components, uint32_t next_id)
  {
    m_components = std::move(components);
    m_next_component_id = next_id;
  }

  // Erase the description matching this component_id. Returns true if one was
  // removed.
  bool remove_component_description(uint32_t component_id)
  {
    auto it = std::find_if(m_components.begin(), m_components.end(),
                           [component_id](const ComponentDescription& c) {
                             return c.component_id == component_id;
                           });
    if (it == m_components.end()) return false;
    m_components.erase(it);
    return true;
  }

  // Mint a fresh component id (monotonically increasing, starting at 1).
  [[nodiscard]] uint32_t mint_component_id() { return m_next_component_id++; }

  // Read-only view of the next id that mint_component_id() would return.
  // Used by HeifPixelImage::clone_component_descriptions_from() to keep the
  // counter aligned across an item-to-image clone.
  [[nodiscard]] uint32_t peek_next_component_id() const { return m_next_component_id; }

  [[nodiscard]] ComponentDescription* find_component_description(uint32_t component_id)
  {
    for (auto& c : m_components) {
      if (c.component_id == component_id) return &c;
    }
    return nullptr;
  }

  [[nodiscard]] const ComponentDescription* find_component_description(uint32_t component_id) const
  {
    for (const auto& c : m_components) {
      if (c.component_id == component_id) return &c;
    }
    return nullptr;
  }


  // --- bayer pattern

  bool has_bayer_pattern(uint32_t component_id) const { return m_bayer_pattern.has_value(); }

  bool has_any_bayer_pattern() const { return m_bayer_pattern.has_value(); }

  const BayerPattern& get_bayer_pattern(uint32_t component_id) const { assert(has_bayer_pattern(component_id)); return *m_bayer_pattern; }

  const BayerPattern& get_any_bayer_pattern() const { assert(has_any_bayer_pattern()); return *m_bayer_pattern; }

  virtual void set_bayer_pattern(const BayerPattern& pattern) { m_bayer_pattern = pattern; }


  // --- polarization pattern

  bool has_polarization_patterns() const { return !m_polarization_patterns.empty(); }

  const std::vector<PolarizationPattern>& get_polarization_patterns() const { return m_polarization_patterns; }

  virtual void set_polarization_patterns(const std::vector<PolarizationPattern>& p) { m_polarization_patterns = p; }

  virtual void add_polarization_pattern(const PolarizationPattern& p) { m_polarization_patterns.push_back(p); }


  // --- sensor bad pixels map

  bool has_sensor_bad_pixels_maps() const { return !m_sensor_bad_pixels_maps.empty(); }

  const std::vector<SensorBadPixelsMap>& get_sensor_bad_pixels_maps() const { return m_sensor_bad_pixels_maps; }

  virtual void set_sensor_bad_pixels_maps(const std::vector<SensorBadPixelsMap>& m) { m_sensor_bad_pixels_maps = m; }

  virtual void add_sensor_bad_pixels_map(const SensorBadPixelsMap& m) { m_sensor_bad_pixels_maps.push_back(m); }


  // --- sensor non-uniformity correction

  bool has_sensor_nuc() const { return !m_sensor_nuc.empty(); }

  const std::vector<SensorNonUniformityCorrection>& get_sensor_nuc() const { return m_sensor_nuc; }

  virtual void set_sensor_nuc(const std::vector<SensorNonUniformityCorrection>& n) { m_sensor_nuc = n; }

  virtual void add_sensor_nuc(const SensorNonUniformityCorrection& n) { m_sensor_nuc.push_back(n); }


  // --- chroma sample location (ISO 23001-17, Section 6.1.4)

  bool has_chroma_location() const { return m_chroma_location.has_value(); }

  uint8_t get_chroma_location() const { return m_chroma_location.value_or(0); }

  virtual void set_chroma_location(uint8_t loc) { m_chroma_location = loc; }


  // --- sample duration (for images that are frames in a sequence)
  // 0 means "no duration assigned" (the default for still images).

  void set_sample_duration(uint32_t d) { m_sample_duration = d; }

  uint32_t get_sample_duration() const { return m_sample_duration; }


  bool has_omaf_image_projection() const {
    return (m_omaf_image_projection != heif_omaf_image_projection_flat);
  }

  const heif_omaf_image_projection get_omaf_image_projection() const {
    return m_omaf_image_projection;
  }

  virtual void set_omaf_image_projection(const heif_omaf_image_projection projection) {
    m_omaf_image_projection = projection;
  }

  // Copies all per-image metadata from `other` (color profiles, premultiplied
  // alpha, pixel aspect ratio, clli, mdcv, tai timestamp, gimi sample content
  // id, bayer pattern, polarization patterns, sensor maps, sensor nuc, chroma
  // location, omaf projection). Per-component descriptions
  // (m_components / m_next_component_id) are intentionally not copied; those
  // are managed separately by callers (via add_channel / add_component, or
  // set_component_descriptions on the destination).
  //
  // Bayer / polarization / sensor-map metadata refers to image geometry;
  // transforms that change orientation or position copy them verbatim and
  // would need separate geometry adjustment to remain semantically correct.
  void copy_metadata_from(const ImageDescription& other);

private:
  bool m_premultiplied_alpha = false;
  nclx_profile m_color_profile_nclx = nclx_profile::undefined();
  std::shared_ptr<const color_profile_raw> m_color_profile_icc;

  uint32_t m_PixelAspectRatio_h = 1;
  uint32_t m_PixelAspectRatio_v = 1;
  heif_content_light_level m_clli{};
  std::optional<heif_mastering_display_colour_volume> m_mdcv;
  std::optional<heif_ambient_viewing_environment> m_amve;
  std::optional<uint32_t> m_nominal_diffuse_white_luminance;

  heif_tai_timestamp_packet* m_tai_timestamp = nullptr;

  // Empty string means "no content id assigned".
  std::string m_gimi_sample_content_id;

  // Per-component description vector. Single source of truth for per-component
  // metadata (id, channel, type, format, datatype, bit depth, dims, content ID).
  std::vector<ComponentDescription> m_components;

  // ID allocator for the per-component descriptions above. Used both by
  // HeifPixelImage (decoded side) and ImageItem (handle side) via the
  // mint_component_id() / peek_next_component_id() accessors.
  uint32_t m_next_component_id = 1;

  std::optional<BayerPattern> m_bayer_pattern;

  std::vector<PolarizationPattern> m_polarization_patterns;

  std::vector<SensorBadPixelsMap> m_sensor_bad_pixels_maps;

  std::vector<SensorNonUniformityCorrection> m_sensor_nuc;

  std::optional<uint8_t> m_chroma_location;

  uint32_t m_sample_duration = 0; // duration of a sequence frame, 0 for stills

  heif_omaf_image_projection m_omaf_image_projection = heif_omaf_image_projection::heif_omaf_image_projection_flat;

protected:
  std::shared_ptr<Box_clli> create_clli_box() const;

  std::shared_ptr<Box_mdcv> create_mdcv_box() const;

  std::shared_ptr<Box_amve> create_amve_box() const;

  std::shared_ptr<Box_ndwt> create_ndwt_box() const;

  std::shared_ptr<Box_pasp> create_pasp_box() const;

  std::shared_ptr<Box_colr> create_colr_box_nclx() const;

  std::shared_ptr<Box_colr> create_colr_box_icc() const;

  std::shared_ptr<Box_prfr> create_prfr_box() const;
};

#endif
