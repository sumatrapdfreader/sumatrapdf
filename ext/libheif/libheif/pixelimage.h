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


#ifndef LIBHEIF_IMAGE_H
#define LIBHEIF_IMAGE_H

//#include "heif.h"
#include "error.h"
#include "nclx.h"
#include <libheif/heif_experimental.h>
#include "security_limits.h"

#include <vector>
#include <memory>
#include <map>
#include <set>
#include <utility>
#include <cassert>
#include <string>

heif_chroma chroma_from_subsampling(int h, int v);

uint32_t chroma_width(uint32_t w, heif_chroma chroma);

uint32_t chroma_height(uint32_t h, heif_chroma chroma);

uint32_t channel_width(uint32_t w, heif_chroma chroma, heif_channel channel);

uint32_t channel_height(uint32_t h, heif_chroma chroma, heif_channel channel);

bool is_interleaved_with_alpha(heif_chroma chroma);

int num_interleaved_pixels_per_plane(heif_chroma chroma);

bool is_integer_multiple_of_chroma_size(uint32_t width,
                                        uint32_t height,
                                        heif_chroma chroma);

// Returns the list of valid heif_chroma values for a given colorspace.
std::vector<heif_chroma> get_valid_chroma_values_for_colorspace(heif_colorspace colorspace);

// TODO: move to public API when used
enum heif_chroma420_sample_position {
  // values 0-5 according to ISO 23091-2 / ITU-T H.273
  heif_chroma420_sample_position_00_05 = 0,
  heif_chroma420_sample_position_05_05 = 1,
  heif_chroma420_sample_position_00_00 = 2,
  heif_chroma420_sample_position_05_00 = 3,
  heif_chroma420_sample_position_00_10 = 4,
  heif_chroma420_sample_position_05_10 = 5,

  // values 6 according to ISO 23001-17
  heif_chroma420_sample_position_00_00_01_00 = 6
};


class ImageExtraData
{
public:
  virtual ~ImageExtraData();

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

  virtual Error set_tai_timestamp(const heif_tai_timestamp_packet* tai) {
    delete m_tai_timestamp;

    m_tai_timestamp = heif_tai_timestamp_packet_alloc();
    heif_tai_timestamp_packet_copy(m_tai_timestamp, tai);
    return Error::Ok;
  }

  const heif_tai_timestamp_packet* get_tai_timestamp() const {
    return m_tai_timestamp;
  }


  virtual void set_gimi_sample_content_id(std::string id) { m_gimi_sample_content_id = id; }

  bool has_gimi_sample_content_id() const { return m_gimi_sample_content_id.has_value(); }

  std::string get_gimi_sample_content_id() const { assert(has_gimi_sample_content_id()); return *m_gimi_sample_content_id; }

private:
  bool m_premultiplied_alpha = false;
  nclx_profile m_color_profile_nclx = nclx_profile::undefined();
  std::shared_ptr<const color_profile_raw> m_color_profile_icc;

  uint32_t m_PixelAspectRatio_h = 1;
  uint32_t m_PixelAspectRatio_v = 1;
  heif_content_light_level m_clli{};
  std::optional<heif_mastering_display_colour_volume> m_mdcv;

  heif_tai_timestamp_packet* m_tai_timestamp = nullptr;

  std::optional<std::string> m_gimi_sample_content_id;

protected:
  std::shared_ptr<Box_clli> get_clli_box() const;

  std::shared_ptr<Box_mdcv> get_mdcv_box() const;

  std::shared_ptr<Box_pasp> get_pasp_box() const;

  std::shared_ptr<Box_colr> get_colr_box_nclx() const;

  std::shared_ptr<Box_colr> get_colr_box_icc() const;
};


class HeifPixelImage : public std::enable_shared_from_this<HeifPixelImage>,
                       public ImageExtraData,
                       public ErrorBuffer
{
public:
  explicit HeifPixelImage() = default;

  ~HeifPixelImage() override;

  void create(uint32_t width, uint32_t height, heif_colorspace colorspace, heif_chroma chroma);

  Error create_clone_image_at_new_size(const std::shared_ptr<const HeifPixelImage>& source, uint32_t w, uint32_t h,
                                       const heif_security_limits* limits);

  Error add_plane(heif_channel channel, uint32_t width, uint32_t height, int bit_depth, const heif_security_limits* limits);

  Error add_channel(heif_channel channel, uint32_t width, uint32_t height, heif_channel_datatype datatype, int bit_depth,
                    const heif_security_limits* limits);

  bool has_channel(heif_channel channel) const;

  // Has alpha information either as a separate channel or in the interleaved format.
  bool has_alpha() const;

  uint32_t get_width() const { return m_width; }

  uint32_t get_height() const { return m_height; }

  uint32_t get_width(heif_channel channel) const;

  uint32_t get_height(heif_channel channel) const;

  bool has_odd_width() const { return !!(m_width & 1); }

  bool has_odd_height() const { return !!(m_height & 1); }

  heif_chroma get_chroma_format() const { return m_chroma; }

  heif_colorspace get_colorspace() const { return m_colorspace; }

  std::set<heif_channel> get_channel_set() const;

  uint8_t get_storage_bits_per_pixel(heif_channel channel) const;

  uint8_t get_bits_per_pixel(heif_channel channel) const;

  // Get the maximum bit depth of a visual channel (YCbCr or RGB).
  uint8_t get_visual_image_bits_per_pixel() const;

  heif_channel_datatype get_datatype(heif_channel channel) const;

  int get_number_of_interleaved_components(heif_channel channel) const;

  // Note: we are using size_t as stride type since the stride is usually involved in a multiplication with the line number.
  //       For very large images (e.g. >2 GB), this can result in an integer overflow and corresponding illegal memory access.
  //       (see https://github.com/strukturag/libheif/issues/1419)
  uint8_t* get_plane(heif_channel channel, size_t* out_stride) { return get_channel<uint8_t>(channel, out_stride); }

  const uint8_t* get_plane(heif_channel channel, size_t* out_stride) const { return get_channel<uint8_t>(channel, out_stride); }

  template <typename T>
  T* get_channel(heif_channel channel, size_t* out_stride)
  {
    auto iter = m_planes.find(channel);
    if (iter == m_planes.end()) {
      if (out_stride)
        *out_stride = 0;

      return nullptr;
    }

    if (out_stride) {
      *out_stride = static_cast<int>(iter->second.stride / sizeof(T));
    }

    //assert(sizeof(T) == iter->second.get_bytes_per_pixel());

    return static_cast<T*>(iter->second.mem);
  }

  template <typename T>
  const T* get_channel(heif_channel channel, size_t* out_stride) const
  {
    return const_cast<HeifPixelImage*>(this)->get_channel<T>(channel, out_stride);
  }

  Error copy_new_plane_from(const std::shared_ptr<const HeifPixelImage>& src_image,
                            heif_channel src_channel,
                            heif_channel dst_channel,
                            const heif_security_limits* limits);

  Error extract_alpha_from_RGBA(const std::shared_ptr<const HeifPixelImage>& srcimage, const heif_security_limits* limits);

  void fill_plane(heif_channel dst_channel, uint16_t value);

  Error fill_new_plane(heif_channel dst_channel, uint16_t value, int width, int height, int bpp, const heif_security_limits* limits);

  void transfer_plane_from_image_as(const std::shared_ptr<HeifPixelImage>& source,
                                    heif_channel src_channel,
                                    heif_channel dst_channel);

  Error copy_image_to(const std::shared_ptr<const HeifPixelImage>& source, uint32_t x0, uint32_t y0);

  Result<std::shared_ptr<HeifPixelImage>> rotate_ccw(int angle_degrees, const heif_security_limits* limits);

  Result<std::shared_ptr<HeifPixelImage>> mirror_inplace(heif_transform_mirror_direction, const heif_security_limits* limits);

  Result<std::shared_ptr<HeifPixelImage>> crop(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom,
                                               const heif_security_limits* limits) const;

  Error fill_RGB_16bit(uint16_t r, uint16_t g, uint16_t b, uint16_t a);

  Error overlay(std::shared_ptr<HeifPixelImage>& overlay, int32_t dx, int32_t dy);

  Error scale_nearest_neighbor(std::shared_ptr<HeifPixelImage>& output, uint32_t width, uint32_t height,
                               const heif_security_limits* limits) const;

  void forward_all_metadata_from(const std::shared_ptr<const HeifPixelImage>& src_image);

  void debug_dump() const;

  Error extend_padding_to_size(uint32_t width, uint32_t height, bool adjust_size,
                               const heif_security_limits* limits);

  Error extend_to_size_with_zero(uint32_t width, uint32_t height, const heif_security_limits* limits);

  Result<std::shared_ptr<HeifPixelImage>> extract_image_area(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                                                             const heif_security_limits* limits) const;


  // --- sequences

  void set_sample_duration(uint32_t d) { m_sample_duration = d; }

  uint32_t get_sample_duration() const { return m_sample_duration; }

  // --- warnings

  void add_warning(Error warning) { m_warnings.emplace_back(std::move(warning)); }

  void add_warnings(const std::vector<Error>& warning) { for (const auto& err : warning) m_warnings.emplace_back(err); }

  const std::vector<Error>& get_warnings() const { return m_warnings; }

private:
  struct ImagePlane
  {
    // limits=nullptr disables the limits
    Error alloc(uint32_t width, uint32_t height, heif_channel_datatype datatype, int bit_depth,
                int num_interleaved_components,
                const heif_security_limits* limits,
                MemoryHandle& memory_handle);

    heif_channel_datatype m_datatype = heif_channel_datatype_unsigned_integer;
    uint8_t m_bit_depth = 0;
    uint8_t m_num_interleaved_components = 1;

    // the "visible" area of the plane
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // the allocated memory size
    uint32_t m_mem_width = 0;
    uint32_t m_mem_height = 0;

    void* mem = nullptr; // aligned memory start
    uint8_t* allocated_mem = nullptr; // unaligned memory we allocated
    size_t   allocation_size = 0;
    size_t   stride = 0; // bytes per line

    int get_bytes_per_pixel() const;

    template <typename T> void mirror_inplace(heif_transform_mirror_direction);

    template<typename T>
    void rotate_ccw(int angle_degrees, ImagePlane& out_plane) const;

    void crop(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom, int bytes_per_pixel, ImagePlane& out_plane) const;
  };

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  heif_colorspace m_colorspace = heif_colorspace_undefined;
  heif_chroma m_chroma = heif_chroma_undefined;

  std::map<heif_channel, ImagePlane> m_planes;
  MemoryHandle m_memory_handle;

  uint32_t m_sample_duration = 0; // duration of a sequence frame

  std::vector<Error> m_warnings;
};

#endif
