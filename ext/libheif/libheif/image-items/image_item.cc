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

#include "image_item.h"
#include "mask_image.h"
#include "context.h"
#include "file.h"
#include "jpeg.h"
#include "jpeg2000.h"
#include "avif.h"
#include "avc.h"
#include "hevc.h"
#include "grid.h"
#include "overlay.h"
#include "iden.h"
#include "tiled.h"
#include "codecs/decoder.h"
#include "color-conversion/colorconversion.h"
#include "api_structs.h"
#include "plugin_registry.h"
#include "security_limits.h"

#include <limits>
#include <cassert>
#include <cstring>
//#include <ranges>

#if WITH_UNCOMPRESSED_CODEC
#include "image-items/unc_image.h"
#endif


ImageItem::ImageItem(HeifContext* context)
    : m_heif_context(context)
{
  memset(&m_depth_representation_info, 0, sizeof(m_depth_representation_info));
}


ImageItem::ImageItem(HeifContext* context, heif_item_id id)
    : ImageItem(context)
{
  m_id = id;
}


bool ImageItem::is_property_essential(const std::shared_ptr<Box>& property) const
{
  if (property->get_short_type() == fourcc("ispe")) {
    return is_ispe_essential();
  }
  else {
    return property->is_essential();
  }
}


std::shared_ptr<HeifFile> ImageItem::get_file() const
{
  return m_heif_context->get_heif_file();
}


heif_property_id ImageItem::add_property(std::shared_ptr<Box> property, bool essential)
{
  if (!property) {
    return 0;
  }

  // TODO: is this correct? What happens when add_property does deduplicate the property?
  m_properties.push_back(property);
  return get_file()->add_property(get_id(), property, essential);
}


heif_property_id ImageItem::add_property_without_deduplication(std::shared_ptr<Box> property, bool essential)
{
  if (!property) {
    return 0;
  }

  m_properties.push_back(property);
  return get_file()->add_property_without_deduplication(get_id(), property, essential);
}


heif_compression_format ImageItem::compression_format_from_fourcc_infe_type(uint32_t type)
{
  switch (type) {
    case fourcc("jpeg"):
      return heif_compression_JPEG;
    case fourcc("hvc1"):
      return heif_compression_HEVC;
    case fourcc("av01"):
      return heif_compression_AV1;
    case fourcc("vvc1"):
      return heif_compression_VVC;
    case fourcc("j2k1"):
      return heif_compression_JPEG2000;
    case fourcc("unci"):
      return heif_compression_uncompressed;
    case fourcc("mski"):
      return heif_compression_mask;
    default:
      return heif_compression_undefined;
  }
}

uint32_t ImageItem::compression_format_to_fourcc_infe_type(heif_compression_format format)
{
  switch (format) {
    case heif_compression_JPEG:
      return fourcc("jpeg");
    case heif_compression_HEVC:
      return fourcc("hvc1");
    case heif_compression_AV1:
      return fourcc("av01");
    case heif_compression_VVC:
      return fourcc("vvc1");
    case heif_compression_JPEG2000:
      return fourcc("j2k1");
    case heif_compression_uncompressed:
      return fourcc("unci");
    case heif_compression_mask:
      return fourcc("mski");
    default:
      return 0;
  }
}


std::shared_ptr<ImageItem> ImageItem::alloc_for_infe_box(HeifContext* ctx, const std::shared_ptr<Box_infe>& infe)
{
  uint32_t item_type = infe->get_item_type_4cc();
  heif_item_id id = infe->get_item_ID();

  if (item_type == fourcc("jpeg") ||
      (item_type == fourcc("mime") && infe->get_content_type() == "image/jpeg")) {
    return std::make_shared<ImageItem_JPEG>(ctx, id);
  }
  else if (item_type == fourcc("hvc1")) {
    return std::make_shared<ImageItem_HEVC>(ctx, id);
  }
  else if (item_type == fourcc("av01")) {
    return std::make_shared<ImageItem_AVIF>(ctx, id);
  }
  else if (item_type == fourcc("vvc1")) {
    return std::make_shared<ImageItem_VVC>(ctx, id);
  }
  else if (item_type == fourcc("avc1")) {
    return std::make_shared<ImageItem_AVC>(ctx, id);
  }
  else if (item_type == fourcc("unci")) {
#if WITH_UNCOMPRESSED_CODEC
    return std::make_shared<ImageItem_uncompressed>(ctx, id);
#else
    // It is an image item type that we do not support. Thus, generate an ImageItem_Error.

    std::stringstream sstr;
    sstr << "Image item of type '" << fourcc_to_string(item_type) << "' is not supported.";
    Error err{ heif_error_Unsupported_feature, heif_suberror_Unsupported_image_type, sstr.str() };
    return std::make_shared<ImageItem_Error>(item_type, id, err);
#endif
  }
  else if (item_type == fourcc("j2k1")) {
    return std::make_shared<ImageItem_JPEG2000>(ctx, id);
  }
  else if (item_type == fourcc("lhv1")) {
    return std::make_shared<ImageItem_Error>(item_type, id,
                                             Error{heif_error_Unsupported_feature,
                                                   heif_suberror_Unsupported_image_type,
                                                   "Layered HEVC images (lhv1) are not supported yet"});
  }
  else if (item_type == fourcc("mski")) {
    return std::make_shared<ImageItem_mask>(ctx, id);
  }
  else if (item_type == fourcc("grid")) {
    return std::make_shared<ImageItem_Grid>(ctx, id);
  }
  else if (item_type == fourcc("iovl")) {
    return std::make_shared<ImageItem_Overlay>(ctx, id);
  }
  else if (item_type == fourcc("iden")) {
    return std::make_shared<ImageItem_iden>(ctx, id);
  }
  else if (item_type == fourcc("tili")) {
    return std::make_shared<ImageItem_Tiled>(ctx, id);
  }
  else {
    // This item has an unknown type. It could be an image or anything else.
    // Do not process the item.

    return nullptr;
  }
}


std::shared_ptr<ImageItem> ImageItem::alloc_for_compression_format(HeifContext* ctx, heif_compression_format format)
{
  switch (format) {
    case heif_compression_JPEG:
      return std::make_shared<ImageItem_JPEG>(ctx);
    case heif_compression_HEVC:
      return std::make_shared<ImageItem_HEVC>(ctx);
    case heif_compression_AV1:
      return std::make_shared<ImageItem_AVIF>(ctx);
    case heif_compression_VVC:
      return std::make_shared<ImageItem_VVC>(ctx);
    case heif_compression_AVC:
      return std::make_shared<ImageItem_AVC>(ctx);
#if WITH_UNCOMPRESSED_CODEC
    case heif_compression_uncompressed:
      return std::make_shared<ImageItem_uncompressed>(ctx);
#endif
    case heif_compression_JPEG2000:
    case heif_compression_HTJ2K:
      return std::make_shared<ImageItem_JPEG2000>(ctx);
    case heif_compression_mask:
      return std::make_shared<ImageItem_mask>(ctx);
    default:
      assert(false);
      return nullptr;
  }
}


Result<Encoder::CodedImageData> ImageItem::encode_to_bitstream_and_boxes(const std::shared_ptr<HeifPixelImage>& image,
                                                                           heif_encoder* encoder,
                                                                           const heif_encoding_options& options,
                                                                           heif_image_input_class input_class)
{
  // === generate compressed image bitstream

  Result<Encoder::CodedImageData> encodeResult = encode(image, encoder, options, input_class);
  if (!encodeResult) {
    return encodeResult;
  }

  Encoder::CodedImageData& codedImage = *encodeResult;

  // === generate properties

  // --- choose which color profile to put into 'colr' box

  auto colr_boxes = add_color_profile(image, options, input_class, options.output_nclx_profile);
  codedImage.properties.insert(codedImage.properties.end(),
                               colr_boxes.begin(),
                               colr_boxes.end());


  // --- ispe
  // Note: 'ispe' must come before the transformation properties

  uint32_t input_width, input_height;
  input_width = image->get_width();
  input_height = image->get_height();

  // --- get the real size of the encoded image

  // highest priority: codedImageData
  uint32_t encoded_width = codedImage.encoded_image_width;
  uint32_t encoded_height = codedImage.encoded_image_height;

  // second priority: query plugin API
  if (encoded_width == 0 &&
      encoder->plugin->plugin_api_version >= 3 &&
      encoder->plugin->query_encoded_size != nullptr) {

    encoder->plugin->query_encoded_size(encoder->encoder,
                                        input_width, input_height,
                                        &encoded_width,
                                        &encoded_height);
  }
  else if (encoded_width == 0) {
    // fallback priority: use input size
    encoded_width = input_width;
    encoded_height = input_height;
  }

  auto ispe = std::make_shared<Box_ispe>();
  ispe->set_size(encoded_width, encoded_height);
  ispe->set_is_essential(is_ispe_essential());
  codedImage.properties.push_back(ispe);


  // --- clap (if needed)

  if (input_width != encoded_width ||
      input_height != encoded_height) {

    auto clap = std::make_shared<Box_clap>();
    clap->set(input_width, input_height, encoded_width, encoded_height);
    codedImage.properties.push_back(clap);
  }



  // --- add common metadata properties (pixi, ...)

  auto colorspace = image->get_colorspace();
  auto chroma = image->get_chroma_format();


  // --- write PIXI property

  std::shared_ptr<Box_pixi> pixi = std::make_shared<Box_pixi>();
  if (colorspace == heif_colorspace_monochrome) {
    pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_Y));
  }
  else if (colorspace == heif_colorspace_YCbCr) {
    pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_Y));
    pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_Cb));
    pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_Cr));
  }
  else if (colorspace == heif_colorspace_RGB) {
    if (chroma == heif_chroma_444) {
      pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_R));
      pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_G));
      pixi->add_channel_bits(image->get_bits_per_pixel(heif_channel_B));
    }
    else if (chroma == heif_chroma_interleaved_RGB ||
             chroma == heif_chroma_interleaved_RGBA ||
             chroma == heif_chroma_interleaved_RRGGBB_LE ||
             chroma == heif_chroma_interleaved_RRGGBB_BE ||
             chroma == heif_chroma_interleaved_RRGGBBAA_LE ||
             chroma == heif_chroma_interleaved_RRGGBBAA_BE) {
      uint8_t bpp = image->get_bits_per_pixel(heif_channel_interleaved);
      pixi->add_channel_bits(bpp);
      pixi->add_channel_bits(bpp);
      pixi->add_channel_bits(bpp);
    }
  }
  codedImage.properties.push_back(pixi);

  // --- generate properties for image extra data

  // copy over ImageExtraData into image item
  *static_cast<ImageExtraData*>(this) = static_cast<ImageExtraData>(*image);

  auto extra_data_properties = image->generate_property_boxes(false);
  codedImage.properties.insert(codedImage.properties.end(),
                               extra_data_properties.begin(),
                               extra_data_properties.end());

  return encodeResult;
}


Error ImageItem::encode_to_item(HeifContext* ctx,
                                const std::shared_ptr<HeifPixelImage>& image,
                                heif_encoder* encoder,
                                const heif_encoding_options& options,
                                heif_image_input_class input_class)
{
  uint32_t input_width = image->get_width();
  uint32_t input_height = image->get_height();

  set_size(input_width, input_height);


  // compress image and assign data to item

  Result<Encoder::CodedImageData> codingResult = encode_to_bitstream_and_boxes(image, encoder, options, input_class);
  if (!codingResult) {
    return codingResult.error();
  }

  Encoder::CodedImageData& codedImage = *codingResult;

  auto infe_box = ctx->get_heif_file()->add_new_infe_box(get_infe_type());
  heif_item_id image_id = infe_box->get_item_ID();
  set_id(image_id);

  ctx->get_heif_file()->append_iloc_data(image_id, codedImage.bitstream, 0);


  // set item properties

  for (auto& propertyBox : codingResult->properties) {
    bool essential = is_property_essential(propertyBox);

    // TODO: can we simply use add_property() ?
    int index = ctx->get_heif_file()->get_ipco_box()->find_or_append_child_box(propertyBox);
    ctx->get_heif_file()->get_ipma_box()->add_property_for_item_ID(image_id, Box_ipma::PropertyAssociation{essential,
                                                                                                           uint16_t(index + 1)});
  }


  // MIAF 7.3.6.7
  // This is according to MIAF without Amd2. With Amd2, the restriction has been lifted and the image is MIAF compatible.
  // However, since AVIF is based on MIAF, the whole image would be invalid in that case.

  // We might remove this code at a later point in time when MIAF Amd2 is in wide use.

  if (encoder->plugin->compression_format != heif_compression_AV1 &&
      image->get_colorspace() == heif_colorspace_YCbCr) {
    if (!is_integer_multiple_of_chroma_size(image->get_width(),
                                            image->get_height(),
                                            image->get_chroma_format())) {
      mark_not_miaf_compatible();
    }
  }

  // TODO: move this into encode_to_bistream_and_boxes()
  ctx->get_heif_file()->add_orientation_properties(image_id, options.image_orientation);

  return Error::Ok;
}

bool ImageItem::has_ispe_resolution() const
{
  return get_property<Box_ispe>() != nullptr;
}

uint32_t ImageItem::get_ispe_width() const
{
  auto ispe = get_property<Box_ispe>();
  if (!ispe) {
    return 0;
  }
  else {
    return ispe->get_width();
  }
}


uint32_t ImageItem::get_ispe_height() const
{
  auto ispe = get_property<Box_ispe>();
  if (!ispe) {
    return 0;
  }
  else {
    return ispe->get_height();
  }
}


void ImageItem::get_tile_size(uint32_t& w, uint32_t& h) const
{
  w = get_width();
  h = get_height();
}


Error ImageItem::postprocess_coded_image_colorspace(heif_colorspace* inout_colorspace, heif_chroma* inout_chroma) const
{
#if 0
  auto pixi = m_heif_context->get_heif_file()->get_property<Box_pixi>(id);
  if (pixi && pixi->get_num_channels() == 1) {
    *out_colorspace = heif_colorspace_monochrome;
    *out_chroma = heif_chroma_monochrome;
  }
#endif

  if (*inout_colorspace == heif_colorspace_YCbCr) {
    auto nclx = get_color_profile_nclx();
    if (nclx.get_matrix_coefficients() == 0) {
      *inout_colorspace = heif_colorspace_RGB;
      *inout_chroma = heif_chroma_444; // TODO: this or keep the original chroma?
    }
  }

  return Error::Ok;
}


Error ImageItem::get_coded_image_colorspace(heif_colorspace* out_colorspace, heif_chroma* out_chroma) const
{
  auto decoderResult = get_decoder();
  if (!decoderResult) {
    return decoderResult.error();
  }

  auto decoder = *decoderResult;

  Error err = decoder->get_coded_image_colorspace(out_colorspace, out_chroma);
  if (err) {
    return err;
  }

  postprocess_coded_image_colorspace(out_colorspace, out_chroma);

  return Error::Ok;
}


int ImageItem::get_luma_bits_per_pixel() const
{
  auto decoderResult = get_decoder();
  if (!decoderResult) {
    return decoderResult.error();
  }

  auto decoder = *decoderResult;

  return decoder->get_luma_bits_per_pixel();
}


int ImageItem::get_chroma_bits_per_pixel() const
{
  auto decoderResult = get_decoder();
  if (!decoderResult) {
    return decoderResult.error();
  }

  auto decoder = *decoderResult;

  return decoder->get_chroma_bits_per_pixel();
}


Result<Encoder::CodedImageData> ImageItem::encode(const std::shared_ptr<HeifPixelImage>& image,
                                                  heif_encoder* h_encoder,
                                                  const heif_encoding_options& options,
                                                  heif_image_input_class input_class)
{
  auto encoder = get_encoder();
  return encoder->encode(image, h_encoder, options, input_class);
}


std::vector<std::shared_ptr<Box_colr> >
ImageItem::add_color_profile(const std::shared_ptr<HeifPixelImage>& image,
                             const heif_encoding_options& options,
                             heif_image_input_class input_class,
                             const heif_color_profile_nclx* target_heif_nclx)
{
  std::vector<std::shared_ptr<Box_colr> > colr_boxes;

  if (input_class == heif_image_input_class_normal || input_class == heif_image_input_class_thumbnail) {
    auto icc_profile = image->get_color_profile_icc();
    if (icc_profile) {
      auto colr = std::make_shared<Box_colr>();
      colr->set_color_profile(icc_profile);
      colr_boxes.push_back(colr);
    }


    // save nclx profile

    bool save_nclx_profile = (options.output_nclx_profile != nullptr);

    // if there is an ICC profile, only save NCLX when we chose to save both profiles
    if (icc_profile && !(options.version >= 3 &&
                         options.save_two_colr_boxes_when_ICC_and_nclx_available)) {
      save_nclx_profile = false;
    }

    // we might have turned off nclx completely because macOS/iOS cannot read it
    if (options.version >= 4 && options.macOS_compatibility_workaround_no_nclx_profile) {
      save_nclx_profile = false;
    }

    if (save_nclx_profile) {
      auto target_nclx_profile = std::make_shared<color_profile_nclx>();
      target_nclx_profile->set_from_heif_color_profile_nclx(target_heif_nclx);

      auto colr = std::make_shared<Box_colr>();
      colr->set_color_profile(target_nclx_profile);
      colr_boxes.push_back(colr);
    }
  }

  return colr_boxes;
}


Error ImageItem::transform_requested_tile_position_to_original_tile_position(uint32_t& tile_x, uint32_t& tile_y) const
{
  Result<std::vector<std::shared_ptr<Box>>> propertiesResult = get_properties();
  if (!propertiesResult) {
    return propertiesResult.error();
  }

  heif_image_tiling tiling = get_heif_image_tiling();

  //for (auto& prop : std::ranges::reverse_view(propertiesResult.value)) {
  for (auto propIter = propertiesResult->rbegin(); propIter != propertiesResult->rend(); propIter++) {
    if (auto irot = std::dynamic_pointer_cast<Box_irot>(*propIter)) {
      switch (irot->get_rotation_ccw()) {
        case 90: {
          uint32_t tx0 = tiling.num_columns - 1 - tile_y;
          uint32_t ty0 = tile_x;
          tile_y = ty0;
          tile_x = tx0;
          break;
        }
        case 270: {
          uint32_t tx0 = tile_y;
          uint32_t ty0 = tiling.num_rows - 1 - tile_x;
          tile_y = ty0;
          tile_x = tx0;
          break;
        }
        case 180: {
          tile_x = tiling.num_columns - 1 - tile_x;
          tile_y = tiling.num_rows - 1 - tile_y;
          break;
        }
        case 0:
          break;
        default:
          assert(false);
          break;
      }
    }

    if (auto imir = std::dynamic_pointer_cast<Box_imir>(*propIter)) {
      switch (imir->get_mirror_direction()) {
        case heif_transform_mirror_direction_horizontal:
          tile_x = tiling.num_columns - 1 - tile_x;
          break;
        case heif_transform_mirror_direction_vertical:
          tile_y = tiling.num_rows - 1 - tile_y;
          break;
        default:
          assert(false);
          break;
      }
    }
  }

  return Error::Ok;
}


void ImageItem::set_clli(const heif_content_light_level& clli)
{
  ImageExtraData::set_clli(clli);
  add_property(get_clli_box(), false);
}


void ImageItem::set_mdcv(const heif_mastering_display_colour_volume& mdcv)
{
  ImageExtraData::set_mdcv(mdcv);
  add_property(get_mdcv_box(), false);
}


void ImageItem::set_pixel_ratio(uint32_t h, uint32_t v)
{
  ImageExtraData::set_pixel_ratio(h, v);
  add_property(get_pasp_box(), false);
}


void ImageItem::set_color_profile_nclx(const nclx_profile& profile)
{
  ImageExtraData::set_color_profile_nclx(profile);
  add_property(get_colr_box_nclx(), false);
}


void ImageItem::set_color_profile_icc(const std::shared_ptr<const color_profile_raw>& profile)
{
  ImageExtraData::set_color_profile_icc(profile);
  add_property(get_colr_box_icc(), false);
}


Result<std::shared_ptr<HeifPixelImage>> ImageItem::decode_image(const heif_decoding_options& options,
                                                                bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                std::set<heif_item_id> processed_ids) const
{
  // --- check whether image size (according to 'ispe') exceeds maximum

  if (!decode_tile_only) {
    auto ispe = get_property<Box_ispe>();
    if (ispe) {
      Error err = check_for_valid_image_size(get_context()->get_security_limits(), ispe->get_width(), ispe->get_height());
      if (err) {
        return err;
      }
    }
  }


  // --- transform tile position

  if (decode_tile_only && options.ignore_transformations == false) {
    if (Error error = transform_requested_tile_position_to_original_tile_position(tile_x0, tile_y0)) {
      return error;
    }
  }

  // --- decode image

  Result<std::shared_ptr<HeifPixelImage>> decodingResult = decode_compressed_image(options, decode_tile_only, tile_x0, tile_y0, processed_ids);
  if (!decodingResult) {
    return decodingResult.error();
  }

  auto img = *decodingResult;
  if (!img) {
    // Can happen if missing tiled image is decoded in non-strict mode.
    return Error(heif_error_Decoder_plugin_error, heif_suberror_Unspecified);
  }

  std::shared_ptr<HeifFile> file = m_heif_context->get_heif_file();


  // --- apply image transformations

  Error error;

  if (options.ignore_transformations == false) {
    Result<std::vector<std::shared_ptr<Box>>> propertiesResult = get_properties();
    if (!propertiesResult) {
      return propertiesResult.error();
    }

    const std::vector<std::shared_ptr<Box>>& properties = *propertiesResult;

    for (const auto& property : properties) {
      if (auto rot = std::dynamic_pointer_cast<Box_irot>(property)) {
        auto rotateResult = img->rotate_ccw(rot->get_rotation_ccw(), m_heif_context->get_security_limits());
        if (!rotateResult) {
          return error;
        }

        img = *rotateResult;
      }


      if (auto mirror = std::dynamic_pointer_cast<Box_imir>(property)) {
        auto mirrorResult = img->mirror_inplace(mirror->get_mirror_direction(),
                                                get_context()->get_security_limits());
        if (!mirrorResult) {
          return error;
        }
        img = *mirrorResult;
      }


      if (!decode_tile_only) {
        // For tiles decoding, we do not process the 'clap' because this is handled by a shift of the tiling grid.

        if (auto clap = std::dynamic_pointer_cast<Box_clap>(property)) {
          std::shared_ptr<HeifPixelImage> clap_img;

          uint32_t img_width = img->get_width();
          uint32_t img_height = img->get_height();

          int left = clap->left_rounded(img_width);
          int right = clap->right_rounded(img_width);
          int top = clap->top_rounded(img_height);
          int bottom = clap->bottom_rounded(img_height);

          if (left < 0) { left = 0; }
          if (top < 0) { top = 0; }

          if ((uint32_t) right >= img_width) { right = img_width - 1; }
          if ((uint32_t) bottom >= img_height) { bottom = img_height - 1; }

          if (left > right ||
              top > bottom) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Invalid_clean_aperture);
          }

          auto cropResult = img->crop(left, right, top, bottom, m_heif_context->get_security_limits());
          if (!cropResult) {
            return cropResult.error();
          }

          img = *cropResult;
        }
      }
    }
  }


  // --- add alpha channel, if available

  // TODO: this if statement is probably wrong. When we have a tiled image with alpha
  // channel, then the alpha images should be associated with their respective tiles.
  // However, the tile images are not part of the m_all_images list.
  // Fix this, when we have a test image available.

  std::shared_ptr<ImageItem> alpha_image = get_alpha_channel();
  if (alpha_image) {
    if (alpha_image->get_item_error()) {
      return alpha_image->get_item_error();
    }

    auto alphaDecodingResult = alpha_image->decode_image(options, decode_tile_only, tile_x0, tile_y0, processed_ids);
    if (!alphaDecodingResult) {
      return alphaDecodingResult.error();
    }

    std::shared_ptr<HeifPixelImage> alpha = *alphaDecodingResult;

    // TODO: check that sizes are the same and that we have an Y channel
    // BUT: is there any indication in the standard that the alpha channel should have the same size?

    // TODO: convert in case alpha is decoded as RGB interleaved

    heif_channel channel;
    switch (alpha->get_colorspace()) {
      case heif_colorspace_YCbCr:
      case heif_colorspace_monochrome:
        channel = heif_channel_Y;
        break;
      case heif_colorspace_RGB:
        channel = heif_channel_R;
        break;
      case heif_colorspace_undefined:
      default:
        return Error(heif_error_Invalid_input,
                     heif_suberror_Unsupported_color_conversion);
    }


    // TODO: we should include a decoding option to control whether libheif should automatically scale the alpha channel, and if so, which scaling filter (enum: Off, NN, Bilinear, ...).
    //       It might also be that a specific output format implies that alpha is scaled (RGBA32). That would favor an enum for the scaling filter option + a bool to switch auto-filtering on.
    //       But we can only do this when libheif itself doesn't assume anymore that the alpha channel has the same resolution.

    if ((alpha_image->get_width() != img->get_width()) || (alpha_image->get_height() != img->get_height())) {
      std::shared_ptr<HeifPixelImage> scaled_alpha;
      Error err = alpha->scale_nearest_neighbor(scaled_alpha, img->get_width(), img->get_height(), m_heif_context->get_security_limits());
      if (err) {
        return err;
      }
      alpha = std::move(scaled_alpha);
    }
    img->transfer_plane_from_image_as(alpha, channel, heif_channel_Alpha);

    if (is_premultiplied_alpha()) {
      img->set_premultiplied_alpha(true);
    }
  }


  // --- set color profile

  // If there is an NCLX profile in the HEIF/AVIF metadata, use this for the color conversion.
  // Otherwise, use the profile that is stored in the image stream itself and then set the
  // (non-NCLX) profile later.
  auto nclx = get_color_profile_nclx();
  if (!nclx.is_undefined()) {
    img->set_color_profile_nclx(nclx);
  }

  auto icc = get_color_profile_icc();
  if (icc) {
    img->set_color_profile_icc(icc);
  }


  // --- attach metadata to image

  {
    auto ipco_box = file->get_ipco_box();
    auto ipma_box = file->get_ipma_box();

    // CLLI

    auto clli = get_property<Box_clli>();
    if (clli) {
      img->set_clli(clli->clli);
    }

    // MDCV

    auto mdcv = get_property<Box_mdcv>();
    if (mdcv) {
      img->set_mdcv(mdcv->mdcv);
    }

    // PASP

    auto pasp = get_property<Box_pasp>();
    if (pasp) {
      img->set_pixel_ratio(pasp->hSpacing, pasp->vSpacing);
    }

    // TAI

    auto itai = get_property<Box_itai>();
    if (itai) {
      img->set_tai_timestamp(itai->get_tai_timestamp_packet());
    }

    // GIMI content ID

    auto gimi_content_id = get_property<Box_gimi_content_id>();
    if (gimi_content_id) {
      img->set_gimi_sample_content_id(gimi_content_id->get_content_id());
    }
  }

  return img;
}

#if 0
Result<std::vector<uint8_t>> ImageItem::read_bitstream_configuration_data_override(heif_item_id itemId, heif_compression_format format) const
{
  auto item_codec = ImageItem::alloc_for_compression_format(const_cast<HeifContext*>(get_context()), format);
  assert(item_codec);

  Error err = item_codec->init_decoder_from_item(itemId);
  if (err) {
    return err;
  }

  return item_codec->read_bitstream_configuration_data(itemId);
}
#endif

Result<std::shared_ptr<HeifPixelImage>> ImageItem::decode_compressed_image(const heif_decoding_options& options,
                                                                           bool decode_tile_only, uint32_t tile_x0, uint32_t tile_y0,
                                                                           std::set<heif_item_id> processed_ids) const
{
  if (processed_ids.contains(m_id)) {
    return Error{heif_error_Invalid_input,
                 heif_suberror_Unspecified,
                 "'iref' has cyclic references"};
  }

  processed_ids.insert(m_id);


  DataExtent extent;
  extent.set_from_image_item(get_file(), get_id());

  auto decoderResult = get_decoder();
  if (!decoderResult) {
    return decoderResult.error();
  }

  auto decoder = *decoderResult;

  decoder->set_data_extent(std::move(extent));

  return decoder->decode_single_frame_from_compressed_data(options,
                                                           get_context()->get_security_limits());
}


heif_image_tiling ImageItem::get_heif_image_tiling() const
{
  // --- Return a dummy tiling consisting of only a single tile for the whole image

  heif_image_tiling tiling{};

  tiling.version = 1;
  tiling.num_columns = 1;
  tiling.num_rows = 1;

  tiling.tile_width = m_width;
  tiling.tile_height = m_height;
  tiling.image_width = m_width;
  tiling.image_height = m_height;

  tiling.top_offset = 0;
  tiling.left_offset = 0;
  tiling.number_of_extra_dimensions = 0;

  for (uint32_t& s : tiling.extra_dimension_size) {
    s = 0;
  }

  return tiling;
}


Result<std::vector<std::shared_ptr<Box>>> ImageItem::get_properties() const
{
  std::vector<std::shared_ptr<Box>> properties;
  auto ipco_box = get_file()->get_ipco_box();
  auto ipma_box = get_file()->get_ipma_box();
  Error error = ipco_box->get_properties_for_item_ID(m_id, ipma_box, properties);
  if (error) {
    return error;
  }

  return properties;
}


bool ImageItem::has_essential_property_other_than(const std::set<uint32_t>& props) const
{
  Result<std::vector<std::shared_ptr<Box>>> propertiesResult = get_properties();
  if (!propertiesResult) {
    return false;
  }

  for (const auto& property : *propertiesResult) {
    if (is_property_essential(property) &&
        props.find(property->get_short_type()) == props.end()) {
      return true;
    }
  }

  return false;
}


Error ImageItem::process_image_transformations_on_tiling(heif_image_tiling& tiling) const
{
  Result<std::vector<std::shared_ptr<Box>>> propertiesResult = get_properties();
  if (!propertiesResult) {
    return propertiesResult.error();
  }

  const std::vector<std::shared_ptr<Box>>& properties = *propertiesResult;

  uint32_t left_excess = 0;
  uint32_t top_excess = 0;
  uint32_t right_excess;
  uint32_t bottom_excess;

  // Prevent divide by zero.

  if (tiling.tile_width != 0 && tiling.tile_height != 0) {
    right_excess = tiling.image_width % tiling.tile_width;
    bottom_excess = tiling.image_height % tiling.tile_height;
  }
  else {
    right_excess = 0;
    bottom_excess = 0;
  }


  for (const auto& property : properties) {

    // --- rotation

    if (auto rot = std::dynamic_pointer_cast<Box_irot>(property)) {
      int angle = rot->get_rotation_ccw();
      if (angle == 90 || angle == 270) {
        std::swap(tiling.tile_width, tiling.tile_height);
        std::swap(tiling.image_width, tiling.image_height);
        std::swap(tiling.num_rows, tiling.num_columns);
      }

      switch (angle) {
        case 0:
          break;
        case 180:
          std::swap(left_excess, right_excess);
          std::swap(top_excess, bottom_excess);
          break;
        case 90: {
          uint32_t old_top_excess = top_excess;
          top_excess = right_excess;
          right_excess = bottom_excess;
          bottom_excess = left_excess;
          left_excess = old_top_excess;
          break;
        }
        case 270: {
          uint32_t old_top_excess = top_excess;
          top_excess = left_excess;
          left_excess = bottom_excess;
          bottom_excess = right_excess;
          right_excess = old_top_excess;
          break;
        }
        default:
          assert(false);
          break;
      }
    }

    // --- mirror

    if (auto mirror = std::dynamic_pointer_cast<Box_imir>(property)) {
      switch (mirror->get_mirror_direction()) {
        case heif_transform_mirror_direction_horizontal:
          std::swap(left_excess, right_excess);
          break;
        case heif_transform_mirror_direction_vertical:
          std::swap(top_excess, bottom_excess);
          break;
        default:
          assert(false);
          break;
      }
    }

    // --- crop

    if (auto clap = std::dynamic_pointer_cast<Box_clap>(property)) {
      std::shared_ptr<HeifPixelImage> clap_img;

      int left = clap->left_rounded(tiling.image_width);
      int right = clap->right_rounded(tiling.image_width);
      int top = clap->top_rounded(tiling.image_height);
      int bottom = clap->bottom_rounded(tiling.image_height);

      if (left < 0) { left = 0; }
      if (top < 0) { top = 0; }

      if ((uint32_t)right >= tiling.image_width) { right = tiling.image_width - 1; }
      if ((uint32_t)bottom >= tiling.image_height) { bottom = tiling.image_height - 1; }

      if (left > right ||
          top > bottom) {
        return {heif_error_Invalid_input,
                heif_suberror_Invalid_clean_aperture};
      }

      left_excess += left;
      right_excess += right;
      top_excess += top;
      bottom_excess += bottom;
    }
  }

  tiling.left_offset = left_excess;
  tiling.top_offset = top_excess;

  return Error::Ok;
}
