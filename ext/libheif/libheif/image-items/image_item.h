/*
 * HEIF image base codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_IMAGEITEM_H
#define LIBHEIF_IMAGEITEM_H

#include "api/libheif/heif.h"
#include "error.h"
#include "nclx.h"
#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <set>

#include "pixelimage.h"
#include "api/libheif/heif_plugin.h"
#include "codecs/encoder.h"


class HeifContext;

class HeifPixelImage;


class ImageMetadata
{
public:
  heif_item_id item_id;
  std::string item_type;  // e.g. "Exif"
  std::string content_type;
  std::string item_uri_type;
  std::vector<uint8_t> m_data;
};


class ImageItem : public ImageExtraData,
                  public ErrorBuffer
{
public:
  ImageItem(HeifContext* ctx);

  ImageItem(HeifContext* ctx, heif_item_id id);

  static std::shared_ptr<ImageItem> alloc_for_infe_box(HeifContext*, const std::shared_ptr<Box_infe>&);

  static std::shared_ptr<ImageItem> alloc_for_compression_format(HeifContext*, heif_compression_format);

  static heif_compression_format compression_format_from_fourcc_infe_type(uint32_t type);

  static uint32_t compression_format_to_fourcc_infe_type(heif_compression_format);

  virtual uint32_t get_infe_type() const { return 0; }

  virtual const char* get_auxC_alpha_channel_type() const { return "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha"; }

  virtual bool is_ispe_essential() const { return false; }

  bool is_property_essential(const std::shared_ptr<Box>& property) const;

  virtual Error get_item_error() const { return Error::Ok; }

  virtual heif_compression_format get_compression_format() const { return heif_compression_undefined; }

  virtual Result<std::vector<uint8_t>> read_bitstream_configuration_data() const { return std::vector<uint8_t>{}; }

  void clear()
  {
    m_thumbnails.clear();
    m_alpha_channel.reset();
    m_depth_channel.reset();
    m_aux_images.clear();
  }

  HeifContext* get_context() { return m_heif_context; }

  const HeifContext* get_context() const { return m_heif_context; }

  std::shared_ptr<class HeifFile> get_file() const;

  void set_properties(std::vector<std::shared_ptr<Box>> properties) {
    m_properties = std::move(properties);
  }

  template<class BoxType>
  std::shared_ptr<BoxType> get_property() const
  {
    for (auto& property : m_properties) {
      if (auto box = std::dynamic_pointer_cast<BoxType>(property)) {
        return box;
      }
    }

    return nullptr;
  }

  heif_property_id add_property(std::shared_ptr<Box> property, bool essential);

  heif_property_id add_property_without_deduplication(std::shared_ptr<Box> property, bool essential);

  void set_resolution(uint32_t w, uint32_t h)
  {
    m_width = w;
    m_height = h;
  }

  heif_item_id get_id() const { return m_id; }

  void set_id(heif_item_id id) { m_id = id; }

  void set_primary(bool flag = true) { m_is_primary = flag; }

  bool is_primary() const { return m_is_primary; }

  // 32bit limitation from `ispe`
  uint32_t get_width() const { return m_width; }

  uint32_t get_height() const { return m_height; }

  bool has_ispe_resolution() const;

  uint32_t get_ispe_width() const;

  uint32_t get_ispe_height() const;

  // Default behavior: forward call to Decoder
  [[nodiscard]] virtual int get_luma_bits_per_pixel() const;

  // Default behavior: forward call to Decoder
  [[nodiscard]] virtual int get_chroma_bits_per_pixel() const;

  void set_size(uint32_t w, uint32_t h)
  {
    m_width = w;
    m_height = h;
  }

  virtual void get_tile_size(uint32_t& w, uint32_t& h) const;

  virtual Error get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const;

  Error postprocess_coded_image_colorspace(heif_colorspace* inout_colorspace, heif_chroma* inout_chroma) const;

  virtual void process_before_write() { }

  // -- thumbnails

  void set_is_thumbnail()
  {
    m_is_thumbnail = true;
  }

  void add_thumbnail(const std::shared_ptr<ImageItem>& img) { m_thumbnails.push_back(img); }

  bool is_thumbnail() const { return m_is_thumbnail; }

  const std::vector<std::shared_ptr<ImageItem>>& get_thumbnails() const { return m_thumbnails; }


  // --- alpha channel

  void set_is_alpha_channel()
  {
    m_is_alpha_channel = true;
  }

  void set_alpha_channel(std::shared_ptr<ImageItem> img) { m_alpha_channel = std::move(img); }

  bool is_alpha_channel() const { return m_is_alpha_channel; }

  const std::shared_ptr<ImageItem>& get_alpha_channel() const { return m_alpha_channel; }

  void set_is_premultiplied_alpha(bool flag) { m_premultiplied_alpha = flag; }

  bool is_premultiplied_alpha() const { return m_premultiplied_alpha; }

  // Whether the image has an alpha channel coded in the main image (not as an auxiliary image)
  virtual bool has_coded_alpha_channel() const { return false; }

  // --- depth channel

  void set_is_depth_channel()
  {
    m_is_depth_channel = true;
  }

  void set_depth_channel(std::shared_ptr<ImageItem> img) { m_depth_channel = std::move(img); }

  bool is_depth_channel() const { return m_is_depth_channel; }

  const std::shared_ptr<ImageItem>& get_depth_channel() const { return m_depth_channel; }


  void set_depth_representation_info(heif_depth_representation_info& info)
  {
    m_has_depth_representation_info = true;
    m_depth_representation_info = info;
  }

  bool has_depth_representation_info() const
  {
    return m_has_depth_representation_info;
  }

  const heif_depth_representation_info& get_depth_representation_info() const
  {
    return m_depth_representation_info;
  }


  // --- generic aux image

  void set_is_aux_image(const std::string& aux_type)
  {
    m_is_aux_image = true;
    m_aux_image_type = aux_type;
  }

  void add_aux_image(std::shared_ptr<ImageItem> img) { m_aux_images.push_back(std::move(img)); }

  bool is_aux_image() const { return m_is_aux_image; }

  const std::string& get_aux_type() const { return m_aux_image_type; }

  std::vector<std::shared_ptr<ImageItem>> get_aux_images(int aux_image_filter = 0) const
  {
    if (aux_image_filter == 0) {
      return m_aux_images;
    }
    else {
      std::vector<std::shared_ptr<ImageItem>> auxImgs;
      for (const auto& aux : m_aux_images) {
        if ((aux_image_filter & LIBHEIF_AUX_IMAGE_FILTER_OMIT_ALPHA) && aux->is_alpha_channel()) {
          continue;
        }

        if ((aux_image_filter & LIBHEIF_AUX_IMAGE_FILTER_OMIT_DEPTH) &&
            aux->is_depth_channel()) {
          continue;
        }

        auxImgs.push_back(aux);
      }

      return auxImgs;
    }
  }


  // --- metadata

  void add_metadata(std::shared_ptr<ImageMetadata> metadata)
  {
    m_metadata.push_back(std::move(metadata));
  }

  const std::vector<std::shared_ptr<ImageMetadata>>& get_metadata() const { return m_metadata; }



  // --- ImageExtraData

  void set_clli(const heif_content_light_level& clli) override;

  void set_mdcv(const heif_mastering_display_colour_volume& mdcv) override;

  void set_pixel_ratio(uint32_t h, uint32_t v) override;

  void set_color_profile_nclx(const nclx_profile& profile) override;

  void set_color_profile_icc(const std::shared_ptr<const color_profile_raw>& profile) override;

  // --- miaf

  // TODO: we should have a function that checks all MIAF constraints and sets the compatibility flag.
  void mark_not_miaf_compatible() { m_miaf_compatible = false; }

  bool is_miaf_compatible() const { return m_miaf_compatible; }

  // return 0 if we don't know the brand
  virtual heif_brand2 get_compatible_brand() const { return 0; }

  // === decoding ===

  virtual Error initialize_decoder() { return Error::Ok; }

  virtual void set_decoder_input_data() { }

  virtual Result<std::shared_ptr<HeifPixelImage>> decode_image(const heif_decoding_options& options,
                                                               bool decode_tile_only, uint32_t tile_x0,
                                                               uint32_t tile_y0,
                                                               std::set<heif_item_id> processed_ids) const;

  virtual Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                          bool decode_tile_only, uint32_t tile_x0,
                                                                          uint32_t tile_y0,
                                                                          std::set<heif_item_id> processed_ids) const;

  Result<std::vector<std::shared_ptr<Box>>> get_properties() const;

  bool has_essential_property_other_than(const std::set<uint32_t>&) const;

  // === encoding ===

  Result<Encoder::CodedImageData> encode_to_bitstream_and_boxes(const std::shared_ptr<HeifPixelImage>& image,
                                                                heif_encoder* encoder,
                                                                const heif_encoding_options& options,
                                                                heif_image_input_class input_class);

  // Entry point for encoding pixel images.
  Error encode_to_item(HeifContext* ctx,
                       const std::shared_ptr<HeifPixelImage>& image,
                       heif_encoder* encoder,
                       const heif_encoding_options& options,
                       heif_image_input_class input_class);

  void set_intrinsic_matrix(const Box_cmin::RelativeIntrinsicMatrix& cmin) {
    m_has_intrinsic_matrix = true;
    m_intrinsic_matrix = cmin.to_absolute(get_ispe_width(), get_ispe_height());
  }

  bool has_intrinsic_matrix() const { return m_has_intrinsic_matrix; }

  Box_cmin::AbsoluteIntrinsicMatrix& get_intrinsic_matrix() { return m_intrinsic_matrix; }

  const Box_cmin::AbsoluteIntrinsicMatrix& get_intrinsic_matrix() const { return m_intrinsic_matrix; }


  void set_extrinsic_matrix(const Box_cmex::ExtrinsicMatrix& cmex) {
    m_has_extrinsic_matrix = true;
    m_extrinsic_matrix = cmex;
  }

  bool has_extrinsic_matrix() const { return m_has_extrinsic_matrix; }

  Box_cmex::ExtrinsicMatrix& get_extrinsic_matrix() { return m_extrinsic_matrix; }

  const Box_cmex::ExtrinsicMatrix& get_extrinsic_matrix() const { return m_extrinsic_matrix; }


  void add_region_item_id(heif_item_id id) { m_region_item_ids.push_back(id); }

  const std::vector<heif_item_id>& get_region_item_ids() const { return m_region_item_ids; }


  void add_decoding_warning(Error err) { m_decoding_warnings.emplace_back(std::move(err)); }

  const std::vector<Error>& get_decoding_warnings() const { return m_decoding_warnings; }

  virtual heif_image_tiling get_heif_image_tiling() const;

  Error process_image_transformations_on_tiling(heif_image_tiling&) const;

  Error transform_requested_tile_position_to_original_tile_position(uint32_t& tile_x, uint32_t& tile_y) const;

  virtual Result<std::shared_ptr<class Decoder>> get_decoder() const
  {
    return Error{
      heif_error_Unsupported_feature,
      heif_suberror_No_matching_decoder_installed,
      "No decoder for this image format"
    };
  }

  virtual std::shared_ptr<class Encoder> get_encoder() const { return nullptr; }

  void add_text_item_id(heif_item_id id) {
    m_text_item_ids.push_back(id);
  }

  const std::vector<heif_item_id>& get_text_item_ids() const { return m_text_item_ids; }

private:
  HeifContext* m_heif_context;
  std::vector<std::shared_ptr<Box>> m_properties;

  heif_item_id m_id = 0;
  uint32_t m_width = 0, m_height = 0;  // after all transformations have been applied
  bool m_is_primary = false;

  bool m_is_thumbnail = false;

  std::vector<std::shared_ptr<ImageItem>> m_thumbnails;

  bool m_is_alpha_channel = false;
  bool m_premultiplied_alpha = false;
  std::shared_ptr<ImageItem> m_alpha_channel;

  bool m_is_depth_channel = false;
  std::shared_ptr<ImageItem> m_depth_channel;

  bool m_has_depth_representation_info = false;
  heif_depth_representation_info m_depth_representation_info;

  bool m_is_aux_image = false;
  std::string m_aux_image_type;
  std::vector<std::shared_ptr<ImageItem>> m_aux_images;

  std::vector<std::shared_ptr<ImageMetadata>> m_metadata;

  bool m_miaf_compatible = true;

  std::vector<heif_item_id> m_region_item_ids;

  bool m_has_intrinsic_matrix = false;
  Box_cmin::AbsoluteIntrinsicMatrix m_intrinsic_matrix{};

  bool m_has_extrinsic_matrix = false;
  Box_cmex::ExtrinsicMatrix m_extrinsic_matrix{};

  std::vector<Error> m_decoding_warnings;

  std::vector<heif_item_id> m_text_item_ids;

  void generate_property_boxes_for_ImageExtraData();

protected:
  // Result<std::vector<uint8_t>> read_bitstream_configuration_data_override(heif_item_id itemId, heif_compression_format format) const;

  virtual Result<Encoder::CodedImageData> encode(const std::shared_ptr<HeifPixelImage>& image,
                                                 heif_encoder* encoder,
                                                 const heif_encoding_options& options,
                                                 heif_image_input_class input_class);

  // --- encoding utility functions

  static std::vector<std::shared_ptr<Box_colr> >
  add_color_profile(const std::shared_ptr<HeifPixelImage>& image,
                    const heif_encoding_options& options,
                    heif_image_input_class input_class,
                    const heif_color_profile_nclx* target_heif_nclx);
};


class ImageItem_Error : public ImageItem
{
public:
  // dummy ImageItem class that is a placeholder for unsupported item types

  ImageItem_Error(uint32_t item_type, heif_item_id id, Error err)
    : ImageItem(nullptr, id), m_item_type(item_type), m_item_error(err) {}

  uint32_t get_infe_type() const override
  {
    return m_item_type;
  }

  Error get_item_error() const override { return m_item_error; }

  Result<std::shared_ptr<HeifPixelImage>> decode_image(const heif_decoding_options& options,
                                                       bool decode_tile_only, uint32_t tile_x0,
                                                       uint32_t tile_y0,
                                                       std::set<heif_item_id> processed_ids) const override
  {
    return m_item_error;
  }

  Result<std::shared_ptr<HeifPixelImage>> decode_compressed_image(const heif_decoding_options& options,
                                                                  bool decode_tile_only, uint32_t tile_x0,
                                                                  uint32_t tile_y0, std::set<heif_item_id> processed_ids) const override
  {
    return m_item_error;
  }

  [[nodiscard]] int get_luma_bits_per_pixel() const override { return -1; }

  [[nodiscard]] int get_chroma_bits_per_pixel() const override { return -1; }

private:
  uint32_t m_item_type;
  Error m_item_error;
};

#endif //LIBHEIF_IMAGEITEM_H
