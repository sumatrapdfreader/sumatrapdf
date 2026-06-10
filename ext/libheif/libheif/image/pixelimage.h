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

#include "image_description.h"
#include "security_limits.h"

#include <vector>
#include <memory>
#include <set>
#include <utility>
#include <cassert>

constexpr uint16_t heif_cmpd_component_type_UNDEFINED = 0x7FFF;


heif_chroma chroma_from_subsampling(int h, int v);

uint32_t chroma_width(uint32_t w, heif_chroma chroma);

uint32_t chroma_height(uint32_t h, heif_chroma chroma);

uint32_t channel_width(uint32_t w, heif_chroma chroma, heif_channel channel);

uint32_t channel_height(uint32_t h, heif_chroma chroma, heif_channel channel);

bool is_interleaved_with_alpha(heif_chroma chroma);

int num_interleaved_components_per_plane(heif_chroma chroma);

bool is_integer_multiple_of_chroma_size(uint32_t width,
                                        uint32_t height,
                                        heif_chroma chroma);

// Returns the list of valid heif_chroma values for a given colorspace.
std::vector<heif_chroma> get_valid_chroma_values_for_colorspace(heif_colorspace colorspace);



class HeifPixelImage : public std::enable_shared_from_this<HeifPixelImage>,
                       public ImageDescription,
                       public ErrorBuffer
{
public:
  explicit HeifPixelImage() = default;

  ~HeifPixelImage() override;

  void create(uint32_t width, uint32_t height, heif_colorspace colorspace, heif_chroma chroma);

  Error create_clone_image_at_new_size(const std::shared_ptr<const HeifPixelImage>& source, uint32_t w, uint32_t h,
                                       const heif_security_limits* limits);

  Error add_channel(heif_channel channel, uint32_t width, uint32_t height, int bit_depth,
                  const heif_security_limits* limits,
                  heif_component_datatype datatype = heif_component_datatype_unsigned_integer);

  bool has_channel(heif_channel channel) const;

  // Has alpha information either as a separate channel or in the interleaved format.
  bool has_alpha() const;

  // Get logical image size. Individual components may vary.
  uint32_t get_width() const { return m_width; }

  // Get logical image size. Individual components may vary.
  uint32_t get_height() const { return m_height; }

  uint32_t get_width(heif_channel channel) const;

  uint32_t get_height(heif_channel channel) const;

  uint32_t get_width(uint32_t component_id) const;

  uint32_t get_height(uint32_t component_id) const;

  bool has_odd_width() const { return !!(m_width & 1); }

  bool has_odd_height() const { return !!(m_height & 1); }

  // Returns true if the "primary" pixel plane(s) have exactly the given size.
  // Which plane is "primary" depends on the colorspace:
  //   YCbCr / monochrome  -> the Y plane (Cb/Cr may be legitimately subsampled)
  //   RGB planar          -> R, G and B planes must all be present and all equal
  //   RGB interleaved     -> the interleaved plane
  //   filter_array        -> the filter_array plane
  //   undefined / custom  -> not checked here; returns true
  bool primary_planes_have_size(uint32_t width, uint32_t height) const;

  heif_chroma get_chroma_format() const { return m_chroma; }

  heif_colorspace get_colorspace() const { return m_colorspace; }

  std::set<heif_channel> get_channel_set() const;

  uint16_t get_storage_bits_per_pixel(heif_channel channel) const;

  uint16_t get_bits_per_pixel(heif_channel channel) const;

  // Get the maximum bit depth of a visual channel (YCbCr or RGB).
  uint16_t get_visual_image_bits_per_pixel() const;

  heif_component_datatype get_datatype(heif_channel channel) const;

  int get_number_of_interleaved_components(heif_channel channel) const;

  // Note: we are using size_t as stride type since the stride is usually involved in a multiplication with the line number.
  //       For very large images (e.g. >2 GB), this can result in an integer overflow and corresponding illegal memory access.
  //       (see https://github.com/strukturag/libheif/issues/1419)
  uint8_t* get_channel_memory(heif_channel channel, size_t* out_stride) { return get_channel_memory<uint8_t>(channel, out_stride); }

  const uint8_t* get_channel_memory(heif_channel channel, size_t* out_stride) const { return get_channel_memory<uint8_t>(channel, out_stride); }

  template <typename T>
  T* get_channel_memory(heif_channel channel, size_t* out_stride)
  {
    auto* comp = find_storage_for_channel(channel);
    if (!comp) {
      if (out_stride)
        *out_stride = 0;

      return nullptr;
    }

    if (out_stride) {
      *out_stride = static_cast<int>(comp->stride);
    }

    return static_cast<T*>(comp->mem);
  }

  template <typename T>
  const T* get_channel_memory(heif_channel channel, size_t* out_stride) const
  {
    return const_cast<HeifPixelImage*>(this)->get_channel_memory<T>(channel, out_stride);
  }


  // --- id-based component access (for ISO 23001-17 multi-component images)

  uint32_t get_number_of_used_components() const { return static_cast<uint32_t>(get_component_descriptions().size()); }

  //uint32_t get_total_number_of_cmpd_components() const { return static_cast<uint32_t>(m_cmpd_component_types.size()); }

  heif_channel get_component_channel(uint32_t component_id) const;

  uint32_t get_component_width(uint32_t component_id) const;
  uint32_t get_component_height(uint32_t component_id) const;
  uint16_t get_component_bits_per_pixel(uint32_t component_id) const;
  uint16_t get_component_storage_bits_per_pixel(uint32_t component_id) const;
  heif_component_datatype get_component_datatype(uint32_t component_id) const;

  // Look up the component type from the cmpd table. Works for any cmpd index,
  // even those that have no image plane (e.g. bayer reference components).
  uint16_t get_component_type(uint32_t component_id) const;

  //std::vector<uint16_t> get_cmpd_component_types() const { return m_cmpd_component_types; }

  std::vector<uint32_t> get_component_ids_interleaved() const;

  //uint32_t get_component_cmpd_index() const { assert(m_cmpd_component_types.size()==1); return m_cmpd_component_types[0]; }

  // Encoder path: auto-generates component_index by appending to cmpd table.
  Result<uint32_t> add_component(uint32_t width, uint32_t height,
                                 uint16_t component_type,
                                 heif_component_datatype datatype, int bit_depth,
                                 const heif_security_limits* limits);

  // TODO: replace uint16_t component_type with class that also handled the std::string type
  uint32_t add_component_without_data(uint16_t component_type);

  // Decoder path: copy the per-component description from a source
  // ImageDescription (typically the ImageItem the decoder is about to populate).
  // After this, allocate_buffer_for_component() can be called with each id
  // already present in m_components to allocate its pixel buffer.
  void clone_component_descriptions_from(const ImageDescription& src);

  // Post-decode reconciliation: rebind this image's component descriptions
  // and the m_component_ids on each plane so they match `src` (typically the
  // ImageItem). Used after a plugin-based codec decode returns: the plugin
  // auto-minted ids via the public C API (heif_image_add_plane_safe), but
  // the handle has a canonical description list we want to expose.
  // Channels in `src` overwrite this image's matching descriptions.
  // Channels present in this image but not in `src` (e.g. alpha-from-aux
  // attached after main decode) are kept under fresh ids that won't collide
  // with `src`. No-op if `src` has no descriptions.
  void apply_descriptions_from(const ImageDescription& src);

  // Decoder path: allocate a pixel buffer for a component whose description
  // (channel, datatype, bit_depth, width, height) is already in m_components.
  // No new id is minted. Used after clone_component_descriptions_from().
  Error allocate_buffer_for_component(uint32_t component_id,
                                      const heif_security_limits* limits);

  // Decoder path: uses a pre-populated cmpd table to look up the component type.
#if 0
  Result<uint32_t> add_component_for_index(uint32_t component_index,
                                            uint32_t width, uint32_t height,
                                            heif_component_datatype datatype, int bit_depth,
                                            const heif_security_limits* limits);
#endif

  // Populate the cmpd component types table (decoder path).
  //void set_cmpd_component_types(std::vector<uint16_t> types) { m_cmpd_component_types = std::move(types); }

  //const std::vector<uint16_t>& get_cmpd_component_types() { return m_cmpd_component_types; }

  // Returns the component ids of all components, in component-description
  // order. This includes reference components that have no pixel plane
  // (has_data_plane == false); the result size always equals
  // get_number_of_used_components().
  std::vector<uint32_t> get_used_component_ids() const;

  std::vector<uint32_t> get_used_planar_component_ids() const;

  uint8_t* get_component(uint32_t component_id, size_t* out_stride);
  const uint8_t* get_component(uint32_t component_id, size_t* out_stride) const;

  template <typename T>
  T* get_component_memory(uint32_t component_id, size_t* out_stride)
  {
    auto* comp = find_storage_for_component(component_id);
    if (!comp) {
      if (out_stride) *out_stride = 0;
      return nullptr;
    }

    if (out_stride) {
      *out_stride = comp->stride;
    }
    return static_cast<T*>(comp->mem);
  }

  template <typename T>
  const T* get_component_memory(uint32_t component_id, size_t* out_stride) const
  {
    return const_cast<HeifPixelImage*>(this)->get_component_memory<T>(component_id, out_stride);
  }

  Error copy_new_channel_from(const std::shared_ptr<const HeifPixelImage>& src_image,
                            heif_channel src_channel,
                            heif_channel dst_channel,
                            const heif_security_limits* limits);

  Error extract_alpha_from_RGBA(const std::shared_ptr<const HeifPixelImage>& srcimage, const heif_security_limits* limits);

  void fill_channel(heif_channel dst_channel, uint16_t value);

  Error fill_new_channel(heif_channel dst_channel, uint16_t value, int width, int height, int bpp, const heif_security_limits* limits);

  void transfer_channel_from_image_as(const std::shared_ptr<HeifPixelImage>& source,
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


  void debug_dump() const;

  Error extend_padding_to_size(uint32_t width, uint32_t height, bool adjust_size,
                               const heif_security_limits* limits);

  Error extend_to_size_with_zero(uint32_t width, uint32_t height, const heif_security_limits* limits);

  Result<std::shared_ptr<HeifPixelImage>> extract_image_area(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                                                             const heif_security_limits* limits) const;


  // --- warnings

  void add_warning(Error warning) { m_warnings.emplace_back(std::move(warning)); }

  void add_warnings(const std::vector<Error>& warning) { for (const auto& err : warning) m_warnings.emplace_back(err); }

  const std::vector<Error>& get_warnings() const { return m_warnings; }

private:
  struct ComponentStorage
  {
    heif_channel m_channel = heif_channel_Y;

    // index into the cmpd component definition table
    // Interleaved channels will have a list of indices in the order R,G,B,A
    std::vector<uint32_t> m_component_ids;

    // limits=nullptr disables the limits
    Error alloc(uint32_t width, uint32_t height, heif_component_datatype datatype, int bit_depth,
                int num_interleaved_components,
                const heif_security_limits* limits,
                MemoryHandle& memory_handle);

    heif_component_datatype m_datatype = heif_component_datatype_unsigned_integer;

    // logical bit depth per component
    // For interleaved formats, it is the number of bits for one component.
    // It is not the storage width.
    uint16_t m_bit_depth = 0; // 1-256
    uint8_t m_num_interleaved_components = 1;

    // Cached at alloc() time so get_bytes_per_pixel() doesn't have to do
    // the bit-depth → bytes ladder on every call. Equal to
    // bytes_per_component * m_num_interleaved_components.
    uint8_t m_bytes_per_pixel = 0;

    // the "visible" area of the component
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
    void rotate_ccw(int angle_degrees, ComponentStorage& out_plane) const;

    void crop(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom, int bytes_per_pixel, ComponentStorage& out_plane) const;
  };

  ComponentStorage* find_storage_for_channel(heif_channel channel);
  const ComponentStorage* find_storage_for_channel(heif_channel channel) const;

  ComponentStorage* find_storage_for_component(uint32_t component_id);
  const ComponentStorage* find_storage_for_component(uint32_t component_id) const;

  // After plane.alloc() has succeeded, mints fresh component ids, appends
  // them to plane.m_component_ids, and pushes fully-populated
  // ComponentDescription entries for each component_type. Channel, datatype,
  // bit_depth, width and height are read from `plane`. Must only be called
  // once allocation has succeeded so that no descriptions are registered for
  // a plane that failed to materialize.
  void register_component_descriptions(ComponentStorage& plane,
                                       const std::vector<uint16_t>& component_types);

  // Overload that clones existing ComponentDescriptions (preserving
  // component_type, gimi_content_id, has_data_plane, and any other per-id
  // metadata). Geometry/datatype/bit_depth/channel fields are overwritten
  // from `plane`; component_ids are freshly minted on this image. Use this
  // when allocating a new plane that mirrors an existing one (e.g.
  // geometry-preserving transforms like rotation and crop).
  void register_component_descriptions(ComponentStorage& plane,
                                       const std::vector<const ComponentDescription*>& source_descriptions);

  uint32_t m_width = 0;
  uint32_t m_height = 0;
  heif_colorspace m_colorspace = heif_colorspace_undefined;
  heif_chroma m_chroma = heif_chroma_undefined;

  std::vector<ComponentStorage> m_storage;
  MemoryHandle m_memory_handle;

  std::vector<Error> m_warnings;
};

#endif
