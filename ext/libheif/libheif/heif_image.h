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


#ifndef LIBHEIF_HEIF_IMAGE_H
#define LIBHEIF_HEIF_IMAGE_H

#include "heif.h"
#include "error.h"
#include "box.h" // only for color_profile, TODO: maybe move the color_profiles to its own header

#include <vector>
#include <memory>
#include <map>
#include <set>
#include <utility>


namespace heif {

  uint8_t chroma_h_subsampling(heif_chroma c);

  uint8_t chroma_v_subsampling(heif_chroma c);

  heif_chroma chroma_from_subsampling(int h, int v);

  void get_subsampled_size(int width, int height,
                           heif_channel channel,
                           heif_chroma chroma,
                           int* subsampled_width, int* subsampled_height);

  bool is_chroma_with_alpha(heif_chroma chroma);

  int num_interleaved_pixels_per_plane(heif_chroma chroma);

  bool is_integer_multiple_of_chroma_size(int width,
                                          int height,
                                          heif_chroma chroma);


  class HeifPixelImage : public std::enable_shared_from_this<HeifPixelImage>,
                         public ErrorBuffer
  {
  public:
    explicit HeifPixelImage() = default;

    ~HeifPixelImage();

    void create(int width, int height, heif_colorspace colorspace, heif_chroma chroma);

    bool add_plane(heif_channel channel, int width, int height, int bit_depth);

    bool has_channel(heif_channel channel) const;

    // Has alpha information either as a separate channel or in the interleaved format.
    bool has_alpha() const;

    bool is_premultiplied_alpha() const { return m_premultiplied_alpha; }

    void set_premultiplied_alpha(bool flag) { m_premultiplied_alpha = flag; }

    int get_width() const { return m_width; }

    int get_height() const { return m_height; }

    int get_width(enum heif_channel channel) const;

    int get_height(enum heif_channel channel) const;

    heif_chroma get_chroma_format() const { return m_chroma; }

    heif_colorspace get_colorspace() const { return m_colorspace; }

    std::set<enum heif_channel> get_channel_set() const;

    uint8_t get_storage_bits_per_pixel(enum heif_channel channel) const;

    uint8_t get_bits_per_pixel(enum heif_channel channel) const;

    uint8_t* get_plane(enum heif_channel channel, int* out_stride);

    const uint8_t* get_plane(enum heif_channel channel, int* out_stride) const;

    void copy_new_plane_from(const std::shared_ptr<const HeifPixelImage>& src_image,
                             heif_channel src_channel,
                             heif_channel dst_channel);

    void fill_new_plane(heif_channel dst_channel, uint16_t value, int width, int height, int bpp);

    void transfer_plane_from_image_as(const std::shared_ptr<HeifPixelImage>& source,
                                      heif_channel src_channel,
                                      heif_channel dst_channel);

    Error rotate_ccw(int angle_degrees,
                     std::shared_ptr<HeifPixelImage>& out_img);

    Error mirror_inplace(bool horizontal);

    Error crop(int left, int right, int top, int bottom,
               std::shared_ptr<HeifPixelImage>& out_img) const;

    Error fill_RGB_16bit(uint16_t r, uint16_t g, uint16_t b, uint16_t a);

    Error overlay(std::shared_ptr<HeifPixelImage>& overlay, int dx, int dy);

    Error scale_nearest_neighbor(std::shared_ptr<HeifPixelImage>& output, int width, int height) const;

    void set_color_profile_nclx(const std::shared_ptr<const color_profile_nclx>& profile) { m_color_profile_nclx = profile; }

    const std::shared_ptr<const color_profile_nclx>& get_color_profile_nclx() const { return m_color_profile_nclx; }

    void set_color_profile_icc(const std::shared_ptr<const color_profile_raw>& profile) { m_color_profile_icc = profile; }

    const std::shared_ptr<const color_profile_raw>& get_color_profile_icc() const { return m_color_profile_icc; }

    void debug_dump() const;

    bool extend_padding_to_size(int width, int height);

    void add_warning(Error warning) { m_warnings.emplace_back(std::move(warning)); }

    const std::vector<Error>& get_warnings() const { return m_warnings; }

  private:
    struct ImagePlane
    {
      bool alloc(int width, int height, int bit_depth, heif_chroma chroma);

      uint8_t m_bit_depth = 0;

      // the "visible" area of the plane
      int m_width = 0;
      int m_height = 0;

      // the allocated memory size
      int m_mem_width = 0;
      int m_mem_height = 0;

      uint8_t* mem = nullptr; // aligned memory start
      uint8_t* allocated_mem = nullptr; // unaligned memory we allocated
      uint32_t stride = 0; // bytes per line
    };

    int m_width = 0;
    int m_height = 0;
    heif_colorspace m_colorspace = heif_colorspace_undefined;
    heif_chroma m_chroma = heif_chroma_undefined;
    bool m_premultiplied_alpha = false;
    std::shared_ptr<const color_profile_nclx> m_color_profile_nclx;
    std::shared_ptr<const color_profile_raw> m_color_profile_icc;

    std::map<heif_channel, ImagePlane> m_planes;

    std::vector<Error> m_warnings;
  };

}

#endif
