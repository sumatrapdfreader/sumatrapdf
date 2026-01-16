/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_CONTEXT_H
#define LIBHEIF_CONTEXT_H

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>
#include <utility>

#include "error.h"

#include "libheif/heif.h"
#include "libheif/heif_experimental.h"
#include "libheif/heif_plugin.h"
#include "bitstream.h"

#include "box.h" // only for color_profile, TODO: maybe move the color_profiles to its own header
#include "file.h"
#include "region.h"

#include "text.h"

class HeifFile;

class HeifPixelImage;

class StreamWriter;

class ImageItem;

class Track;

struct TrackOptions;


Result<std::shared_ptr<HeifPixelImage>>
create_alpha_image_from_image_alpha_channel(const std::shared_ptr<HeifPixelImage>& image,
                                            const heif_security_limits* limits);


// This is a higher-level view than HeifFile.
// Images are grouped logically into main images and their thumbnails.
// The class also handles automatic color-space conversion.
class HeifContext : public ErrorBuffer
{
public:
  HeifContext();

  ~HeifContext();

  void set_max_decoding_threads(int max_threads) { m_max_decoding_threads = max_threads; }

  int get_max_decoding_threads() const { return m_max_decoding_threads; }

  void set_security_limits(const heif_security_limits* limits);

  [[nodiscard]] heif_security_limits* get_security_limits() { return &m_limits; }

  [[nodiscard]] const heif_security_limits* get_security_limits() const { return &m_limits; }

  Error read(const std::shared_ptr<StreamReader>& reader);

  Error read_from_file(const char* input_filename);

  Error read_from_memory(const void* data, size_t size, bool copy);

  std::shared_ptr<HeifFile> get_heif_file() const { return m_heif_file; }


  // === image items ===

  std::vector<std::shared_ptr<ImageItem>> get_top_level_images(bool return_error_images);

  void insert_image_item(heif_item_id id, const std::shared_ptr<ImageItem>& img) {
    m_all_images.insert(std::make_pair(id, img));
  }

  std::shared_ptr<ImageItem> get_image(heif_item_id id, bool return_error_images);

  std::shared_ptr<const ImageItem> get_image(heif_item_id id, bool return_error_images) const
  {
    return const_cast<HeifContext*>(this)->get_image(id, return_error_images);
  }

  std::shared_ptr<ImageItem> get_primary_image(bool return_error_image);

  std::shared_ptr<const ImageItem> get_primary_image(bool return_error_image) const;

  bool is_image(heif_item_id ID) const;

  bool has_alpha(heif_item_id ID) const;

  Result<std::shared_ptr<HeifPixelImage>> decode_image(heif_item_id ID,
                                                       heif_colorspace out_colorspace,
                                                       heif_chroma out_chroma,
                                                       const heif_decoding_options& options,
                                                       bool decode_only_tile, uint32_t tx, uint32_t ty,
                                                       std::set<heif_item_id> processed_ids) const;

  Result<std::shared_ptr<HeifPixelImage>> convert_to_output_colorspace(std::shared_ptr<HeifPixelImage> img,
                                                                       heif_colorspace out_colorspace,
                                                                       heif_chroma out_chroma,
                                                                       const heif_decoding_options& options) const;

  Error get_id_of_non_virtual_child_image(heif_item_id in, heif_item_id& out) const;

  std::string debug_dump_boxes() const;


  // === writing ===

  void write(StreamWriter& writer);

  // Create all boxes necessary for an empty HEIF file.
  // Note that this is no valid HEIF file, since some boxes (e.g. pitm) are generated, but
  // contain no valid data yet.
  void reset_to_empty_heif();

  Result<std::shared_ptr<ImageItem>> encode_image(const std::shared_ptr<HeifPixelImage>& image,
                                                  heif_encoder* encoder,
                                                  const heif_encoding_options& options,
                                                  heif_image_input_class input_class);

  void set_primary_image(const std::shared_ptr<ImageItem>& image);

  bool is_primary_image_set() const { return m_primary_image != nullptr; }

  Error assign_thumbnail(const std::shared_ptr<ImageItem>& master_image,
                         const std::shared_ptr<ImageItem>& thumbnail_image);

  Result<std::shared_ptr<ImageItem>> encode_thumbnail(const std::shared_ptr<HeifPixelImage>& image,
                                                      heif_encoder* encoder,
                                                      const heif_encoding_options& options,
                                                      int bbox_size);

  Error add_exif_metadata(const std::shared_ptr<ImageItem>& master_image, const void* data, int size);

  Error add_XMP_metadata(const std::shared_ptr<ImageItem>& master_image, const void* data, int size, heif_metadata_compression compression);

  Error add_generic_metadata(const std::shared_ptr<ImageItem>& master_image, const void* data, int size,
                             uint32_t item_type, const char* content_type, const char* item_uri_type,
                             heif_metadata_compression compression, heif_item_id* out_item_id);

  heif_property_id add_property(heif_item_id targetItem, std::shared_ptr<Box> property, bool essential);

  Result<heif_item_id> add_pyramid_group(const std::vector<heif_item_id>& layers);

  Result<heif_property_id> add_text_property(heif_item_id, const std::string& language);


  // --- region items

  void add_region_item(std::shared_ptr<RegionItem> region_item)
  {
    m_region_items.push_back(std::move(region_item));
  }

  std::shared_ptr<RegionItem> add_region_item(uint32_t reference_width, uint32_t reference_height);

  std::shared_ptr<RegionItem> get_region_item(heif_item_id id) const
  {
    for (auto& item : m_region_items) {
      if (item->item_id == id)
        return item;
    }

    return nullptr;
  }

  void add_region_referenced_mask_ref(heif_item_id region_item_id, heif_item_id mask_item_id);


  // === sequences ==

  bool has_sequence() const { return !m_tracks.empty(); }

  int get_number_of_tracks() const { return static_cast<int>(m_tracks.size()); }

  std::vector<uint32_t> get_track_IDs() const;

  // If 0 is passed as track_id, the main visual track is returned (we assume that there is only one visual track).
  Result<std::shared_ptr<Track>> get_track(uint32_t track_id);

  Result<std::shared_ptr<const Track>> get_track(uint32_t track_id) const;

  uint32_t get_sequence_timescale() const;

  uint64_t get_sequence_duration() const;

  void set_sequence_timescale(uint32_t timescale);

  void set_number_of_sequence_repetitions(uint32_t repetitions);

  Result<std::shared_ptr<class Track_Visual>> add_visual_sequence_track(const TrackOptions*, uint32_t handler_type,
                                                                        uint16_t width, uint16_t height);

  Result<std::shared_ptr<class Track_Metadata>> add_uri_metadata_sequence_track(const TrackOptions*, std::string uri);

  void add_text_item(std::shared_ptr<TextItem> text_item)
  {
    m_text_items.push_back(std::move(text_item));
  }

  std::shared_ptr<TextItem> add_text_item(const char* content_type, const char* text);

  std::shared_ptr<TextItem> get_text_item(heif_item_id id) const
  {
    for (auto& item : m_text_items) {
      if (item->get_item_id() == id)
        return item;
    }

    return nullptr;
  }

  template<typename T>
  Result<std::shared_ptr<T>> find_property(heif_item_id itemId, heif_property_id propertyId)
  {
    auto file = this->get_heif_file();

    // For propertyId == 0, return the first property with this type.
    if (propertyId == 0) {
      return find_property<T>(itemId);
    }

    std::vector<std::shared_ptr<Box>> properties;
    Error err = file->get_properties(itemId, properties);
    if (err) {
      return err;
    }

    if (propertyId - 1 >= properties.size()) {
      Error(heif_error_Usage_error, heif_suberror_Invalid_property, "property index out of range");
    }

    auto box = properties[propertyId - 1];
    auto box_casted = std::dynamic_pointer_cast<T>(box);
    if (!box_casted) {
      return Error(heif_error_Usage_error, heif_suberror_Invalid_property, "wrong property type");
    }

    return box_casted;
  }

  template<typename T>
  Result<std::shared_ptr<T>> find_property(heif_item_id itemId) {
    auto file = this->get_heif_file();
    auto result = file->get_property_for_item<T>(itemId);
    if (!result) {
      return Error(heif_error_Invalid_input,
                   heif_suberror_No_properties_assigned_to_item,
                   "property not found on item");
    }
    return result;
  }

  template<typename T>
  bool has_property(heif_item_id itemId) const
  {
    auto file = this->get_heif_file();
    auto result = file->get_property_for_item<T>(itemId);
    return result != nullptr;
  }

private:
  std::map<heif_item_id, std::shared_ptr<ImageItem>> m_all_images;

  // We store this in a vector because we need stable indices for the C API.
  // TODO: stable indices are obsolet now...
  std::vector<std::shared_ptr<ImageItem>> m_top_level_images;

  std::shared_ptr<ImageItem> m_primary_image; // shortcut to primary image

  std::shared_ptr<HeifFile> m_heif_file;

  int m_max_decoding_threads = 4;

  heif_security_limits m_limits;
  TotalMemoryTracker m_memory_tracker;

  std::vector<std::shared_ptr<RegionItem>> m_region_items;
  std::vector<std::shared_ptr<TextItem>> m_text_items;

  // --- sequences

  std::map<uint32_t, std::shared_ptr<Track>> m_tracks;
  uint32_t m_visual_track_id = 0;
  uint32_t m_sequence_repetitions = 1;

  Error interpret_heif_file();

  Error interpret_heif_file_images();

  Error interpret_heif_file_sequences();

  void remove_top_level_image(const std::shared_ptr<ImageItem>& image);
};

#endif
