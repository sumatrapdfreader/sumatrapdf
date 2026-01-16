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

#ifndef LIBHEIF_FILE_H
#define LIBHEIF_FILE_H

#include "box.h"
#include "nclx.h"
#include "image-items/avif.h"
#include "image-items/hevc.h"
#include "image-items/vvc.h"
//#include "codecs/uncompressed/unc_boxes.h"
#include "file_layout.h"

#include <map>
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <limits>
#include <utility>
#include "mdat_data.h"

#if ENABLE_PARALLEL_TILE_DECODING

#include <mutex>

#endif


class HeifPixelImage;

class Box_j2kH;

class Box_moov;

class Box_mvhd;



class HeifFile
{
public:
  HeifFile();

  ~HeifFile();

  // The limits will be shared from the HeifContext limits.
  // You have to make sure that the pointer points to a valid object as long as the HeifFile is used.
  void set_security_limits(const heif_security_limits* limits) { m_limits = limits; }

  Error read(const std::shared_ptr<StreamReader>& reader);

  Error read_from_file(const char* input_filename);

  Error read_from_memory(const void* data, size_t size, bool copy);

  bool has_images() const { return m_meta_box != nullptr; }

  bool has_sequences() const { return m_moov_box != nullptr; }

  std::shared_ptr<StreamReader> get_reader() { return m_input_stream; }

  void new_empty_file();

  void init_for_image();

  void init_for_sequence();

  void set_hdlr_box(std::shared_ptr<Box_hdlr> box) { m_hdlr_box = std::move(box); }

  size_t append_mdat_data(const std::vector<uint8_t>& data);

  void derive_box_versions();

  void write(StreamWriter& writer);

  int get_num_images() const { return static_cast<int>(m_infe_boxes.size()); }

  heif_item_id get_primary_image_ID() const { return m_pitm_box->get_item_ID(); }

  size_t get_number_of_items() const { return m_infe_boxes.size(); }

  std::vector<heif_item_id> get_item_IDs() const;

  bool item_exists(heif_item_id ID) const;

  bool has_item_with_id(heif_item_id ID) const;

  uint32_t get_item_type_4cc(heif_item_id ID) const;

  std::string get_content_type(heif_item_id ID) const;

  std::string get_item_uri_type(heif_item_id ID) const;

  Result<std::vector<uint8_t>> get_uncompressed_item_data(heif_item_id ID) const;

  Error append_data_from_file_range(std::vector<uint8_t>& out_data, uint64_t offset, uint32_t size) const;

  Error append_data_from_iloc(heif_item_id ID, std::vector<uint8_t>& out_data, uint64_t offset, uint64_t size) const;

  Error append_data_from_iloc(heif_item_id ID, std::vector<uint8_t>& out_data) const {
    return append_data_from_iloc(ID, out_data, 0, std::numeric_limits<uint64_t>::max());
  }

  // If `out_compression` is not NULL, the compression method is returned there and the compressed data is returned.
  // If `out_compression` is NULL, the data is returned decompressed.
  Result<std::vector<uint8_t>> get_item_data(heif_item_id ID, heif_metadata_compression* out_compression) const;

  std::shared_ptr<Box_ftyp> get_ftyp_box() { return m_ftyp_box; }

  void init_meta_box() { m_meta_box = std::make_shared<Box_meta>(); }

  std::shared_ptr<const Box_infe> get_infe_box(heif_item_id imageID) const;

  std::shared_ptr<Box_infe> get_infe_box(heif_item_id imageID);

  void set_iref_box(std::shared_ptr<Box_iref>);

  std::shared_ptr<Box_iref> get_iref_box() { return m_iref_box; }

  std::shared_ptr<const Box_iref> get_iref_box() const { return m_iref_box; }

  std::shared_ptr<Box_ipco> get_ipco_box() { return m_ipco_box; }

  void set_ipco_box(std::shared_ptr<Box_ipco>);

  std::shared_ptr<Box_ipco> get_ipco_box() const { return m_ipco_box; }

  void set_ipma_box(std::shared_ptr<Box_ipma>);

  std::shared_ptr<Box_ipma> get_ipma_box() { return m_ipma_box; }

  std::shared_ptr<Box_ipma> get_ipma_box() const { return m_ipma_box; }

  std::shared_ptr<Box_grpl> get_grpl_box() const { return m_grpl_box; }

  std::shared_ptr<Box_meta> get_meta_box() const { return m_meta_box; }

  std::shared_ptr<Box_EntityToGroup> get_entity_group(heif_entity_group_id id);

  Error get_properties(heif_item_id imageID,
                       std::vector<std::shared_ptr<Box>>& properties) const;

  template<class BoxType>
  std::shared_ptr<BoxType> get_property_for_item(heif_item_id imageID) const
  {
    std::vector<std::shared_ptr<Box>> properties;
    Error err = get_properties(imageID, properties);
    if (err) {
      return nullptr;
    }

    for (auto& property : properties) {
      if (auto box = std::dynamic_pointer_cast<BoxType>(property)) {
        return box;
      }
    }

    return nullptr;
  }

  std::string debug_dump_boxes() const;


  // --- writing ---

  heif_item_id get_unused_item_id() const;

  heif_item_id add_new_image(uint32_t item_type);

  std::shared_ptr<Box_infe> add_new_infe_box(uint32_t item_type);

  void add_ispe_property(heif_item_id id, uint32_t width, uint32_t height, bool essential);

  // set irot/imir according to heif_orientation
  void add_orientation_properties(heif_item_id id, heif_orientation);

  // TODO: can we remove the 'essential' parameter and take this from the box? Or is that depending on the context?
  heif_property_id add_property(heif_item_id id, const std::shared_ptr<Box>& property, bool essential);

  heif_property_id add_property_without_deduplication(heif_item_id id, const std::shared_ptr<Box>& property, bool essential);

  Result<heif_item_id> add_infe(uint32_t item_type, const uint8_t* data, size_t size);

  void add_infe_box(heif_item_id, std::shared_ptr<Box_infe> infe);

  Result<heif_item_id> add_infe_mime(const char* content_type, heif_metadata_compression content_encoding, const uint8_t* data, size_t size);

  Result<heif_item_id> add_precompressed_infe_mime(const char* content_type, std::string content_encoding, const uint8_t* data, size_t size);

  Result<heif_item_id> add_infe_uri(const char* item_uri_type, const uint8_t* data, size_t size);

  Error set_item_data(const std::shared_ptr<Box_infe>& item, const uint8_t* data, size_t size, heif_metadata_compression compression);

  Error set_precompressed_item_data(const std::shared_ptr<Box_infe>& item, const uint8_t* data, size_t size, std::string content_encoding);

  void append_iloc_data(heif_item_id id, const std::vector<uint8_t>& nal_packets, uint8_t construction_method);

  void replace_iloc_data(heif_item_id id, uint64_t offset, const std::vector<uint8_t>& data, uint8_t construction_method = 0);

  void set_iloc_box(std::shared_ptr<Box_iloc>);

  std::shared_ptr<Box_iloc> get_iloc_box() { return m_iloc_box; }

  void set_primary_item_id(heif_item_id id);

  void add_iref_reference(heif_item_id from, uint32_t type,
                          const std::vector<heif_item_id>& to);

  void set_iref_reference(heif_item_id from, uint32_t type, int reference_idx, heif_item_id to_item);

  void add_entity_group_box(const std::shared_ptr<Box>& entity_group_box);

  void set_auxC_property(heif_item_id id, const std::string& type);

#if defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)
  static std::wstring convert_utf8_path_to_utf16(std::string pathutf8);
#endif


  // --- sequences

  std::shared_ptr<Box_moov> get_moov_box() { return m_moov_box; }

  std::shared_ptr<Box_mvhd> get_mvhd_box() { return m_mvhd_box; }

private:
#if ENABLE_PARALLEL_TILE_DECODING
  mutable std::mutex m_read_mutex;
#endif

  std::shared_ptr<FileLayout> m_file_layout;

  std::shared_ptr<StreamReader> m_input_stream;

  std::vector<std::shared_ptr<Box> > m_top_level_boxes;

  std::shared_ptr<Box_ftyp> m_ftyp_box;
  std::shared_ptr<Box_hdlr> m_hdlr_box;
  std::shared_ptr<Box_meta> m_meta_box;
#if ENABLE_EXPERIMENTAL_MINI_FORMAT
  std::shared_ptr<Box_mini> m_mini_box; // meta alternative
#endif

  std::shared_ptr<Box_ipco> m_ipco_box;
  std::shared_ptr<Box_ipma> m_ipma_box;
  std::shared_ptr<Box_iloc> m_iloc_box;
  std::shared_ptr<Box_idat> m_idat_box;
  std::shared_ptr<Box_iref> m_iref_box;
  std::shared_ptr<Box_pitm> m_pitm_box;
  std::shared_ptr<Box_iinf> m_iinf_box;
  std::shared_ptr<Box_grpl> m_grpl_box;

  std::shared_ptr<Box_iprp> m_iprp_box;

  std::map<heif_item_id, std::shared_ptr<Box_infe> > m_infe_boxes;

  std::unique_ptr<MdatData> m_mdat_data;

  // returns the position of the first data byte in the file
  Result<size_t> write_mdat(StreamWriter& writer);

  // --- sequences

  std::shared_ptr<Box_moov> m_moov_box;
  std::shared_ptr<Box_mvhd> m_mvhd_box;

  const heif_security_limits* m_limits = nullptr;

  Error parse_heif_file();

  Error parse_heif_images();

  Error parse_heif_sequences();

  Error check_for_ref_cycle(heif_item_id ID,
                            const std::shared_ptr<Box_iref>& iref_box) const;

  Error check_for_ref_cycle_recursion(heif_item_id ID,
                                      const std::shared_ptr<Box_iref>& iref_box,
                                      std::unordered_set<heif_item_id>& parent_items) const;
};

#endif
