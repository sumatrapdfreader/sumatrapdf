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


#include "pixelimage.h"
#include "common_utils.h"
#include "security_limits.h"

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <limits>
#include <algorithm>
#include <map>
#include <color-conversion/colorconversion.h>

#include "codecs/uncompressed/unc_types.h"


heif_chroma chroma_from_subsampling(int h, int v)
{
  if (h == 2 && v == 2) {
    return heif_chroma_420;
  }
  else if (h == 2 && v == 1) {
    return heif_chroma_422;
  }
  else if (h == 1 && v == 1) {
    return heif_chroma_444;
  }
  else {
    assert(false);
    return heif_chroma_undefined;
  }
}


uint32_t chroma_width(uint32_t w, heif_chroma chroma)
{
  switch (chroma) {
    case heif_chroma_420:
    case heif_chroma_422:
      return w/2 + (w & 1); // note: prevents integer overflow
    default:
      return w;
  }
}

uint32_t chroma_height(uint32_t h, heif_chroma chroma)
{
  switch (chroma) {
    case heif_chroma_420:
      return h/2 + (h & 1); // note: prevents integer overflow
    default:
      return h;
  }
}

uint32_t channel_width(uint32_t w, heif_chroma chroma, heif_channel channel)
{
  if (channel == heif_channel_Cb || channel == heif_channel_Cr) {
    return chroma_width(w, chroma);
  }
  else {
    return w;
  }
}

uint32_t channel_height(uint32_t h, heif_chroma chroma, heif_channel channel)
{
  if (channel == heif_channel_Cb || channel == heif_channel_Cr) {
    return chroma_height(h, chroma);
  }
  else {
    return h;
  }
}



static std::vector<uint16_t> map_channel_to_component_type(heif_channel channel, heif_chroma chroma)
{
  switch (channel) {
    case heif_channel_Y:
      return {heif_cmpd_component_type_Y};
    case heif_channel_Cb:
      return {heif_cmpd_component_type_Cb};
    case heif_channel_Cr:
      return {heif_cmpd_component_type_Cr};
    case heif_channel_R:
      return {heif_cmpd_component_type_red};
    case heif_channel_G:
      return {heif_cmpd_component_type_green};
    case heif_channel_B:
      return {heif_cmpd_component_type_blue};
    case heif_channel_Alpha:
      return {heif_cmpd_component_type_alpha};
    case heif_channel_filter_array:
      return {heif_cmpd_component_type_filter_array};
    case heif_channel_depth:
      return {heif_cmpd_component_type_depth};
    case heif_channel_disparity:
      return {heif_cmpd_component_type_disparity};
    case heif_channel_interleaved:
      switch (chroma) {
        case heif_chroma_interleaved_RGB:
        case heif_chroma_interleaved_RRGGBB_BE:
        case heif_chroma_interleaved_RRGGBB_LE:
          return {
            heif_cmpd_component_type_red,
            heif_cmpd_component_type_green,
            heif_cmpd_component_type_blue
          };
        case heif_chroma_interleaved_RGBA:
        case heif_chroma_interleaved_RRGGBBAA_BE:
        case heif_chroma_interleaved_RRGGBBAA_LE:
          return {
            heif_cmpd_component_type_red,
            heif_cmpd_component_type_green,
            heif_cmpd_component_type_blue,
            heif_cmpd_component_type_alpha
          };
        default:
          assert(false);
          return {static_cast<uint16_t>(1000 + channel)};
          break;
      }
    default:
      // For other channels without a direct match,
      // use an internal custom value.
      return {static_cast<uint16_t>(1000 + channel)};
  }
}





HeifPixelImage::~HeifPixelImage()
{
  for (auto& component : m_storage) {
    std::free(component.allocated_mem);
  }
}


HeifPixelImage::ComponentStorage* HeifPixelImage::find_storage_for_channel(heif_channel channel)
{
  for (auto& component : m_storage) {
    if (component.m_channel == channel) {
      return &component;
    }
  }
  return nullptr;
}

const HeifPixelImage::ComponentStorage* HeifPixelImage::find_storage_for_channel(heif_channel channel) const
{
  for (const auto& component : m_storage) {
    if (component.m_channel == channel) {
      return &component;
    }
  }
  return nullptr;
}


int num_interleaved_components_per_plane(heif_chroma chroma)
{
  switch (chroma) {
    case heif_chroma_undefined:
    case heif_chroma_monochrome:
    case heif_chroma_420:
    case heif_chroma_422:
    case heif_chroma_444:
      return 1;

    case heif_chroma_interleaved_RGB:
    case heif_chroma_interleaved_RRGGBB_BE:
    case heif_chroma_interleaved_RRGGBB_LE:
      return 3;

    case heif_chroma_interleaved_RGBA:
    case heif_chroma_interleaved_RRGGBBAA_BE:
    case heif_chroma_interleaved_RRGGBBAA_LE:
      return 4;
  }

  assert(false);
  return 0;
}


bool is_integer_multiple_of_chroma_size(uint32_t width,
                                        uint32_t height,
                                        heif_chroma chroma)
{
  switch (chroma) {
    case heif_chroma_444:
    case heif_chroma_monochrome:
      return true;
    case heif_chroma_422:
      return (width & 1) == 0;
    case heif_chroma_420:
      return (width & 1) == 0 && (height & 1) == 0;
    default:
      assert(false);
      return false;
  }
}


std::vector<heif_chroma> get_valid_chroma_values_for_colorspace(heif_colorspace colorspace)
{
  switch (colorspace) {
    case heif_colorspace_YCbCr:
      return {heif_chroma_420, heif_chroma_422, heif_chroma_444};

    case heif_colorspace_RGB:
      // heif_chroma_planar and heif_chroma_444 are synonyms here.
      // HeifPixelImage::create() canonicalizes the stored value to
      // heif_chroma_444, so internal state and read-back always see 444;
      // listing both here signals to callers that either is accepted, and
      // prepares for a possible future switch of the canonical name.
      return {heif_chroma_444,
              heif_chroma_planar,
              heif_chroma_interleaved_RGB,
              heif_chroma_interleaved_RGBA,
              heif_chroma_interleaved_RRGGBB_BE,
              heif_chroma_interleaved_RRGGBBAA_BE,
              heif_chroma_interleaved_RRGGBB_LE,
              heif_chroma_interleaved_RRGGBBAA_LE};

    case heif_colorspace_monochrome:
      return {heif_chroma_planar};

    case heif_colorspace_custom:
      // Custom-colorspace images may have any number of planar components
      // with arbitrary semantics. heif_chroma_planar describes the layout
      // (planar, no subsampling); the per-component semantics live in the
      // component descriptions.
      return {heif_chroma_planar};

    case heif_colorspace_filter_array:
      // Filter-array (CFA / Bayer) images are a single mosaicked plane.
      // The spatial pattern encodes color, so the legacy "monochrome" label
      // is misleading; heif_chroma_planar describes only the layout.
      return {heif_chroma_planar};

    default:
      return {};
  }
}


void HeifPixelImage::create(uint32_t width, uint32_t height, heif_colorspace colorspace, heif_chroma chroma)
{
  // Canonicalize (RGB, planar) to (RGB, 444). heif_chroma_planar is accepted
  // as a synonym at this layer too (not just at heif_image_create), so any
  // internal caller passing it gets the canonical form stored.
  if (colorspace == heif_colorspace_RGB && chroma == heif_chroma_planar) {
    chroma = heif_chroma_444;
  }

  m_width = width;
  m_height = height;
  m_colorspace = colorspace;
  m_chroma = chroma;
}

static uint32_t rounded_size(uint32_t s)
{
  s = (s + 1U) & ~1U;

  if (s < 64) {
    s = 64;
  }

  return s;
}

void HeifPixelImage::register_component_descriptions(ComponentStorage& plane,
                                                 const std::vector<uint16_t>& component_types)
{
  for (uint16_t type : component_types) {
    uint32_t id = mint_component_id();
    plane.m_component_ids.push_back(id);

    ComponentDescription desc;
    desc.component_id = id;
    desc.channel = plane.m_channel;
    desc.component_type = type;
    desc.datatype = plane.m_datatype;
    desc.bit_depth = plane.m_bit_depth;
    desc.width = plane.m_width;
    desc.height = plane.m_height;
    desc.has_data_plane = true;
    add_component_description(std::move(desc));
  }
}


void HeifPixelImage::register_component_descriptions(ComponentStorage& plane,
                                                 const std::vector<const ComponentDescription*>& source_descriptions)
{
  for (const ComponentDescription* src : source_descriptions) {
    uint32_t id = mint_component_id();
    plane.m_component_ids.push_back(id);

    // Start from the source description so per-component metadata
    // (component_type, gimi_content_id, has_data_plane, ...) is preserved.
    // If the lookup failed (shouldn't happen on a well-formed source),
    // fall back to a default-initialized description.
    ComponentDescription desc;
    if (src) {
      desc = *src;
    }
    desc.component_id = id;
    desc.channel = plane.m_channel;
    desc.datatype = plane.m_datatype;
    desc.bit_depth = plane.m_bit_depth;
    desc.width = plane.m_width;
    desc.height = plane.m_height;
    add_component_description(std::move(desc));
  }
}


Error HeifPixelImage::add_channel(heif_channel channel, uint32_t width, uint32_t height, int bit_depth,
                                const heif_security_limits* limits,
                                heif_component_datatype datatype)
{
  // for backwards compatibility, allow for 24/32 bits for RGB/RGBA interleaved chromas

  if (m_chroma == heif_chroma_interleaved_RGB && bit_depth == 24) {
    bit_depth = 8;
  }

  if (m_chroma == heif_chroma_interleaved_RGBA && bit_depth == 32) {
    bit_depth = 8;
  }

  int num_interleaved_pixels = num_interleaved_components_per_plane(m_chroma);

  ComponentStorage plane;
  plane.m_channel = channel;

  if (auto err = plane.alloc(width, height, datatype, bit_depth, num_interleaved_pixels, limits, m_memory_handle)) {
    return err;
  }

  register_component_descriptions(plane, map_channel_to_component_type(channel, m_chroma));
  m_storage.push_back(std::move(plane));
  return Error::Ok;
}


Error HeifPixelImage::ComponentStorage::alloc(uint32_t width, uint32_t height, heif_component_datatype datatype, int bit_depth,
                                        int num_interleaved_components,
                                        const heif_security_limits* limits,
                                        MemoryHandle& memory_handle)
{
  assert(bit_depth >= 1);
  assert(bit_depth <= 128);

  if (width == 0 || height == 0) {
    return {heif_error_Usage_error,
            heif_suberror_Unspecified,
            "Invalid image size"};
  }

  if (width == std::numeric_limits<uint32_t>::max() || height == std::numeric_limits<uint32_t>::max()) {
    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            "Image size too large for memory alignment"};
  }

  // use 16 byte alignment (enough for 128 bit data-types). Every row is an integer number of data-elements.
  uint16_t alignment = 16; // must be power of two

  m_width = width;
  m_height = height;

  m_mem_width = rounded_size(width);
  m_mem_height = rounded_size(height);

  assert(num_interleaved_components > 0 && num_interleaved_components <= 255);

  m_bit_depth = static_cast<uint8_t>(bit_depth);
  m_num_interleaved_components = static_cast<uint8_t>(num_interleaved_components);
  m_datatype = datatype;

  // Cache bytes-per-pixel for the inner-loop get_bytes_per_pixel().
  int bytes_per_component;
  if (bit_depth <= 8)        bytes_per_component = 1;
  else if (bit_depth <= 16)  bytes_per_component = 2;
  else if (bit_depth <= 32)  bytes_per_component = 4;
  else if (bit_depth <= 64)  bytes_per_component = 8;
  else { assert(bit_depth <= 128); bytes_per_component = 16; }
  m_bytes_per_pixel = static_cast<uint8_t>(bytes_per_component * num_interleaved_components);

  int bytes_per_pixel = m_bytes_per_pixel;

  uint64_t stride_64 = static_cast<uint64_t>(m_mem_width) * bytes_per_pixel;
  stride_64 = (stride_64 + alignment - 1U) & ~static_cast<uint64_t>(alignment - 1U);
  if (stride_64 > std::numeric_limits<size_t>::max()) {
    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            "Image stride overflow"};
  }
  stride = static_cast<size_t>(stride_64);

  assert(alignment>=1);

  if (limits &&
      limits->max_image_size_pixels &&
      limits->max_image_size_pixels / height < width) {

    std::stringstream sstr;
    sstr << "Allocating an image of size " << width << "x" << height << " exceeds the security limit of "
         << limits->max_image_size_pixels << " pixels";

    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            sstr.str()};
  }

  // Check for allocation size overflow using 64-bit arithmetic
  // Test case was an overlay image with size 1x134217727.
  // Width 1 gets aligned to 64 and then width * height overflows 32 bit systems.
  uint64_t alloc_64 = static_cast<uint64_t>(m_mem_height) * stride + alignment - 1;
  if (alloc_64 > std::numeric_limits<size_t>::max()) {
    return {heif_error_Memory_allocation_error,
            heif_suberror_Security_limit_exceeded,
            "Image allocation size overflow"};
  }
  allocation_size = static_cast<size_t>(alloc_64);

  if (auto err = memory_handle.alloc(allocation_size, limits, "image data")) {
    return err;
  }

    // --- allocate memory

  // Must zero-initialize: padding regions (stride, rounded_size(), alignment slack) are not
  // written by decoders, so uninitialized contents would leak across decoded images.
  allocated_mem = static_cast<uint8_t*>(std::calloc(1, allocation_size));
  if (allocated_mem == nullptr) {
    std::stringstream sstr;
    sstr << "Allocating " << allocation_size << " bytes failed";

    return {heif_error_Memory_allocation_error,
            heif_suberror_Unspecified,
            sstr.str()};
  }

  uint8_t* mem_8 = allocated_mem;

  // shift beginning of image data to aligned memory position

  auto mem_start_addr = (uint64_t) mem_8;
  auto mem_start_offset = (mem_start_addr & (alignment - 1U));
  if (mem_start_offset != 0) {
    mem_8 += alignment - mem_start_offset;
  }

  mem = mem_8;

  return Error::Ok;
}


Error HeifPixelImage::extend_padding_to_size(uint32_t width, uint32_t height, bool adjust_size,
                                             const heif_security_limits* limits)
{
  for (auto& component : m_storage) {
    // get_subsampled_size() only adjusts the size for Cb/Cr; for every other
    // channel it assumes the component has the full logical image size. We
    // cannot compute a correct padded size for a non-Cb/Cr component that does
    // not follow that assumption (e.g. multi-component ISO 23001-17 images).
    if ((component.m_width != m_width ||
         component.m_height != m_height) &&
        (component.m_channel != heif_channel_Cb &&
         component.m_channel != heif_channel_Cr)) {
      return Error{heif_error_Unsupported_feature,
                   heif_suberror_Unspecified,
                   "Cannot extend padding for an image with non-uniform component sizes."};
    }

    uint32_t subsampled_width, subsampled_height;
    get_subsampled_size(width, height, component.m_channel, m_chroma,
                        &subsampled_width, &subsampled_height);

    uint32_t old_width = component.m_width;
    uint32_t old_height = component.m_height;

    int bytes_per_pixel = component.get_bytes_per_pixel();

    if (component.m_mem_width < subsampled_width ||
        component.m_mem_height < subsampled_height) {

      ComponentStorage newPlane;
      newPlane.m_channel = component.m_channel;
      newPlane.m_component_ids = component.m_component_ids;
      if (auto err = newPlane.alloc(subsampled_width, subsampled_height, component.m_datatype, component.m_bit_depth,
                                    num_interleaved_components_per_plane(m_chroma),
                                    limits, m_memory_handle))
      {
        return err;
      }

      // This is not needed, but we have to silence the clang-tidy false positive.
      if (!newPlane.mem) {
        return Error::InternalError;
      }

      // copy the visible part of the old plane into the new plane

      for (uint32_t y = 0; y < component.m_height; y++) {
        memcpy(static_cast<uint8_t*>(newPlane.mem) + y * newPlane.stride,
               static_cast<uint8_t*>(component.mem) + y * component.stride,
               component.m_width * bytes_per_pixel);
      }

      // --- release the old plane before replacing it with the reallocated plane

      m_memory_handle.free(component.allocation_size);
      std::free(component.allocated_mem);

      component = newPlane;
    }

    // extend plane size

    if (old_width != subsampled_width) {
      for (uint32_t y = 0; y < old_height; y++) {
        for (uint32_t x = old_width; x < subsampled_width; x++) {
          memcpy(static_cast<uint8_t*>(component.mem) + y * component.stride + x * bytes_per_pixel,
                 static_cast<uint8_t*>(component.mem) + y * component.stride + (old_width - 1) * bytes_per_pixel,
                 bytes_per_pixel);
        }
      }
    }

    for (uint32_t y = old_height; y < subsampled_height; y++) {
      memcpy(static_cast<uint8_t*>(component.mem) + y * component.stride,
             static_cast<uint8_t*>(component.mem) + (old_height - 1) * component.stride,
             subsampled_width * bytes_per_pixel);
    }


    if (adjust_size) {
      component.m_width = subsampled_width;
      component.m_height = subsampled_height;
    }
  }

  // modify logical image size, if requested

  if (adjust_size) {
    m_width = width;
    m_height = height;
  }

  return Error::Ok;
}


Error HeifPixelImage::extend_to_size_with_zero(uint32_t width, uint32_t height, const heif_security_limits* limits)
{
  for (auto& component : m_storage) {
    // See extend_padding_to_size(): get_subsampled_size() assumes a non-Cb/Cr
    // component has the full logical image size, so we cannot compute a correct
    // target size for a component that does not follow that assumption.
    if ((component.m_width != m_width ||
         component.m_height != m_height) &&
        (component.m_channel != heif_channel_Cb &&
         component.m_channel != heif_channel_Cr)) {
      return Error{heif_error_Unsupported_feature,
                   heif_suberror_Unspecified,
                   "Cannot extend an image with non-uniform component sizes."};
    }

    uint32_t subsampled_width, subsampled_height;
    get_subsampled_size(width, height, component.m_channel, m_chroma,
                        &subsampled_width, &subsampled_height);

    uint32_t old_width = component.m_width;
    uint32_t old_height = component.m_height;

    int bytes_per_pixel = component.get_bytes_per_pixel();

    if (component.m_mem_width < subsampled_width ||
        component.m_mem_height < subsampled_height) {

      ComponentStorage newPlane;
      newPlane.m_channel = component.m_channel;
      newPlane.m_component_ids = component.m_component_ids;
      if (auto err = newPlane.alloc(subsampled_width, subsampled_height, component.m_datatype, component.m_bit_depth, num_interleaved_components_per_plane(m_chroma), limits, m_memory_handle)) {
        return err;
      }

      // This is not needed, but we have to silence the clang-tidy false positive.
      if (!newPlane.mem) {
        return Error::InternalError;
      }

      // copy the visible part of the old plane into the new plane

      for (uint32_t y = 0; y < component.m_height; y++) {
        memcpy(static_cast<uint8_t*>(newPlane.mem) + y * newPlane.stride,
               static_cast<uint8_t*>(component.mem) + y * component.stride,
               component.m_width * bytes_per_pixel);
      }

      // --- release the old plane before replacing it with the reallocated plane

      m_memory_handle.free(component.allocation_size);
      std::free(component.allocated_mem);

      component = newPlane;
    }

    // extend plane size

    uint8_t fill = 0;
    if (bytes_per_pixel == 1 && (component.m_channel == heif_channel_Cb || component.m_channel == heif_channel_Cr)) {
      fill = 128;
    }

    if (old_width != subsampled_width) {
      for (uint32_t y = 0; y < old_height; y++) {
        memset(static_cast<uint8_t*>(component.mem) + y * component.stride + old_width * bytes_per_pixel,
               fill,
               bytes_per_pixel * (subsampled_width - old_width));
      }
    }

    for (uint32_t y = old_height; y < subsampled_height; y++) {
      memset(static_cast<uint8_t*>(component.mem) + y * component.stride,
             fill,
             subsampled_width * bytes_per_pixel);
    }


    component.m_width = subsampled_width;
    component.m_height = subsampled_height;

    // Keep ComponentDescriptions in sync with the resized plane so that
    // get_component_width/height stays consistent with get_width(channel).
    for (uint32_t id : component.m_component_ids) {
      if (auto* desc = find_component_description(id)) {
        desc->width = subsampled_width;
        desc->height = subsampled_height;
      }
    }
  }

  // modify the logical image size

  m_width = width;
  m_height = height;

  return Error::Ok;
}

bool HeifPixelImage::has_channel(heif_channel channel) const
{
  return find_storage_for_channel(channel) != nullptr;
}


bool HeifPixelImage::has_alpha() const
{
  return has_channel(heif_channel_Alpha) ||
         get_chroma_format() == heif_chroma_interleaved_RGBA ||
         get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_BE ||
         get_chroma_format() == heif_chroma_interleaved_RRGGBBAA_LE;
}


uint32_t HeifPixelImage::get_width(heif_channel channel) const
{
  auto* comp = find_storage_for_channel(channel);
  if (!comp) {
    return 0;
  }

  return comp->m_width;
}


uint32_t HeifPixelImage::get_height(heif_channel channel) const
{
  auto* comp = find_storage_for_channel(channel);
  if (!comp) {
    return 0;
  }

  return comp->m_height;
}


uint32_t HeifPixelImage::get_width(uint32_t component_id) const
{
  auto* comp = find_storage_for_component(component_id);
  if (!comp) {
    return 0;
  }

  return comp->m_width;
}


uint32_t HeifPixelImage::get_height(uint32_t component_id) const
{
  auto* comp = find_storage_for_component(component_id);
  if (!comp) {
    return 0;
  }

  return comp->m_height;
}


bool HeifPixelImage::primary_planes_have_size(uint32_t width, uint32_t height) const
{
  auto channel_has_size = [&](heif_channel channel, uint32_t w, uint32_t h) {
    // get_width()/get_height() return 0 for an absent channel -> mismatch.
    return get_width(channel) == w && get_height(channel) == h;
  };

  switch (m_colorspace) {
    case heif_colorspace_monochrome:
      return channel_has_size(heif_channel_Y, width, height);

    case heif_colorspace_YCbCr: {
      // Y has the full size; Cb/Cr are subsampled according to the chroma format.
      // Downstream color conversion derives chroma plane indices from m_chroma, so
      // Cb/Cr must actually match those subsampled dimensions (issue #1796).
      if (!channel_has_size(heif_channel_Y, width, height)) {
        return false;
      }
      if (m_chroma == heif_chroma_monochrome) {
        return true;
      }
      uint32_t chroma_w, chroma_h;
      get_subsampled_size(width, height, heif_channel_Cb, m_chroma, &chroma_w, &chroma_h);
      return channel_has_size(heif_channel_Cb, chroma_w, chroma_h) &&
             channel_has_size(heif_channel_Cr, chroma_w, chroma_h);
    }

    case heif_colorspace_RGB:
      if (m_chroma == heif_chroma_444) {
        // planar RGB: all three planes must be present and have the full size
        return channel_has_size(heif_channel_R, width, height) &&
               channel_has_size(heif_channel_G, width, height) &&
               channel_has_size(heif_channel_B, width, height);
      }
      else {
        return channel_has_size(heif_channel_interleaved, width, height);
      }

    case heif_colorspace_filter_array:
      return channel_has_size(heif_channel_filter_array, width, height);

    case heif_colorspace_undefined:
    default:
      // Multi-component / custom-colorspace images (CMYK, bayer configs, ...)
      // cannot be checked generically; codec-specific overrides handle these.
      return true;
  }
}


std::set<heif_channel> HeifPixelImage::get_channel_set() const
{
  std::set<heif_channel> channels;

  for (const auto& component : m_storage) {
    channels.insert(component.m_channel);
  }

  return channels;
}


uint16_t HeifPixelImage::get_storage_bits_per_pixel(enum heif_channel channel) const
{
  auto* comp = find_storage_for_channel(channel);
  if (!comp) {
    // Channel not present. The return type is unsigned, so the historical
    // `return -1` actually yielded 65535; use 0 as an unambiguous
    // "not present" value (no real channel has 0 bits per pixel).
    return 0;
  }

  uint32_t bpp = comp->get_bytes_per_pixel() * 8;
  assert(bpp <= 256);
  return static_cast<uint8_t>(bpp);
}


uint16_t HeifPixelImage::get_bits_per_pixel(enum heif_channel channel) const
{
  auto* comp = find_storage_for_channel(channel);
  if (!comp) {
    // Channel not present -- see get_storage_bits_per_pixel().
    return 0;
  }

  return comp->m_bit_depth;
}


uint16_t HeifPixelImage::get_visual_image_bits_per_pixel() const
{
  switch (m_colorspace) {
    case heif_colorspace_monochrome:
      return get_bits_per_pixel(heif_channel_Y);
      break;
    case heif_colorspace_YCbCr:
      return std::max(get_bits_per_pixel(heif_channel_Y),
                      std::max(get_bits_per_pixel(heif_channel_Cb),
                               get_bits_per_pixel(heif_channel_Cr)));
      break;
    case heif_colorspace_RGB:
      if (m_chroma == heif_chroma_444) {
        return std::max(get_bits_per_pixel(heif_channel_R),
             std::max(get_bits_per_pixel(heif_channel_G),
                        get_bits_per_pixel(heif_channel_B)));
      }
      else {
        assert(has_channel(heif_channel_interleaved));
        return get_bits_per_pixel(heif_channel_interleaved);
      }
      break;
    case heif_colorspace_custom:
      return 0;
      break;
    case heif_colorspace_filter_array:
      assert(has_channel(heif_channel_filter_array));
      return get_bits_per_pixel(heif_channel_filter_array);
    default:
      assert(false);
      return 0;
      break;
  }
}


heif_component_datatype HeifPixelImage::get_datatype(heif_channel channel) const
{
  auto* comp = find_storage_for_channel(channel);
  if (!comp) {
    return heif_component_datatype_undefined;
  }

  return comp->m_datatype;
}


int HeifPixelImage::get_number_of_interleaved_components(heif_channel channel) const
{
  auto* comp = find_storage_for_channel(channel);
  if (!comp) {
    return 0;
  }

  return comp->m_num_interleaved_components;
}


Error HeifPixelImage::copy_new_channel_from(const std::shared_ptr<const HeifPixelImage>& src_image,
                                          heif_channel src_channel,
                                          heif_channel dst_channel,
                                          const heif_security_limits* limits)
{
  assert(src_image->has_channel(src_channel));
  assert(!has_channel(dst_channel));

  uint32_t width = src_image->get_width(src_channel);
  uint32_t height = src_image->get_height(src_channel);

  const auto* src_plane_ptr = src_image->find_storage_for_channel(src_channel);
  assert(src_plane_ptr != nullptr);
  const auto& src_plane = *src_plane_ptr;

  auto err = add_channel(dst_channel, width, height,
                       src_image->get_bits_per_pixel(src_channel), limits,
                       src_plane.m_datatype);
  if (err) {
    return err;
  }

  uint8_t* dst;
  size_t dst_stride = 0;

  const uint8_t* src;
  size_t src_stride = 0;

  src = src_image->get_channel_memory(src_channel, &src_stride);
  dst = get_channel_memory(dst_channel, &dst_stride);

  uint32_t bpl = width * (src_image->get_storage_bits_per_pixel(src_channel) / 8);

  for (uint32_t y = 0; y < height; y++) {
    memcpy(dst + y * dst_stride, src + y * src_stride, bpl);
  }

  return Error::Ok;
}


Error HeifPixelImage::extract_alpha_from_RGBA(const std::shared_ptr<const HeifPixelImage>& src_image,
                                              const heif_security_limits* limits)
{
  uint32_t width = src_image->get_width();
  uint32_t height = src_image->get_height();

  // The copy loop below assumes 8-bit interleaved RGBA (4 bytes per pixel,
  // alpha at byte offset 3). 16-bit interleaved formats (RRGGBBAA_*) have a
  // different layout and would be read/written incorrectly.
  if (src_image->get_bits_per_pixel(heif_channel_interleaved) != 8) {
    return {heif_error_Unsupported_feature,
            heif_suberror_Unspecified,
            "extract_alpha_from_RGBA only supports 8-bit interleaved RGBA"};
  }

  if (Error err = add_channel(heif_channel_Y, width, height, src_image->get_bits_per_pixel(heif_channel_interleaved), limits)) {
    return err;
  }

  uint8_t* dst;
  size_t dst_stride = 0;

  const uint8_t* src;
  size_t src_stride = 0;

  src = src_image->get_channel_memory(heif_channel_interleaved, &src_stride);
  dst = get_channel_memory(heif_channel_Y, &dst_stride);

  //int bpl = width * (src_image->get_storage_bits_per_pixel(src_channel) / 8);

  for (uint32_t y = 0; y < height; y++) {
    for (uint32_t x = 0; x < width; x++) {
      dst[y * dst_stride + x] = src[y * src_stride + 4 * x + 3];
    }
  }

  return Error::Ok;
}


Error HeifPixelImage::fill_new_channel(heif_channel dst_channel, uint16_t value, int width, int height, int bpp,
                                     const heif_security_limits* limits)
{
  if (Error err = add_channel(dst_channel, width, height, bpp, limits)) {
    return err;
  }

  fill_channel(dst_channel, value);

  return Error::Ok;
}


void HeifPixelImage::fill_channel(heif_channel dst_channel, uint16_t value)
{
  int num_interleaved = num_interleaved_components_per_plane(m_chroma);

  int bpp = get_bits_per_pixel(dst_channel);
  uint32_t width = get_width(dst_channel);
  uint32_t height = get_height(dst_channel);

  if (bpp <= 8) {
    uint8_t* dst;
    size_t dst_stride = 0;
    dst = get_channel_memory(dst_channel, &dst_stride);
    size_t width_bytes = static_cast<size_t>(width) * num_interleaved;

    for (uint32_t y = 0; y < height; y++) {
      memset(dst + y * dst_stride, value, width_bytes);
    }
  }
  else {
    uint16_t* dst;
    size_t dst_stride = 0;
    dst = get_channel_memory<uint16_t>(dst_channel, &dst_stride);
    dst_stride /= sizeof(uint16_t);

    size_t row_size = static_cast<size_t>(width) * num_interleaved;
    for (uint32_t y = 0; y < height; y++) {
      for (size_t x = 0; x < row_size; x++) {
        dst[y * dst_stride + x] = value;
      }
    }
  }
}


void HeifPixelImage::transfer_channel_from_image_as(const std::shared_ptr<HeifPixelImage>& source,
                                                  heif_channel src_channel,
                                                  heif_channel dst_channel)
{
  // TODO: check that dst_channel does not exist yet

  // Find and remove the component from source
  ComponentStorage plane;
  for (auto it = source->m_storage.begin(); it != source->m_storage.end(); ++it) {
    if (it->m_channel == src_channel) {
      plane = *it;
      source->m_storage.erase(it);
      break;
    }
  }
  source->m_memory_handle.free(plane.allocation_size);

  // Move the matching ComponentDescription(s) from source to destination.
  // The plane's old ids belong to source's m_next_component_id space and may
  // collide with destination's ids, so we mint fresh ids on destination and
  // rewrite plane.m_component_ids accordingly. Source's descriptions for the
  // moved ids are dropped (the buffer is gone).
  std::vector<uint32_t> new_ids;
  new_ids.reserve(plane.m_component_ids.size());
  for (uint32_t old_id : plane.m_component_ids) {
    // Take the source description (snapshot; the source entry is removed below).
    ComponentDescription desc;
    if (auto* src_desc = source->find_component_description(old_id)) {
      desc = *src_desc;
    }
    // Mint a destination id and reset description fields that change on transfer.
    desc.component_id = mint_component_id();
    desc.channel = dst_channel;
    // Re-derive the cmpd component_type from the new channel so the
    // transferred plane is described as e.g. "alpha" rather than carrying
    // over the source's "monochrome"/"Y" type. (The most common case is
    // moving an alpha aux item's Y plane onto a main image as Alpha.)
    auto types = map_channel_to_component_type(dst_channel, heif_chroma_undefined);
    if (!types.empty()) {
      desc.component_type = types[0];
    }
    new_ids.push_back(desc.component_id);
    add_component_description(std::move(desc));

    // Drop the source's description entry for old_id.
    source->remove_component_description(old_id);
  }
  plane.m_component_ids = std::move(new_ids);
  plane.m_channel = dst_channel;
  m_storage.push_back(plane);

  // Note: we assume that image planes are never transferred between heif_contexts
  m_memory_handle.alloc(plane.allocation_size,
                        source->m_memory_handle.get_security_limits(),
                        "transferred image data");
}


bool is_interleaved_with_alpha(heif_chroma chroma)
{
  switch (chroma) {
    case heif_chroma_undefined:
    case heif_chroma_monochrome:
    case heif_chroma_420:
    case heif_chroma_422:
    case heif_chroma_444:
    case heif_chroma_interleaved_RGB:
    case heif_chroma_interleaved_RRGGBB_BE:
    case heif_chroma_interleaved_RRGGBB_LE:
      return false;

    case heif_chroma_interleaved_RGBA:
    case heif_chroma_interleaved_RRGGBBAA_BE:
    case heif_chroma_interleaved_RRGGBBAA_LE:
      return true;
  }

  assert(false);
  return false;
}


Error HeifPixelImage::copy_image_to(const std::shared_ptr<const HeifPixelImage>& source, uint32_t x0, uint32_t y0)
{
  std::set<enum heif_channel> channels = source->get_channel_set();

  uint32_t w = get_width();
  uint32_t h = get_height();
  heif_chroma chroma = get_chroma_format();


  for (heif_channel channel : channels) {

    // The source channel set may contain channels that this image does not
    // have. get_channel_memory() would return nullptr for those, so skip them
    // instead of dereferencing a null pointer.
    if (!has_channel(channel)) {
      continue;
    }

    size_t tile_stride;
    const uint8_t* tile_data = source->get_channel_memory(channel, &tile_stride);

    size_t out_stride;
    uint8_t* out_data = get_channel_memory(channel, &out_stride);

    if (w <= x0 || h <= y0) {
      return {heif_error_Invalid_input,
              heif_suberror_Invalid_grid_data};
    }

    if (source->get_bits_per_pixel(channel) != get_bits_per_pixel(channel)) {
      return {heif_error_Invalid_input,
              heif_suberror_Wrong_tile_image_pixel_depth};
    }

    uint32_t src_width = source->get_width(channel);
    uint32_t src_height = source->get_height(channel);

    uint32_t xs = channel_width(x0, chroma, channel);
    uint32_t ys = channel_height(y0, chroma, channel);

    // Compute copy size from actual plane bounds to avoid chroma rounding mismatch.
    // channel_height(y0) + channel_height(h - y0) can exceed channel_height(h) with 4:2:0
    // due to ceiling division, so we use (plane_size - offset) instead.
    uint32_t copy_width = std::min(src_width, channel_width(w, chroma, channel) - xs);
    uint32_t copy_height = std::min(src_height, channel_height(h, chroma, channel) - ys);

    copy_width *= source->get_storage_bits_per_pixel(channel) / 8;
    xs *= source->get_storage_bits_per_pixel(channel) / 8;

    for (uint32_t py = 0; py < copy_height; py++) {
      memcpy(out_data + xs + (ys + py) * out_stride,
             tile_data + py * tile_stride,
             copy_width);
    }
  }

  return Error::Ok;
}


Result<std::shared_ptr<HeifPixelImage>> HeifPixelImage::rotate_ccw(int angle_degrees, const heif_security_limits* limits)
{
  // TODO: Bayer pattern, polarization patterns and sensor maps reference
  //   image geometry and are currently copied verbatim by
  //   forward_all_metadata_from(). For 90/270° rotations the layout is
  //   transposed and for 180° it is flipped, so the copied metadata is no
  //   longer semantically valid. Either rotate these structures along with
  //   the pixels, or return an error when such metadata is present and
  //   rotation would invalidate it.

  // --- for some subsampled chroma colorspaces, we have to transform to 4:4:4 before rotation

  bool need_conversion = false;

  if (get_chroma_format() == heif_chroma_422) {
    if (angle_degrees == 90 || angle_degrees == 270) {
      need_conversion = true;
    }
    else if (angle_degrees == 180 && has_odd_height()) {
      need_conversion = true;
    }
  }
  else if (get_chroma_format() == heif_chroma_420) {
    if (angle_degrees == 90 && has_odd_width()) {
      need_conversion = true;
    }
    else if (angle_degrees == 180 && (has_odd_width() || has_odd_height())) {
      need_conversion = true;
    }
    else if (angle_degrees == 270 && has_odd_height()) {
      need_conversion = true;
    }
  }

  if (need_conversion) {
    heif_color_conversion_options options{};
    heif_color_conversion_options_set_defaults(&options);

    auto converted_image_result = convert_colorspace(shared_from_this(), heif_colorspace_YCbCr, heif_chroma_444,
                                                     nclx_profile(), // default, undefined
                                                     get_bits_per_pixel(heif_channel_Y), options, nullptr, limits);
    if (!converted_image_result) {
      return converted_image_result.error();
    }

    return (*converted_image_result)->rotate_ccw(angle_degrees, limits);
  }


  // --- create output image

  if (angle_degrees == 0) {
    return shared_from_this();
  }

  uint32_t out_width = m_width;
  uint32_t out_height = m_height;

  if (angle_degrees == 90 || angle_degrees == 270) {
    std::swap(out_width, out_height);
  }

  std::shared_ptr<HeifPixelImage> out_img = std::make_shared<HeifPixelImage>();
  out_img->create(out_width, out_height, m_colorspace, m_chroma);
  out_img->copy_metadata_from(*this);


  // --- rotate all channels

  for (const auto &component: m_storage) {
    uint32_t out_plane_width = component.m_width;
    uint32_t out_plane_height = component.m_height;

    if (angle_degrees == 90 || angle_degrees == 270) {
      std::swap(out_plane_width, out_plane_height);
    }

    ComponentStorage out_component;
    out_component.m_channel = component.m_channel;

    if (Error err = out_component.alloc(out_plane_width, out_plane_height,
                                        component.m_datatype, component.m_bit_depth,
                                        component.m_num_interleaved_components,
                                        limits, out_img->m_memory_handle)) {
      return err;
    }

    // Clone per-component metadata (component_type, gimi_content_id, ...)
    // from the source descriptions rather than re-deriving from chroma, so
    // images built via add_component() preserve their original component
    // types and content ids.
    std::vector<const ComponentDescription*> src_descs;
    src_descs.reserve(component.m_component_ids.size());
    for (uint32_t cid : component.m_component_ids) {
      src_descs.push_back(find_component_description(cid));
    }
    out_img->register_component_descriptions(out_component, src_descs);

    if (component.m_bit_depth <= 8) {
      component.rotate_ccw<uint8_t>(angle_degrees, out_component);
    }
    else if (component.m_bit_depth <= 16) {
      component.rotate_ccw<uint16_t>(angle_degrees, out_component);
    }
    else if (component.m_bit_depth <= 32) {
      component.rotate_ccw<uint32_t>(angle_degrees, out_component);
    }
    else if (component.m_bit_depth <= 64) {
      component.rotate_ccw<uint64_t>(angle_degrees, out_component);
    }
    else if (component.m_bit_depth <= 128) {
      component.rotate_ccw<heif_complex64>(angle_degrees, out_component);
    }
    else {
      std::stringstream sstr;
      sstr << "Cannot rotate images with " << component.m_bit_depth << " bits per pixel";
      return Error{heif_error_Unsupported_feature,
                   heif_suberror_Unspecified,
                   sstr.str()};
    }

    out_img->m_storage.push_back(std::move(out_component));
  }

  out_img->add_warnings(get_warnings());

  return out_img;
}

template<typename T>
void HeifPixelImage::ComponentStorage::rotate_ccw(int angle_degrees,
                                            ComponentStorage& out_plane) const
{
  uint32_t w = m_width;
  uint32_t h = m_height;

  size_t in_stride = stride / sizeof(T);
  const T* in_data = static_cast<const T*>(mem);

  size_t out_stride = out_plane.stride / sizeof(T);
  T* out_data = static_cast<T*>(out_plane.mem);

  if (angle_degrees == 270) {
    for (uint32_t x = 0; x < h; x++)
      for (uint32_t y = 0; y < w; y++) {
        out_data[y * out_stride + x] = in_data[(h - 1 - x) * in_stride + y];
      }
  } else if (angle_degrees == 180) {
    for (uint32_t y = 0; y < h; y++)
      for (uint32_t x = 0; x < w; x++) {
        out_data[y * out_stride + x] = in_data[(h - 1 - y) * in_stride + (w - 1 - x)];
      }
  } else if (angle_degrees == 90) {
    for (uint32_t x = 0; x < h; x++)
      for (uint32_t y = 0; y < w; y++) {
        out_data[y * out_stride + x] = in_data[x * in_stride + (w - 1 - y)];
      }
  }
}


template<typename T>
void HeifPixelImage::ComponentStorage::mirror_inplace(heif_transform_mirror_direction direction)
{
  uint32_t w = m_width;
  uint32_t h = m_height;

  T* data = static_cast<T*>(mem);

  if (direction == heif_transform_mirror_direction_horizontal) {
    for (uint32_t y = 0; y < h; y++) {
      for (uint32_t x = 0; x < w / 2; x++)
        std::swap(data[y * stride / sizeof(T) + x], data[y * stride / sizeof(T) + w - 1 - x]);
    }
  } else {
    for (uint32_t y = 0; y < h / 2; y++) {
      for (uint32_t x = 0; x < w; x++)
        std::swap(data[y * stride / sizeof(T) + x], data[(h - 1 - y) * stride / sizeof(T) + x]);
    }
  }
}


Result<std::shared_ptr<HeifPixelImage>> HeifPixelImage::mirror_inplace(heif_transform_mirror_direction direction,
                                                                       const heif_security_limits* limits)
{
  // TODO: Bayer pattern, polarization patterns and sensor maps reference
  //   image geometry. This function mirrors the pixel data in place but
  //   leaves those structures untouched, so a horizontal/vertical mirror
  //   leaves them out of sync with the pixel layout. Either mirror these
  //   structures along with the pixels, or return an error when such
  //   metadata is present and the mirror would invalidate it.

  // --- for some subsampled chroma colorspaces, we have to transform to 4:4:4 before rotation

  bool need_conversion = false;

  if (get_chroma_format() == heif_chroma_422) {
    if (direction == heif_transform_mirror_direction_horizontal && has_odd_width()) {
      need_conversion = true;
    }
  }
  else if (get_chroma_format() == heif_chroma_420) {
    if (has_odd_width() || has_odd_height()) {
      need_conversion = true;
    }
  }

  if (need_conversion) {
    heif_color_conversion_options options{};
    heif_color_conversion_options_set_defaults(&options);

    auto converted_image_result = convert_colorspace(shared_from_this(), heif_colorspace_YCbCr, heif_chroma_444,
                                                     nclx_profile(), // default, undefined
                                                     get_bits_per_pixel(heif_channel_Y), options, nullptr, limits);
    if (!converted_image_result) {
      return converted_image_result.error();
    }

    return (*converted_image_result)->mirror_inplace(direction, limits);
  }


  for (auto& component : m_storage) {
    if (component.m_bit_depth <= 8) {
      component.mirror_inplace<uint8_t>(direction);
    }
    else if (component.m_bit_depth <= 16) {
      component.mirror_inplace<uint16_t>(direction);
    }
    else if (component.m_bit_depth <= 32) {
      component.mirror_inplace<uint32_t>(direction);
    }
    else if (component.m_bit_depth <= 64) {
      component.mirror_inplace<uint64_t>(direction);
    }
    else if (component.m_bit_depth <= 128) {
      component.mirror_inplace<heif_complex64>(direction);
    }
    else {
      std::stringstream sstr;
      sstr << "Cannot mirror images with " << component.m_bit_depth << " bits per pixel";
      return Error{heif_error_Unsupported_feature,
                   heif_suberror_Unspecified,
                   sstr.str()};
    }
  }

  return shared_from_this();
}


int HeifPixelImage::ComponentStorage::get_bytes_per_pixel() const
{
  return m_bytes_per_pixel;
}


Result<std::shared_ptr<HeifPixelImage>> HeifPixelImage::crop(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom,
                                                             const heif_security_limits* limits) const
{
  // TODO: Bayer pattern, polarization patterns and sensor maps reference
  //   image geometry and are currently copied verbatim by
  //   forward_all_metadata_from(). A crop shifts the (0,0) origin and
  //   changes the image dimensions, so the copied metadata may no longer
  //   match the cropped image (e.g. a 2x2 Bayer pattern with an odd
  //   left/top offset, or a sensor NUC map sized to the original image).
  //   Either translate / resample these structures to the crop region, or
  //   return an error when the crop would invalidate them.

  // (left, right, top, bottom) are coordinate endpoints of the kept region.
  // Reject inverted or out-of-bounds rectangles so the unsigned arithmetic
  // below cannot underflow into a multi-GB memcpy (issue #1746).
  if (right < left || bottom < top || right >= m_width || bottom >= m_height) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value,
                 "Invalid crop region"};
  }

  // --- for some subsampled chroma colorspaces, we have to transform to 4:4:4 before cropping

  bool need_conversion = false;

  if (get_chroma_format() == heif_chroma_422 && (left & 1) == 1) {
      need_conversion = true;
  }
  else if (get_chroma_format() == heif_chroma_420 &&
           ((left & 1) == 1 || (top & 1) == 1)) {
    need_conversion = true;
  }

  if (need_conversion) {
    heif_color_conversion_options options{};
    heif_color_conversion_options_set_defaults(&options);

    auto converted_image_result = convert_colorspace(shared_from_this(), heif_colorspace_YCbCr, heif_chroma_444,
                                                     nclx_profile(), // default, undefined
                                                     get_bits_per_pixel(heif_channel_Y), options, nullptr, limits);

    if (!converted_image_result) {
      return converted_image_result.error();
    }

    return (*converted_image_result)->crop(left, right, top, bottom, limits);
  }



  auto out_img = std::make_shared<HeifPixelImage>();
  out_img->create(right - left + 1, bottom - top + 1, m_colorspace, m_chroma);
  out_img->copy_metadata_from(*this);


  // --- crop all channels

  for (const auto& component : m_storage) {
    heif_channel channel = component.m_channel;

    uint32_t plane_left = get_subsampled_size_h(left, channel, m_chroma, scaling_mode::is_divisible); // is always divisible
    uint32_t plane_right = get_subsampled_size_h(right, channel, m_chroma, scaling_mode::round_down); // this keeps enough chroma since 'right' is a coordinate and not the width
    uint32_t plane_top = get_subsampled_size_v(top, channel, m_chroma, scaling_mode::is_divisible);
    uint32_t plane_bottom = get_subsampled_size_v(bottom, channel, m_chroma, scaling_mode::round_down);

    ComponentStorage out_plane;
    out_plane.m_channel = channel;

    if (Error err = out_plane.alloc(plane_right - plane_left + 1,
                                    plane_bottom - plane_top + 1,
                                    component.m_datatype, component.m_bit_depth,
                                    component.m_num_interleaved_components,
                                    limits, out_img->m_memory_handle)) {
      return err;
    }

    // Clone per-component metadata (component_type, gimi_content_id, ...)
    // from the source descriptions rather than re-deriving from chroma, so
    // images built via add_component() preserve their original component
    // types and content ids.
    std::vector<const ComponentDescription*> src_descs;
    src_descs.reserve(component.m_component_ids.size());
    for (uint32_t cid : component.m_component_ids) {
      src_descs.push_back(find_component_description(cid));
    }
    out_img->register_component_descriptions(out_plane, src_descs);

    int bytes_per_pixel = component.get_bytes_per_pixel();
    component.crop(plane_left, plane_right, plane_top, plane_bottom, bytes_per_pixel, out_plane);

    out_img->m_storage.push_back(std::move(out_plane));
  }

  out_img->add_warnings(get_warnings());

  return out_img;
}


void HeifPixelImage::ComponentStorage::crop(uint32_t left, uint32_t right, uint32_t top, uint32_t bottom,
                                      int bytes_per_pixel, ComponentStorage& out_plane) const
{
  size_t in_stride = stride;
  auto* in_data = static_cast<const uint8_t*>(mem);

  size_t out_stride = out_plane.stride;
  auto* out_data = static_cast<uint8_t*>(out_plane.mem);

  for (uint32_t y = top; y <= bottom; y++) {
    memcpy(&out_data[(y - top) * out_stride],
           &in_data[y * in_stride + left * bytes_per_pixel],
           (right - left + 1) * bytes_per_pixel);
  }
}


Error HeifPixelImage::fill_RGB_16bit(uint16_t r, uint16_t g, uint16_t b, uint16_t a)
{
  for (const auto& channel : {heif_channel_R, heif_channel_G, heif_channel_B, heif_channel_Alpha}) {

    auto* comp = find_storage_for_channel(channel);
    if (!comp) {

      // alpha channel is optional, R,G,B is required
      if (channel == heif_channel_Alpha) {
        continue;
      }

      return {heif_error_Usage_error,
              heif_suberror_Nonexisting_image_channel_referenced};

    }

    ComponentStorage& plane = *comp;

    if (plane.m_bit_depth != 8) {
      return {heif_error_Unsupported_feature,
              heif_suberror_Unspecified,
              "Can currently only fill images with 8 bits per pixel"};
    }

    size_t h = plane.m_height;

    size_t stride = plane.stride;
    auto* data = static_cast<uint8_t*>(plane.mem);

    uint16_t val16;
    switch (channel) {
      case heif_channel_R:
        val16 = r;
        break;
      case heif_channel_G:
        val16 = g;
        break;
      case heif_channel_B:
        val16 = b;
        break;
      case heif_channel_Alpha:
        val16 = a;
        break;
      default:
        // initialization only to avoid warning of uninitialized variable.
        val16 = 0;
        // Should already be detected by the check above ("find_storage_for_channel").
        assert(false);
    }

    auto val8 = static_cast<uint8_t>(val16 >> 8U);


    // memset() even when h * stride > sizeof(size_t)

    if (std::numeric_limits<size_t>::max() / stride > h) {
      // can fill in one step
      memset(data, val8, stride * h);
    }
    else {
      // fill line by line
      auto* p = data;

      for (size_t y=0;y<h;y++) {
        memset(p, val8, stride);
        p += stride;
      }
    }
  }

  return Error::Ok;
}


uint32_t negate_negative_int32(int32_t x)
{
  assert(x <= 0);

  if (x == INT32_MIN) {
    return static_cast<uint32_t>(INT32_MAX) + 1;
  }
  else {
    return static_cast<uint32_t>(-x);
  }
}


Error HeifPixelImage::overlay(std::shared_ptr<HeifPixelImage>& overlay, int32_t dx, int32_t dy)
{
  // This function places the overlay using the full-resolution (dx,dy) offset
  // directly as a per-plane offset. That is only correct when every plane has
  // the full logical image size, i.e. for non-subsampled chroma formats.
  // Subsampled chroma (4:2:0 / 4:2:2) would be mis-placed and could even write
  // outside of the smaller Cb/Cr planes.
  auto has_subsampled_chroma = [](heif_chroma chroma) {
    return chroma == heif_chroma_420 || chroma == heif_chroma_422;
  };

  if (has_subsampled_chroma(get_chroma_format()) ||
      has_subsampled_chroma(overlay->get_chroma_format())) {
    return {heif_error_Unsupported_feature,
            heif_suberror_Unspecified,
            "Overlaying images with subsampled chroma is not supported"};
  }

  std::set<heif_channel> channels = overlay->get_channel_set();

  bool has_alpha = overlay->has_channel(heif_channel_Alpha);
  //bool has_alpha_me = has_channel(heif_channel_Alpha);

  size_t alpha_stride = 0;
  uint8_t* alpha_p;
  alpha_p = overlay->get_channel_memory(heif_channel_Alpha, &alpha_stride);

  for (heif_channel channel : channels) {
    if (!has_channel(channel)) {
      continue;
    }

    size_t in_stride = 0;
    const uint8_t* in_p;

    size_t out_stride = 0;
    uint8_t* out_p;

    in_p = overlay->get_channel_memory(channel, &in_stride);
    out_p = get_channel_memory(channel, &out_stride);

    uint32_t in_w = overlay->get_width(channel);
    uint32_t in_h = overlay->get_height(channel);

    uint32_t out_w = get_width(channel);
    uint32_t out_h = get_height(channel);


    // --- check whether overlay image overlaps with current image
    // Note: all components share the logical image size, so if the overlay
    // image lies completely outside for one component it does so for all of
    // them -> we can return instead of just skipping the current component.

    if (dx > 0 && static_cast<uint32_t>(dx) >= out_w) {
      // the overlay image is completely outside the right border -> skip overlaying
      return Error::Ok;
    }
    else if (dx < 0 && in_w <= negate_negative_int32(dx)) {
      // the overlay image is completely outside the left border -> skip overlaying
      return Error::Ok;
    }

    if (dy > 0 && static_cast<uint32_t>(dy) >= out_h) {
      // the overlay image is completely outside the bottom border -> skip overlaying
      return Error::Ok;
    }
    else if (dy < 0 && in_h <= negate_negative_int32(dy)) {
      // the overlay image is completely outside the top border -> skip overlaying
      return Error::Ok;
    }


    // --- compute overlapping area

    // top-left points where to start copying in source and destination
    uint32_t in_x0;
    uint32_t in_y0;
    uint32_t out_x0;
    uint32_t out_y0;

    // right border
    if (dx + static_cast<int64_t>(in_w) > out_w) {
      // overlay image extends partially outside of right border
      // Notes:
      // - (out_w-dx) cannot underflow because dx<out_w is ensured above
      // - (out_w-dx) cannot overflow (for dx<0) because, as just checked, out_w-dx < in_w
      //              and in_w fits into uint32_t
      in_w = static_cast<uint32_t>(static_cast<int64_t>(out_w) - dx);
    }

    // bottom border
    if (dy + static_cast<int64_t>(in_h) > out_h) {
      // overlay image extends partially outside of bottom border
      in_h = static_cast<uint32_t>(static_cast<int64_t>(out_h) - dy);
    }

    // left border
    if (dx < 0) {
      // overlay image starts partially outside of left border

      in_x0 = negate_negative_int32(dx);
      out_x0 = 0;
      in_w = in_w - in_x0; // in_x0 < in_w because in_w > -dx = in_x0
    }
    else {
      in_x0 = 0;
      out_x0 = static_cast<uint32_t>(dx);
    }

    // top border
    if (dy < 0) {
      // overlay image started partially outside of top border

      in_y0 = negate_negative_int32(dy);
      out_y0 = 0;
      in_h = in_h - in_y0; // in_y0 < in_h because in_h > -dy = in_y0
    }
    else {
      in_y0 = 0;
      out_y0 = static_cast<uint32_t>(dy);
    }

    // --- computer overlay in overlapping area

    for (uint32_t y = in_y0; y < in_h; y++) {
      if (!has_alpha) {
        memcpy(out_p + out_x0 + (out_y0 + y - in_y0) * out_stride,
               in_p + in_x0 + y * in_stride,
               in_w);
      }
      else {
        for (uint32_t x = in_x0; x < in_w; x++) {
          uint8_t* outptr = &out_p[out_x0 + (out_y0 + y - in_y0) * out_stride + x];
          uint8_t in_val = in_p[in_x0 + y * in_stride + x];
          uint8_t alpha_val = alpha_p[in_x0 + y * alpha_stride + x];

          *outptr = (uint8_t) ((in_val * alpha_val + *outptr * (255 - alpha_val)) / 255);
        }
      }
    }
  }

  return Error::Ok;
}


Error HeifPixelImage::scale_nearest_neighbor(std::shared_ptr<HeifPixelImage>& out_img,
                                             uint32_t width, uint32_t height,
                                             const heif_security_limits* limits) const
{
  out_img = std::make_shared<HeifPixelImage>();
  out_img->create(width, height, m_colorspace, m_chroma);


  // --- create output image with scaled planes

  if (has_channel(heif_channel_interleaved)) {
    if (auto err = out_img->add_channel(heif_channel_interleaved, width, height, get_bits_per_pixel(heif_channel_interleaved), limits)) {
      return err;
    }
  }
  else {
    if (get_colorspace() == heif_colorspace_RGB) {
      if (!has_channel(heif_channel_R) ||
          !has_channel(heif_channel_G) ||
          !has_channel(heif_channel_B)) {
        return {heif_error_Invalid_input, heif_suberror_Unspecified, "RGB input without R,G,B, planes"};
      }

      if (auto err = out_img->add_channel(heif_channel_R, width, height, get_bits_per_pixel(heif_channel_R), limits)) {
        return err;
      }
      if (auto err = out_img->add_channel(heif_channel_G, width, height, get_bits_per_pixel(heif_channel_G), limits)) {
        return err;
      }
      if (auto err = out_img->add_channel(heif_channel_B, width, height, get_bits_per_pixel(heif_channel_B), limits)) {
        return err;
      }
    }
    else if (get_colorspace() == heif_colorspace_monochrome) {
      if (!has_channel(heif_channel_Y)) {
        return {heif_error_Invalid_input, heif_suberror_Unspecified, "monochrome input with no Y plane"};
      }

      if (auto err = out_img->add_channel(heif_channel_Y, width, height, get_bits_per_pixel(heif_channel_Y), limits)) {
        return err;
      }
    }
    else if (get_colorspace() == heif_colorspace_YCbCr) {
      if (!has_channel(heif_channel_Y) ||
          !has_channel(heif_channel_Cb) ||
          !has_channel(heif_channel_Cr)) {
        return {heif_error_Invalid_input, heif_suberror_Unspecified, "YCbCr image without Y,Cb,Cr planes"};
      }

      uint32_t cw, ch;
      get_subsampled_size(width, height, heif_channel_Cb, get_chroma_format(), &cw, &ch);
      if (auto err = out_img->add_channel(heif_channel_Y, width, height, get_bits_per_pixel(heif_channel_Y), limits)) {
        return err;
      }
      if (auto err = out_img->add_channel(heif_channel_Cb, cw, ch, get_bits_per_pixel(heif_channel_Cb), limits)) {
        return err;
      }
      if (auto err = out_img->add_channel(heif_channel_Cr, cw, ch, get_bits_per_pixel(heif_channel_Cr), limits)) {
        return err;
      }
    }
    else {
      return {heif_error_Invalid_input, heif_suberror_Unspecified, "unknown color configuration"};
    }

    if (has_channel(heif_channel_Alpha)) {
      if (auto err = out_img->add_channel(heif_channel_Alpha, width, height, get_bits_per_pixel(heif_channel_Alpha), limits)) {
        return err;
      }
    }
  }


  // --- scale all channels

  int nInterleaved = num_interleaved_components_per_plane(m_chroma);
  if (nInterleaved > 1) {
    const auto* comp = find_storage_for_channel(heif_channel_interleaved);
    assert(comp != nullptr); // the plane must exist since we have an interleaved chroma format
    const ComponentStorage& plane = *comp;

    uint32_t out_w = out_img->get_width(heif_channel_interleaved);
    uint32_t out_h = out_img->get_height(heif_channel_interleaved);

    if (plane.m_bit_depth <= 8) {
      // SDR interleaved

      size_t in_stride = plane.stride;
      const auto* in_data = static_cast<const uint8_t*>(plane.mem);

      size_t out_stride = 0;
      auto* out_data = out_img->get_channel_memory(heif_channel_interleaved, &out_stride);

      for (uint32_t y = 0; y < out_h; y++) {
        uint32_t iy = static_cast<uint32_t>(static_cast<uint64_t>(y) * m_height / height);

        for (uint32_t x = 0; x < out_w; x++) {
          uint32_t ix = static_cast<uint32_t>(static_cast<uint64_t>(x) * m_width / width);

          for (int c = 0; c < nInterleaved; c++) {
            out_data[y * out_stride + x * nInterleaved + c] = in_data[iy * in_stride + ix * nInterleaved + c];
          }
        }
      }
    }
    else {
      // HDR interleaved
      // TODO: untested

      size_t in_stride = plane.stride;
      const uint16_t* in_data = static_cast<const uint16_t*>(plane.mem);

      size_t out_stride = 0;
      uint16_t* out_data = out_img->get_channel_memory<uint16_t>(heif_channel_interleaved, &out_stride);

      in_stride /= 2;
      out_stride /= 2;

      for (uint32_t y = 0; y < out_h; y++) {
        uint32_t iy = static_cast<uint32_t>(static_cast<uint64_t>(y) * m_height / height);

        for (uint32_t x = 0; x < out_w; x++) {
          uint32_t ix = static_cast<uint32_t>(static_cast<uint64_t>(x) * m_width / width);

          for (int c = 0; c < nInterleaved; c++) {
            out_data[y * out_stride + x * nInterleaved + c] = in_data[iy * in_stride + ix * nInterleaved + c];
          }
        }
      }
    }
  }
  else {
    for (const auto& component : m_storage) {
      heif_channel channel = component.m_channel;
      const ComponentStorage& plane = component;

      if (!out_img->has_channel(channel)) {
        return {heif_error_Invalid_input, heif_suberror_Unspecified, "scaling input has extra color plane"};
      }


      uint32_t out_w = out_img->get_width(channel);
      uint32_t out_h = out_img->get_height(channel);

      if (plane.m_bit_depth <= 8) {
        // SDR planar

        size_t in_stride = plane.stride;
        const auto* in_data = static_cast<const uint8_t*>(plane.mem);

        size_t out_stride = 0;
        auto* out_data = out_img->get_channel_memory(channel, &out_stride);

        for (uint32_t y = 0; y < out_h; y++) {
          uint32_t iy = static_cast<uint32_t>(static_cast<uint64_t>(y) * m_height / height);

          for (uint32_t x = 0; x < out_w; x++) {
            uint32_t ix = static_cast<uint32_t>(static_cast<uint64_t>(x) * m_width / width);

            out_data[y * out_stride + x] = in_data[iy * in_stride + ix];
          }
        }
      }
      else {
        // HDR planar

        size_t in_stride = plane.stride;
        const uint16_t* in_data = static_cast<const uint16_t*>(plane.mem);

        size_t out_stride = 0;
        uint16_t* out_data = out_img->get_channel_memory<uint16_t>(channel, &out_stride);

        in_stride /= 2;
        out_stride /= 2;

        for (uint32_t y = 0; y < out_h; y++) {
          uint32_t iy = static_cast<uint32_t>(static_cast<uint64_t>(y) * m_height / height);

          for (uint32_t x = 0; x < out_w; x++) {
            uint32_t ix = static_cast<uint32_t>(static_cast<uint64_t>(x) * m_width / width);

            out_data[y * out_stride + x] = in_data[iy * in_stride + ix];
          }
        }
      }
    }
  }

  return Error::Ok;
}


void HeifPixelImage::debug_dump() const
{
  auto channels = get_channel_set();
  for (auto c : channels) {
    size_t stride = 0;
    const uint8_t* p = get_channel_memory(c, &stride);

    // clamp the dump region to the actual plane size to avoid reading past it
    uint32_t dump_w = std::min(get_width(c), 8u);
    uint32_t dump_h = std::min(get_height(c), 8u);

    for (uint32_t y = 0; y < dump_h; y++) {
      for (uint32_t x = 0; x < dump_w; x++) {
        printf("%02x ", p[y * stride + x]);
      }
      printf("\n");
    }
  }
}

Error HeifPixelImage::create_clone_image_at_new_size(const std::shared_ptr<const HeifPixelImage>& source, uint32_t w, uint32_t h,
                                                     const heif_security_limits* limits)
{
  heif_colorspace colorspace = source->get_colorspace();
  heif_chroma chroma = source->get_chroma_format();

  create(w, h, colorspace, chroma);

  for (const auto& src_plane : source->m_storage) {
    // TODO: do we also support images where some planes (e.g. the alpha-plane) have a different size than the main image?
    //       We could do this by scaling all planes proportionally. This would also handle chroma channels implicitly.
    uint32_t plane_w = channel_width(w, chroma, src_plane.m_channel);
    uint32_t plane_h = channel_height(h, chroma, src_plane.m_channel);

    ComponentStorage plane;
    plane.m_channel = src_plane.m_channel;
    plane.m_component_ids = src_plane.m_component_ids;

    if (auto err = plane.alloc(plane_w, plane_h, src_plane.m_datatype, src_plane.m_bit_depth,
                               src_plane.m_num_interleaved_components, limits, m_memory_handle)) {
      return err;
    }

    m_storage.push_back(plane);
  }

  // The source's descriptions carry the source's geometry; the planes above
  // were allocated at the new (w,h) size, so descriptions must be resized to
  // match — otherwise get_component_width/height returns stale source dims.
  auto descs = source->get_component_descriptions();
  for (auto& desc : descs) {
    desc.width = channel_width(w, chroma, desc.channel);
    desc.height = channel_height(h, chroma, desc.channel);
  }
  set_component_descriptions(std::move(descs), source->peek_next_component_id());

  copy_metadata_from(*source);

  return Error::Ok;
}


Result<std::shared_ptr<HeifPixelImage>>
HeifPixelImage::extract_image_area(uint32_t x0, uint32_t y0, uint32_t w, uint32_t h,
                                   const heif_security_limits* limits) const
{
  // The top-left corner must lie inside the image. Without this check,
  // get_width() - x0 (and the per-channel offsets derived from x0/y0) would
  // underflow and the copy loop below would read far outside the source planes.
  if (x0 >= get_width() || y0 >= get_height()) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value,
                 "extract_image_area: top-left position is outside the image"};
  }

  uint32_t minW = std::min(w, get_width() - x0);
  uint32_t minH = std::min(h, get_height() - y0);

  auto areaImg = std::make_shared<HeifPixelImage>();
  Error err = areaImg->create_clone_image_at_new_size(shared_from_this(), minW, minH, limits);
  if (err) {
    return err;
  }

  std::set<enum heif_channel> channels = get_channel_set();
  heif_chroma chroma = get_chroma_format();

  for (heif_channel channel : channels) {

    size_t src_stride;
    const uint8_t* src_data = get_channel_memory(channel, &src_stride);

    size_t out_stride;
    uint8_t* out_data = areaImg->get_channel_memory(channel, &out_stride);

    if (areaImg->get_bits_per_pixel(channel) != get_bits_per_pixel(channel)) {
      return Error{
        heif_error_Invalid_input,
        heif_suberror_Wrong_tile_image_pixel_depth
      };
    }

    uint32_t xs = channel_width(x0, chroma, channel);
    uint32_t ys = channel_height(y0, chroma, channel);

    // Clamp copy size to source plane bounds to avoid chroma rounding mismatch OOB read.
    uint32_t src_plane_h = channel_height(get_height(), chroma, channel);
    uint32_t src_plane_w = channel_width(get_width(), chroma, channel);
    uint32_t copy_width = std::min(channel_width(minW, chroma, channel), src_plane_w - xs);
    uint32_t copy_height = std::min(channel_height(minH, chroma, channel), src_plane_h - ys);

    copy_width *= get_storage_bits_per_pixel(channel) / 8;
    xs *= get_storage_bits_per_pixel(channel) / 8;

    for (uint32_t py = 0; py < copy_height; py++) {
      memcpy(out_data + py * out_stride,
             src_data + xs + (ys + py) * src_stride,
             copy_width);
    }
  }

  err = areaImg->extend_to_size_with_zero(w,h,limits);
  if (err) {
    return err;
  }

  return areaImg;
}


// --- index-based component access methods

HeifPixelImage::ComponentStorage* HeifPixelImage::find_storage_for_component(uint32_t component_id)
{
  for (auto& plane : m_storage) {
    // we search through all indices in case we have an interleaved plane
    if (std::find(plane.m_component_ids.begin(),
                  plane.m_component_ids.end(),
                  component_id) != plane.m_component_ids.end()) {
      return &plane;
    }
  }
  return nullptr;
}


const HeifPixelImage::ComponentStorage* HeifPixelImage::find_storage_for_component(uint32_t component_id) const
{
  return const_cast<HeifPixelImage*>(this)->find_storage_for_component(component_id);
}


heif_channel HeifPixelImage::get_component_channel(uint32_t component_id) const
{
  auto* desc = find_component_description(component_id);
  assert(desc);
  return desc->channel;
}


uint32_t HeifPixelImage::get_component_width(uint32_t component_id) const
{
  auto* desc = find_component_description(component_id);
  assert(desc);
  return desc->width;
}


uint32_t HeifPixelImage::get_component_height(uint32_t component_id) const
{
  auto* desc = find_component_description(component_id);
  assert(desc);
  return desc->height;
}


uint16_t HeifPixelImage::get_component_bits_per_pixel(uint32_t component_id) const
{
  auto* desc = find_component_description(component_id);
  assert(desc);
  return desc->bit_depth;
}


uint16_t HeifPixelImage::get_component_storage_bits_per_pixel(uint32_t component_id) const
{
  // Storage is a buffer-layout concern (alignment / padding), so this stays
  // routed through ComponentStorage rather than the description.
  auto* comp = find_storage_for_component(component_id);
  assert(comp);
  uint32_t bpp = comp->get_bytes_per_pixel() * 8;
  assert(bpp);
  return static_cast<uint16_t>(bpp);
}


heif_component_datatype HeifPixelImage::get_component_datatype(uint32_t component_id) const
{
  auto* desc = find_component_description(component_id);
  assert(desc);
  return desc->datatype;
}


uint16_t HeifPixelImage::get_component_type(uint32_t component_id) const
{
  if (const auto* desc = find_component_description(component_id)) {
    return desc->component_type;
  }
  return heif_cmpd_component_type_UNDEFINED;
}


std::vector<uint32_t> HeifPixelImage::get_component_ids_interleaved() const
{
  const ComponentStorage* comp = find_storage_for_channel(heif_channel_interleaved);
  assert(comp);
  return comp->m_component_ids;
}


Result<uint32_t> HeifPixelImage::add_component(uint32_t width, uint32_t height,
                                               uint16_t component_type,
                                               heif_component_datatype datatype, int bit_depth,
                                               const heif_security_limits* limits)
{
  heif_channel channel = map_uncompressed_component_to_channel(component_type);

  ComponentStorage plane;
  plane.m_channel = channel;

  if (Error err = plane.alloc(width, height, datatype, bit_depth, 1, limits, m_memory_handle)) {
    return {err};
  }

  register_component_descriptions(plane, std::vector<uint16_t>{component_type});
  uint32_t component_id = plane.m_component_ids.front();
  m_storage.push_back(std::move(plane));
  return component_id;
}


uint32_t HeifPixelImage::add_component_without_data(uint16_t component_type)
{
  uint32_t new_component_id = mint_component_id();

  ComponentDescription desc;
  desc.component_id = new_component_id;
  desc.channel = map_uncompressed_component_to_channel(component_type);
  desc.component_type = component_type;
  desc.has_data_plane = false;
  add_component_description(std::move(desc));

  return new_component_id;
}


void HeifPixelImage::clone_component_descriptions_from(const ImageDescription& src)
{
  set_component_descriptions(src.get_component_descriptions(),
                             src.peek_next_component_id());
}


void HeifPixelImage::apply_descriptions_from(const ImageDescription& src)
{
  const auto& src_descs = src.get_component_descriptions();
  if (src_descs.empty()) {
    return; // nothing to apply (e.g. grid/iden ImageItem with no description)
  }

  // Skip when this image's descriptions already match src's exactly. This
  // is the unci decode path: the decoder used clone_component_descriptions_from
  // (item) so the full description list (including any cpat reference-only
  // entries with has_data_plane=false) was copied verbatim. Comparing the
  // full lists also handles multiple planes that share a channel
  // (e.g. unci multi-component-of-same-type), which the channel-keyed remap
  // below can't represent.
  const auto& my_descs = get_component_descriptions();
  if (my_descs.size() == src_descs.size()) {
    bool already_aligned = true;
    for (size_t i = 0; i < src_descs.size(); i++) {
      if (my_descs[i].component_id != src_descs[i].component_id ||
          my_descs[i].channel != src_descs[i].channel) {
        already_aligned = false;
        break;
      }
    }
    if (already_aligned) {
      return;
    }
  }

  // Snapshot pre-remap descriptions keyed by channel (for any "extra"
  // channels not in src that we need to keep, like alpha-from-aux).
  std::map<heif_channel, ComponentDescription> auto_minted_by_channel;
  for (const auto& d : my_descs) {
    auto_minted_by_channel[d.channel] = d;
  }

  // Build a channel -> actual-plane-dimensions map. For tile decodes the
  // src description carries full-image dims, but the decoded plane was
  // allocated at tile size; the description we publish should match what
  // the buffer actually contains.
  std::map<heif_channel, std::pair<uint32_t, uint32_t>> plane_dims_by_channel;
  for (const auto& plane : m_storage) {
    plane_dims_by_channel[plane.m_channel] = {plane.m_width, plane.m_height};
  }

  // Build the new component list from src's data-plane descriptions and a
  // channel -> src-id map.
  std::vector<ComponentDescription> new_components;
  std::map<heif_channel, uint32_t> src_id_by_channel;
  for (const auto& d : src_descs) {
    if (d.has_data_plane) {
      ComponentDescription copy = d;
      auto it = plane_dims_by_channel.find(d.channel);
      if (it != plane_dims_by_channel.end()) {
        copy.width = it->second.first;
        copy.height = it->second.second;
      }
      new_components.push_back(copy);
      src_id_by_channel[d.channel] = d.component_id;
    }
  }

  // Compute a starting id for any extras (above src's high-water mark).
  uint32_t next_id = src.peek_next_component_id();
  for (const auto& d : new_components) {
    if (d.component_id >= next_id) next_id = d.component_id + 1;
  }

  // Remap each plane's m_component_ids by channel match against src; for
  // channels not in src, re-add the auto-minted description with a fresh id.
  for (auto& plane : m_storage) {
    if (plane.m_component_ids.empty()) continue;

    heif_channel ch = plane.m_channel;
    auto src_it = src_id_by_channel.find(ch);
    if (src_it != src_id_by_channel.end()) {
      plane.m_component_ids.assign(1, src_it->second);
    } else {
      auto auto_it = auto_minted_by_channel.find(ch);
      if (auto_it != auto_minted_by_channel.end()) {
        ComponentDescription extra = auto_it->second;
        extra.component_id = next_id++;
        new_components.push_back(extra);
        plane.m_component_ids.assign(1, extra.component_id);
      }
    }
  }

  set_component_descriptions(std::move(new_components), next_id);
}


Error HeifPixelImage::allocate_buffer_for_component(uint32_t component_id,
                                                    const heif_security_limits* limits)
{
  auto* desc = find_component_description(component_id);
  if (!desc) {
    return {heif_error_Usage_error,
            heif_suberror_Invalid_parameter_value,
            "allocate_buffer_for_component: unknown component id"};
  }
  if (!desc->has_data_plane) {
    return Error::Ok; // reference component (e.g. cpat); no buffer needed
  }

  ComponentStorage plane;
  plane.m_channel = desc->channel;
  plane.m_component_ids = std::vector{component_id};
  if (Error err = plane.alloc(desc->width, desc->height,
                              desc->datatype, desc->bit_depth,
                              1, limits, m_memory_handle)) {
    return err;
  }
  m_storage.push_back(plane);
  return Error::Ok;
}


#if 0
Result<uint32_t> HeifPixelImage::add_component_for_index(uint32_t component_index,
                                                          uint32_t width, uint32_t height,
                                                          heif_component_datatype datatype, int bit_depth,
                                                          const heif_security_limits* limits)
{
  if (component_index >= m_cmpd_component_types.size()) {
    return Error{heif_error_Usage_error, heif_suberror_Invalid_parameter_value,
                 "component_index out of range of cmpd table"};
  }

  uint16_t component_type = m_cmpd_component_types[component_index];

  ComponentStorage plane;
  plane.m_channel = map_uncompressed_component_to_channel(component_type);
  plane.m_component_index = std::vector{component_index};
  if (Error err = plane.alloc(width, height, datatype, bit_depth, 1, limits, m_memory_handle)) {
    return err;
  }

  m_storage.push_back(plane);
  return component_index;
}
#endif


std::vector<uint32_t> HeifPixelImage::get_used_component_ids() const
{
  const auto& descs = get_component_descriptions();
  std::vector<uint32_t> indices;
  indices.reserve(descs.size());

  for (const auto& desc : descs) {
    indices.push_back(desc.component_id);
  }

  return indices;
}


std::vector<uint32_t> HeifPixelImage::get_used_planar_component_ids() const
{
  std::vector<uint32_t> indices;

  for (const auto& plane : m_storage) {
    if (plane.m_component_ids.size() == 1) {
      indices.push_back(plane.m_component_ids[0]);
    }
  }

  return indices;
}


uint8_t* HeifPixelImage::get_component(uint32_t component_id, size_t* out_stride)
{
  return get_component_memory<uint8_t>(component_id, out_stride);
}


const uint8_t* HeifPixelImage::get_component(uint32_t component_id, size_t* out_stride) const
{
  return get_component_memory<uint8_t>(component_id, out_stride);
}
