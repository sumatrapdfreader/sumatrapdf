/*
 * HEIF codec.
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

#include "overlay.h"
#include "context.h"
#include "file.h"
#include "color-conversion/colorconversion.h"
#include "security_limits.h"


template<typename I>
void writevec(uint8_t* data, size_t& idx, I value, int len)
{
  for (int i = 0; i < len; i++) {
    data[idx + i] = static_cast<uint8_t>((value >> (len - 1 - i) * 8) & 0xFF);
  }

  idx += len;
}


static int32_t readvec_signed(const std::vector<uint8_t>& data, int& ptr, int len)
{
  const uint32_t high_bit = UINT32_C(0x80) << ((len - 1) * 8);

  uint32_t val = 0;
  while (len--) {
    val <<= 8;
    val |= data[ptr++];
  }

  bool negative = (val & high_bit) != 0;

  if (negative) {
    return -static_cast<int32_t>((~val) & 0x7fffffff) -1;
  }
  else {
    return static_cast<int32_t>(val);
  }

  return val;
}


static uint32_t readvec(const std::vector<uint8_t>& data, int& ptr, int len)
{
  uint32_t val = 0;
  while (len--) {
    val <<= 8;
    val |= data[ptr++];
  }

  return val;
}


Error ImageOverlay::parse(size_t num_images, const std::vector<uint8_t>& data)
{
  Error eofError(heif_error_Invalid_input,
                 heif_suberror_Invalid_overlay_data,
                 "Overlay image data incomplete");

  if (data.size() < 2 + 4 * 2) {
    return eofError;
  }

  m_version = data[0];
  if (m_version != 0) {
    std::stringstream sstr;
    sstr << "Overlay image data version " << ((int) m_version) << " is not implemented yet";

    return {heif_error_Unsupported_feature,
            heif_suberror_Unsupported_data_version,
            sstr.str()};
  }

  m_flags = data[1];

  int field_len = ((m_flags & 1) ? 4 : 2);
  int ptr = 2;

  if (ptr + 4 * 2 + 2 * field_len + num_images * 2 * field_len > data.size()) {
    return eofError;
  }

  for (int i = 0; i < 4; i++) {
    uint16_t color = static_cast<uint16_t>(readvec(data, ptr, 2));
    m_background_color[i] = color;
  }

  m_width = readvec(data, ptr, field_len);
  m_height = readvec(data, ptr, field_len);

  if (m_width == 0 || m_height == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_Invalid_overlay_data,
            "Overlay image with zero width or height."};
  }

  m_offsets.resize(num_images);

  for (size_t i = 0; i < num_images; i++) {
    m_offsets[i].x = readvec_signed(data, ptr, field_len);
    m_offsets[i].y = readvec_signed(data, ptr, field_len);
  }

  return Error::Ok;
}


std::vector<uint8_t> ImageOverlay::write() const
{
  assert(m_version == 0);

  bool longFields = (m_width > 0xFFFF) || (m_height > 0xFFFF);
  for (const auto& img : m_offsets) {
    if (img.x > 0x7FFF || img.y > 0x7FFF || img.x < -32768 || img.y < -32768) {
      longFields = true;
      break;
    }
  }

  std::vector<uint8_t> data;

  data.resize(2 + 4 * 2 + (longFields ? 4 : 2) * (2 + m_offsets.size() * 2));

  size_t idx = 0;
  data[idx++] = m_version;
  data[idx++] = (longFields ? 1 : 0); // flags

  for (uint16_t color : m_background_color) {
    writevec(data.data(), idx, color, 2);
  }

  writevec(data.data(), idx, m_width, longFields ? 4 : 2);
  writevec(data.data(), idx, m_height, longFields ? 4 : 2);

  for (const auto& img : m_offsets) {
    writevec(data.data(), idx, img.x, longFields ? 4 : 2);
    writevec(data.data(), idx, img.y, longFields ? 4 : 2);
  }

  assert(idx == data.size());

  return data;
}


std::string ImageOverlay::dump() const
{
  std::stringstream sstr;

  sstr << "version: " << ((int) m_version) << "\n"
       << "flags: " << ((int) m_flags) << "\n"
       << "background color: " << m_background_color[0]
       << ";" << m_background_color[1]
       << ";" << m_background_color[2]
       << ";" << m_background_color[3] << "\n"
       << "canvas size: " << m_width << "x" << m_height << "\n"
       << "offsets: ";

  for (const ImageWithOffset& offset : m_offsets) {
    sstr << offset.x << ";" << offset.y << " ";
  }
  sstr << "\n";

  return sstr.str();
}


void ImageOverlay::get_background_color(uint16_t col[4]) const
{
  for (int i = 0; i < 4; i++) {
    col[i] = m_background_color[i];
  }
}


void ImageOverlay::get_offset(size_t image_index, int32_t* x, int32_t* y) const
{
  assert(image_index < m_offsets.size());
  assert(x && y);

  *x = m_offsets[image_index].x;
  *y = m_offsets[image_index].y;
}



ImageItem_Overlay::ImageItem_Overlay(HeifContext* ctx)
    : ImageItem(ctx)
{
}


ImageItem_Overlay::ImageItem_Overlay(HeifContext* ctx, heif_item_id id)
    : ImageItem(ctx, id)
{
}


Error ImageItem_Overlay::initialize_decoder()
{
  Error err = read_overlay_spec();
  if (err) {
    return err;
  }

  return Error::Ok;
}


Error ImageItem_Overlay::read_overlay_spec()
{
  auto heif_file = get_context()->get_heif_file();

  auto iref_box = heif_file->get_iref_box();

  if (!iref_box) {
    return {heif_error_Invalid_input,
            heif_suberror_No_iref_box,
            "No iref box available, but needed for iovl image"};
  }


  m_overlay_image_ids = iref_box->get_references(get_id(), fourcc("dimg"));

  /* TODO: probably, it is valid that an iovl image has no references ?

  if (image_references.empty()) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Missing_grid_images,
                 "'iovl' image with more than one reference image");
  }
  */


  auto overlayDataResult = heif_file->get_uncompressed_item_data(get_id());
  if (!overlayDataResult) {
    return overlayDataResult.error();
  }

  Error err = m_overlay_spec.parse(m_overlay_image_ids.size(), *overlayDataResult);
  if (err) {
    return err;
  }

  if (m_overlay_image_ids.size() != m_overlay_spec.get_num_offsets()) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Invalid_overlay_data,
                 "Number of image offsets does not match the number of image references");
  }

  return Error::Ok;
}


Result<std::shared_ptr<HeifPixelImage>> ImageItem_Overlay::decode_compressed_image(const heif_decoding_options& options,
                                                                                   bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                                   std::set<heif_item_id> processed_ids) const
{
  return decode_overlay_image(options, processed_ids);
}


Result<std::shared_ptr<HeifPixelImage>> ImageItem_Overlay::decode_overlay_image(const heif_decoding_options& options,
                                                                                std::set<heif_item_id> processed_ids) const
{
  if (processed_ids.contains(get_id())) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "'iref' has cyclic references"};
  }

  processed_ids.insert(get_id());


  std::shared_ptr<HeifPixelImage> img;

  uint32_t w = m_overlay_spec.get_canvas_width();
  uint32_t h = m_overlay_spec.get_canvas_height();

  Error err = check_for_valid_image_size(get_context()->get_security_limits(), w, h);
  if (err) {
    return err;
  }

  // TODO: seems we always have to compose this in RGB since the background color is an RGB value
  img = std::make_shared<HeifPixelImage>();
  img->create(w, h,
              heif_colorspace_RGB,
              heif_chroma_444);
  if (auto error = img->add_plane(heif_channel_R, w, h, 8, get_context()->get_security_limits())) { // TODO: other bit depths
    return error;
  }
  if (auto error = img->add_plane(heif_channel_G, w, h, 8, get_context()->get_security_limits())) { // TODO: other bit depths
    return error;
  }
  if (auto error = img->add_plane(heif_channel_B, w, h, 8, get_context()->get_security_limits())) { // TODO: other bit depths
    return error;
  }

  uint16_t bkg_color[4];
  m_overlay_spec.get_background_color(bkg_color);

  err = img->fill_RGB_16bit(bkg_color[0], bkg_color[1], bkg_color[2], bkg_color[3]);
  if (err) {
    return err;
  }

  for (size_t i = 0; i < m_overlay_image_ids.size(); i++) {

    // detect if 'iovl' is referencing itself

    if (m_overlay_image_ids[i] == get_id()) {
      return Error{heif_error_Invalid_input,
                   heif_suberror_Unspecified,
                   "Self-reference in 'iovl' image item."};
    }

    auto imgItem = get_context()->get_image(m_overlay_image_ids[i], true);
    if (!imgItem) {
      return Error(heif_error_Invalid_input, heif_suberror_Nonexisting_item_referenced, "'iovl' image references a non-existing item.");
    }
    if (auto error = imgItem->get_item_error()) {
      return error;
    }

    auto decodeResult = imgItem->decode_image(options, false, 0,0, processed_ids);
    if (!decodeResult) {
      return decodeResult.error();
    }

    std::shared_ptr<HeifPixelImage> overlay_img = *decodeResult;


    // process overlay in RGB space

    if (overlay_img->get_colorspace() != heif_colorspace_RGB ||
        overlay_img->get_chroma_format() != heif_chroma_444) {
      auto overlay_img_result = convert_colorspace(overlay_img, heif_colorspace_RGB, heif_chroma_444,
                                                   nclx_profile::undefined(),
                                                   0, options.color_conversion_options, options.color_conversion_options_ext,
                                                   get_context()->get_security_limits());
      if (!overlay_img_result) {
        return overlay_img_result.error();
      }
      else {
        overlay_img = *overlay_img_result;
      }
    }

    int32_t dx, dy;
    m_overlay_spec.get_offset(i, &dx, &dy);

    err = img->overlay(overlay_img, dx, dy);
    if (err) {
      if (err.error_code == heif_error_Invalid_input &&
          err.sub_error_code == heif_suberror_Overlay_image_outside_of_canvas) {
        // NOP, ignore this error
      }
      else {
        return err;
      }
    }
  }

  return img;
}


int ImageItem_Overlay::get_luma_bits_per_pixel() const
{
  heif_item_id child;
  Error err = get_context()->get_id_of_non_virtual_child_image(get_id(), child);
  if (err) {
    return -1;
  }

  auto image = get_context()->get_image(child, true);
  return image->get_luma_bits_per_pixel();
}


int ImageItem_Overlay::get_chroma_bits_per_pixel() const
{
  heif_item_id child;
  Error err = get_context()->get_id_of_non_virtual_child_image(get_id(), child);
  if (err) {
    return -1;
  }

  auto image = get_context()->get_image(child, true);
  return image->get_chroma_bits_per_pixel();
}


Error ImageItem_Overlay::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  *out_colorspace = heif_colorspace_RGB;
  *out_chroma = heif_chroma_444;

  return Error::Ok;
}


Result<std::shared_ptr<ImageItem_Overlay>> ImageItem_Overlay::add_new_overlay_item(HeifContext* ctx, const ImageOverlay& overlayspec)
{
  if (overlayspec.get_num_offsets() > 0xFFFF) {
    return Error{heif_error_Usage_error,
                 heif_suberror_Unspecified,
                 "Too many overlay images (maximum: 65535)"};
  }

  std::vector<heif_item_id> ref_ids;

  auto file = ctx->get_heif_file();

  for (const auto& overlay : overlayspec.get_overlay_stack()) {
    file->get_infe_box(overlay.image_id)->set_hidden_item(true); // only show the full overlay
    ref_ids.push_back(overlay.image_id);
  }


  // Create ImageOverlay

  std::vector<uint8_t> iovl_data = overlayspec.write();

  // Create IOVL Item

  heif_item_id iovl_id = file->add_new_image(fourcc("iovl"));
  std::shared_ptr<ImageItem_Overlay> iovl_image = std::make_shared<ImageItem_Overlay>(ctx, iovl_id);
  ctx->insert_image_item(iovl_id, iovl_image);
  const int construction_method = 1; // 0=mdat 1=idat
  file->append_iloc_data(iovl_id, iovl_data, construction_method);

  // Connect images to overlay
  file->add_iref_reference(iovl_id, fourcc("dimg"), ref_ids);

  // Add ISPE property
  auto ispe = std::make_shared<Box_ispe>();
  ispe->set_size(overlayspec.get_canvas_width(), overlayspec.get_canvas_height());
  iovl_image->add_property(ispe, false);

  // Add PIXI property (copy from first image) - According to MIAF, all images shall have the same color information.
  auto pixi = file->get_property_for_item<Box_pixi>(ref_ids[0]);
  iovl_image->add_property(pixi, true);

  // Set Brands
  //m_heif_file->set_brand(encoder->plugin->compression_format,
  //                       out_grid_image->is_miaf_compatible());

  return iovl_image;
}

heif_brand2 ImageItem_Overlay::get_compatible_brand() const
{
  if (m_overlay_image_ids.empty()) { return 0; }

  heif_item_id child_id = m_overlay_image_ids[0];
  auto child = get_context()->get_image(child_id, false);
  if (!child) { return 0; }

  return child->get_compatible_brand();
}
