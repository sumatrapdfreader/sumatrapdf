/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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

#ifndef LIBHEIF_HEIF_FILE_H
#define LIBHEIF_HEIF_FILE_H

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include "box.h"

#include <map>
#include <memory>
#include <string>
#include <map>
#include <vector>

#if ENABLE_PARALLEL_TILE_DECODING
#include <mutex>
#endif


namespace heif {

  class HeifPixelImage;

  class HeifImage;


  class HeifFile
  {
  public:
    HeifFile();

    ~HeifFile();

    Error read(const std::shared_ptr<StreamReader>& reader);

    Error read_from_file(const char* input_filename);

    Error read_from_memory(const void* data, size_t size, bool copy);

    void new_empty_file();

    void set_brand(heif_compression_format format, bool miaf_compatible);

    void write(StreamWriter& writer);

    int get_num_images() const
    { return static_cast<int>(m_infe_boxes.size()); }

    heif_item_id get_primary_image_ID() const
    { return m_pitm_box->get_item_ID(); }

    std::vector<heif_item_id> get_item_IDs() const;

    bool image_exists(heif_item_id ID) const;

    std::string get_item_type(heif_item_id ID) const;

    std::string get_content_type(heif_item_id ID) const;

    Error get_compressed_image_data(heif_item_id ID, std::vector<uint8_t>* out_data) const;


    std::shared_ptr<Box_infe> get_infe_box(heif_item_id imageID)
    {
      auto iter = m_infe_boxes.find(imageID);
      if (iter == m_infe_boxes.end()) {
        return nullptr;
      }

      return iter->second;
    }

    std::shared_ptr<Box_iref> get_iref_box()
    { return m_iref_box; }

    std::shared_ptr<Box_ipco> get_ipco_box()
    { return m_ipco_box; }

    std::shared_ptr<Box_ipma> get_ipma_box()
    { return m_ipma_box; }

    Error get_properties(heif_item_id imageID,
                         std::vector<Box_ipco::Property>& properties) const;

    heif_chroma get_image_chroma_from_configuration(heif_item_id imageID) const;

    int get_luma_bits_per_pixel_from_configuration(heif_item_id imageID) const;

    int get_chroma_bits_per_pixel_from_configuration(heif_item_id imageID) const;

    std::string debug_dump_boxes() const;


    // --- writing ---

    heif_item_id get_unused_item_id() const;

    heif_item_id add_new_image(const char* item_type);

    std::shared_ptr<Box_infe> add_new_infe_box(const char* item_type);

    void add_hvcC_property(heif_item_id id);

    Error append_hvcC_nal_data(heif_item_id id, const std::vector<uint8_t>& data);

    Error append_hvcC_nal_data(heif_item_id id, const uint8_t* data, size_t size);

    Error set_hvcC_configuration(heif_item_id id, const Box_hvcC::configuration& config);

    void add_av1C_property(heif_item_id id);

    Error set_av1C_configuration(heif_item_id id, const Box_av1C::configuration& config);

    void add_ispe_property(heif_item_id id, uint32_t width, uint32_t height);

    void add_clap_property(heif_item_id id, uint32_t clap_width, uint32_t clap_height,
                           uint32_t image_width, uint32_t image_height);

    void add_pixi_property(heif_item_id id, uint8_t c1, uint8_t c2=0, uint8_t c3=0);

    void append_iloc_data(heif_item_id id, const std::vector<uint8_t>& nal_packets, uint8_t construction_method = 0);

    void append_iloc_data_with_4byte_size(heif_item_id id, const uint8_t* data, size_t size);

    void set_primary_item_id(heif_item_id id);

    void add_iref_reference(heif_item_id from, uint32_t type,
                            const std::vector<heif_item_id>& to);

    void set_auxC_property(heif_item_id id, const std::string& type);

    void set_color_profile(heif_item_id id, const std::shared_ptr<const color_profile>& profile);

    // TODO: the hdlr box is probably not the right place for this. Into which box should we write comments?
    void set_hdlr_library_info(const std::string& encoder_plugin_version);

#if defined(__MINGW32__) || defined(__MINGW64__) || defined(_MSC_VER)
    static std::wstring convert_utf8_path_to_utf16(std::string pathutf8);
#endif

  private:
#if ENABLE_PARALLEL_TILE_DECODING
    mutable std::mutex m_read_mutex;
#endif

    std::shared_ptr<StreamReader> m_input_stream;

    std::vector<std::shared_ptr<Box> > m_top_level_boxes;

    std::shared_ptr<Box_ftyp> m_ftyp_box;
    std::shared_ptr<Box_hdlr> m_hdlr_box;
    std::shared_ptr<Box_meta> m_meta_box;

    std::shared_ptr<Box_ipco> m_ipco_box;
    std::shared_ptr<Box_ipma> m_ipma_box;
    std::shared_ptr<Box_iloc> m_iloc_box;
    std::shared_ptr<Box_idat> m_idat_box;
    std::shared_ptr<Box_iref> m_iref_box;
    std::shared_ptr<Box_pitm> m_pitm_box;
    std::shared_ptr<Box_iinf> m_iinf_box;

    std::shared_ptr<Box_iprp> m_iprp_box;

    std::map<heif_item_id, std::shared_ptr<Box_infe> > m_infe_boxes;

    // list of image items (does not include hidden images or Exif data)
    //std::vector<heif_item_id> m_valid_image_IDs;


    Error parse_heif_file(BitstreamRange& bitstream);

    std::shared_ptr<Box_infe> get_infe(heif_item_id ID) const;
  };

}

#endif
