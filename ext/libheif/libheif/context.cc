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

#include "box.h"
#include "error.h"
#include "libheif/heif.h"
#include "region.h"
#include "brands.h"
#include <cstdint>
#include <cassert>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>
#include <deque>
#include "image-items/image_item.h"
#include <codecs/hevc_boxes.h>
#include "sequences/track.h"
#include "sequences/track_visual.h"
#include "sequences/track_metadata.h"
#include "libheif/heif_sequences.h"

#if ENABLE_PARALLEL_TILE_DECODING
#include <future>
#endif

#include "context.h"
#include "file.h"
#include "pixelimage.h"
#include "api_structs.h"
#include "security_limits.h"
#include "compression.h"
#include "color-conversion/colorconversion.h"
#include "plugin_registry.h"
#include "image-items/hevc.h"
#include "image-items/vvc.h"
#include "image-items/avif.h"
#include "image-items/jpeg.h"
#include "image-items/mask_image.h"
#include "image-items/jpeg2000.h"
#include "image-items/grid.h"
#include "image-items/overlay.h"
#include "image-items/tiled.h"

#if WITH_UNCOMPRESSED_CODEC
#include "image-items/unc_image.h"
#endif
#include "text.h"


heif_encoder::heif_encoder(const heif_encoder_plugin* _plugin)
    : plugin(_plugin)
{

}

heif_encoder::~heif_encoder()
{
  release();
}

void heif_encoder::release()
{
  if (encoder) {
    plugin->free_encoder(encoder);
    encoder = nullptr;
  }
}


heif_error heif_encoder::alloc()
{
  if (encoder == nullptr) {
    heif_error error = plugin->new_encoder(&encoder);
    // TODO: error handling
    return error;
  }

  return {heif_error_Ok, heif_suberror_Unspecified, Error::kSuccess};
}


HeifContext::HeifContext()
    : m_memory_tracker(&m_limits)
{
  const char* security_limits_variable = getenv("LIBHEIF_SECURITY_LIMITS");

  if (security_limits_variable && (strcmp(security_limits_variable, "off") == 0 ||
                                   strcmp(security_limits_variable, "OFF") == 0)) {
    m_limits = disabled_security_limits;
  }
  else {
    m_limits = global_security_limits;
  }

  reset_to_empty_heif();
}


HeifContext::~HeifContext()
{
  // Break circular references between Images (when a faulty input image has circular image references)
  for (auto& it : m_all_images) {
    std::shared_ptr<ImageItem> image = it.second;
    image->clear();
  }
}


static void copy_security_limits(heif_security_limits* dst, const heif_security_limits* src)
{
  dst->max_image_size_pixels = src->max_image_size_pixels;
  dst->max_number_of_tiles = src->max_number_of_tiles;
  dst->max_bayer_pattern_pixels = src->max_bayer_pattern_pixels;
  dst->max_items = src->max_items;

  dst->max_color_profile_size = src->max_color_profile_size;
  dst->max_memory_block_size = src->max_memory_block_size;

  dst->max_components = src->max_components;

  dst->max_iloc_extents_per_item = src->max_iloc_extents_per_item;
  dst->max_size_entity_group = src->max_size_entity_group;

  dst->max_children_per_box = src->max_children_per_box;

  if (src->version >= 2) {
    dst->max_sample_description_box_entries = src->max_sample_description_box_entries;
    dst->max_sample_group_description_box_entries = src->max_sample_group_description_box_entries;
  }
}


void HeifContext::set_security_limits(const heif_security_limits* limits)
{
  // copy default limits
  if (limits->version < global_security_limits.version) {
    copy_security_limits(&m_limits, &global_security_limits);
  }

  // overwrite with input limits
  copy_security_limits(&m_limits, limits);
}


Error HeifContext::read(const std::shared_ptr<StreamReader>& reader)
{
  m_heif_file = std::make_shared<HeifFile>();
  m_heif_file->set_security_limits(&m_limits);
  Error err = m_heif_file->read(reader);
  if (err) {
    return err;
  }

  return interpret_heif_file();
}

Error HeifContext::read_from_file(const char* input_filename)
{
  m_heif_file = std::make_shared<HeifFile>();
  m_heif_file->set_security_limits(&m_limits);
  Error err = m_heif_file->read_from_file(input_filename);
  if (err) {
    return err;
  }

  return interpret_heif_file();
}

Error HeifContext::read_from_memory(const void* data, size_t size, bool copy)
{
  m_heif_file = std::make_shared<HeifFile>();
  m_heif_file->set_security_limits(&m_limits);
  Error err = m_heif_file->read_from_memory(data, size, copy);
  if (err) {
    return err;
  }

  return interpret_heif_file();
}

void HeifContext::reset_to_empty_heif()
{
  m_heif_file = std::make_shared<HeifFile>();
  m_heif_file->set_security_limits(&m_limits);
  m_heif_file->new_empty_file();

  m_all_images.clear();
  m_top_level_images.clear();
  m_primary_image.reset();
}


std::vector<std::shared_ptr<ImageItem>> HeifContext::get_top_level_images(bool return_error_images)
{
  if (return_error_images) {
    return m_top_level_images;
  }
  else {
    std::vector<std::shared_ptr<ImageItem>> filtered;
    for (auto& item : m_top_level_images) {
      if (!item->get_item_error()) {
        filtered.push_back(item);
      }
    }

    return filtered;
  }
}


std::shared_ptr<ImageItem> HeifContext::get_image(heif_item_id id, bool return_error_images)
{
  auto iter = m_all_images.find(id);
  if (iter == m_all_images.end()) {
    return nullptr;
  }
  else {
    if (iter->second->get_item_error() && !return_error_images) {
      return nullptr;
    }
    else {
      return iter->second;
    }
  }
}


std::shared_ptr<ImageItem> HeifContext::get_primary_image(bool return_error_image)
{
  if (m_primary_image == nullptr)
    return nullptr;
  else if (!return_error_image && m_primary_image->get_item_error())
    return nullptr;
  else
    return m_primary_image;
}


std::shared_ptr<const ImageItem> HeifContext::get_primary_image(bool return_error_image) const
{
  return const_cast<HeifContext*>(this)->get_primary_image(return_error_image);
}


bool HeifContext::is_image(heif_item_id ID) const
{
  return m_all_images.contains(ID);
}


std::shared_ptr<RegionItem> HeifContext::add_region_item(uint32_t reference_width, uint32_t reference_height)
{
  std::shared_ptr<Box_infe> box = m_heif_file->add_new_infe_box(fourcc("rgan"));
  box->set_hidden_item(true);

  auto regionItem = std::make_shared<RegionItem>(box->get_item_ID(), reference_width, reference_height);
  add_region_item(regionItem);

  return regionItem;
}

void HeifContext::add_region_referenced_mask_ref(heif_item_id region_item_id, heif_item_id mask_item_id)
{
  m_heif_file->add_iref_reference(region_item_id, fourcc("mask"), {mask_item_id});
}


static uint64_t rescale(uint64_t duration, uint32_t old_base, uint32_t new_base)
{
  // prevent division by zero
  // TODO: we might emit an error in this case
  if (old_base == 0) {
    return 0;
  }

  return duration * new_base / old_base;
}


void HeifContext::write(StreamWriter& writer)
{
  // --- finalize some parameters

  uint64_t max_sequence_duration = 0;
  if (auto mvhd = m_heif_file->get_mvhd_box()) {
    for (const auto& track : m_tracks) {
      track.second->finalize_track();

      // rescale track duration to movie timescale units

      uint64_t track_duration_in_media_units = track.second->get_duration_in_media_units();
      uint32_t media_timescale = track.second->get_timescale();

      uint32_t mvhd_timescale = m_heif_file->get_mvhd_box()->get_time_scale();
      if (mvhd_timescale == 0) {
        mvhd_timescale = track.second->get_timescale();
        m_heif_file->get_mvhd_box()->set_time_scale(mvhd_timescale);
      }

      uint64_t movie_duration = rescale(track_duration_in_media_units, media_timescale, mvhd_timescale);
      uint64_t unrepeated_movie_duration = movie_duration;

      // sequence repetitions

      if (m_sequence_repetitions == heif_sequence_maximum_number_of_repetitions) {
        movie_duration = std::numeric_limits<uint64_t>::max();
      }
      else {
        if (std::numeric_limits<uint64_t>::max() / m_sequence_repetitions < movie_duration) {
          movie_duration = std::numeric_limits<uint64_t>::max();
        }
        else {
          movie_duration *= m_sequence_repetitions;
        }
      }

      if (m_sequence_repetitions != 1) {
        track.second->enable_edit_list_repeat_mode(true);
      }

      track.second->set_track_duration_in_movie_units(movie_duration, unrepeated_movie_duration);

      max_sequence_duration = std::max(max_sequence_duration, movie_duration);
    }

    mvhd->set_duration(max_sequence_duration);
  }

  // --- serialize regions

  for (auto& image : m_all_images) {
    for (auto region : image.second->get_region_item_ids()) {
      m_heif_file->add_iref_reference(region,
                                      fourcc("cdsc"), {image.first});
    }
  }

  for (auto& region : m_region_items) {
    std::vector<uint8_t> data_array;
    Error err = region->encode(data_array);
    // TODO: err

    m_heif_file->append_iloc_data(region->item_id, data_array, 0);
  }

  // --- serialise text items

  for (auto& image : m_all_images) {
    for (auto text_item_id : image.second->get_text_item_ids()) {
      m_heif_file->add_iref_reference(text_item_id, fourcc("text"), {image.first});
    }
  }

  for (auto& text_item : m_text_items) {
    auto encodeResult = text_item->encode();
    if (encodeResult) {
      m_heif_file->append_iloc_data(text_item->get_item_id(), *encodeResult, 1);
    }
  }

  // --- post-process images

  for (auto& img : m_all_images) {
    img.second->process_before_write();
  }

  // --- sort item properties

  if (auto ipma = m_heif_file->get_ipma_box()) {
    ipma->sort_properties(m_heif_file->get_ipco_box());
  }

  // --- derive box versions

  m_heif_file->derive_box_versions();

  // --- determine brands

  heif_brand2 main_brand;
  std::vector<heif_brand2> compatible_brands;
  compatible_brands = compute_compatible_brands(this, &main_brand);

  // Note: major brand should be repeated in the compatible brands, according to this:
  //   ISOBMFF (ISO/IEC 14496-12:2020) § K.4:
  //   NOTE This document requires that the major brand be repeated in the compatible-brands,
  //   but this requirement is relaxed in the 'profiles' parameter for compactness.
  // See https://github.com/strukturag/libheif/issues/478

  auto ftyp = m_heif_file->get_ftyp_box();

  // set major brand if not set manually yet
  if (ftyp->get_major_brand() == 0) {
    ftyp->set_major_brand(main_brand);
  }

  ftyp->set_minor_version(0);
  for (auto brand : compatible_brands) {
    ftyp->add_compatible_brand(brand);
  }

  // --- write to file

  m_heif_file->write(writer);
}

std::string HeifContext::debug_dump_boxes() const
{
  return m_heif_file->debug_dump_boxes();
}


static bool item_type_is_image(uint32_t item_type, const std::string& content_type)
{
  return (item_type == fourcc("hvc1") ||
          item_type == fourcc("av01") ||
          item_type == fourcc("grid") ||
          item_type == fourcc("tili") ||
          item_type == fourcc("iden") ||
          item_type == fourcc("iovl") ||
          item_type == fourcc("avc1") ||
          item_type == fourcc("unci") ||
          item_type == fourcc("vvc1") ||
          item_type == fourcc("jpeg") ||
          (item_type == fourcc("mime") && content_type == "image/jpeg") ||
          item_type == fourcc("j2k1") ||
          item_type == fourcc("mski"));
}


void HeifContext::remove_top_level_image(const std::shared_ptr<ImageItem>& image)
{
  std::vector<std::shared_ptr<ImageItem>> new_list;

  for (const auto& img : m_top_level_images) {
    if (img != image) {
      new_list.push_back(img);
    }
  }

  m_top_level_images = std::move(new_list);
}


Error HeifContext::interpret_heif_file()
{
  if (m_heif_file->has_images()) {
    Error err = interpret_heif_file_images();
    if (err) {
      return err;
    }
  }

  if (m_heif_file->has_sequences()) {
    Error err = interpret_heif_file_sequences();
    if (err) {
      return err;
    }
  }

  return Error::Ok;
}


Error HeifContext::interpret_heif_file_images()
{
  m_all_images.clear();
  m_top_level_images.clear();
  m_primary_image.reset();


  // --- reference all non-hidden images

  std::vector<heif_item_id> image_IDs = m_heif_file->get_item_IDs();

  for (heif_item_id id : image_IDs) {
    auto infe_box = m_heif_file->get_infe_box(id);
    if (!infe_box) {
      // TODO(farindk): Should we return an error instead of skipping the invalid id?
      continue;
    }

    auto imageItem = ImageItem::alloc_for_infe_box(this, infe_box);
    if (!imageItem) {
      // It is no imageItem item, skip it.
      continue;
    }

    std::vector<std::shared_ptr<Box>> properties;
    Error err = m_heif_file->get_properties(id, properties);
    if (err) {
      imageItem = std::make_shared<ImageItem_Error>(imageItem->get_infe_type(), id, err);
    }

    imageItem->set_properties(properties);

    err = imageItem->initialize_decoder();
    if (err) {
      imageItem = std::make_shared<ImageItem_Error>(imageItem->get_infe_type(), id, err);
      imageItem->set_properties(properties);
    }

    m_all_images.insert(std::make_pair(id, imageItem));

    if (!infe_box->is_hidden_item()) {
      if (id == m_heif_file->get_primary_image_ID()) {
        imageItem->set_primary(true);
        m_primary_image = imageItem;
      }

      m_top_level_images.push_back(imageItem);
    }

    imageItem->set_decoder_input_data();
  }

  if (!m_primary_image) {
    return Error(heif_error_Invalid_input,
                 heif_suberror_Nonexisting_item_referenced,
                 "'pitm' box references an unsupported or non-existing image");
  }


  // --- process image properties

  for (auto& pair : m_all_images) {
    auto& image = pair.second;

    if (image->get_item_error()) {
      continue;
    }

    std::vector<std::shared_ptr<Box>> properties;

    Error err = m_heif_file->get_properties(pair.first, properties);
    if (err) {
      return err;
    }


    // --- are there any 'essential' properties that we did not parse?

    for (const auto& prop : properties) {
      if (std::dynamic_pointer_cast<Box_other>(prop) &&
          get_heif_file()->get_ipco_box()->is_property_essential_for_item(pair.first, prop, get_heif_file()->get_ipma_box())) {

        std::stringstream sstr;
        sstr << "could not parse item property '" << prop->get_type_string() << "'";
        return {heif_error_Unsupported_feature, heif_suberror_Unsupported_essential_property, sstr.str()};
      }
    }


    // --- Are there any parse errors in optional properties? Attach the errors as warnings to the images.

    bool ignore_nonfatal_parse_errors = false; // TODO: this should be a user option. Where should we put this (heif_decoding_options, or while creating the context) ?

    for (const auto& prop : properties) {
      if (auto errorbox = std::dynamic_pointer_cast<Box_Error>(prop)) {
        parse_error_fatality fatality = errorbox->get_parse_error_fatality();

        if (fatality == parse_error_fatality::optional ||
            (fatality == parse_error_fatality::ignorable && ignore_nonfatal_parse_errors)) {
          image->add_decoding_warning(errorbox->get_error());
        }
        else {
          return errorbox->get_error();
        }
      }
    }


    // --- extract image resolution

    bool ispe_read = false;
    for (const auto& prop : properties) {
      auto ispe = std::dynamic_pointer_cast<Box_ispe>(prop);
      if (ispe) {
        uint32_t width = ispe->get_width();
        uint32_t height = ispe->get_height();

        if (width == 0 || height == 0) {
          return {heif_error_Invalid_input,
                  heif_suberror_Invalid_image_size,
                  "Zero image width or height"};
        }

        image->set_resolution(width, height);
        ispe_read = true;
      }
    }

    // Note: usually, we would like to check here if an `ispe` property exists as this is mandatory.
    // We want to do this if decoding_options.strict_decoding is set, but we cannot because we have no decoding_options
    // when parsing the file structure.

    if (!ispe_read) {
      image->add_decoding_warning({heif_error_Invalid_input, heif_suberror_No_ispe_property});
    }


    for (const auto& prop : properties) {
      auto colr = std::dynamic_pointer_cast<Box_colr>(prop);
      if (colr) {
        auto profile = colr->get_color_profile();
        image->set_color_profile(profile);
        continue;
      }

      auto cmin = std::dynamic_pointer_cast<Box_cmin>(prop);
      if (cmin) {
        if (!ispe_read) {
          return {heif_error_Invalid_input, heif_suberror_No_ispe_property};
        }

        image->set_intrinsic_matrix(cmin->get_intrinsic_matrix());
      }

      auto cmex = std::dynamic_pointer_cast<Box_cmex>(prop);
      if (cmex) {
        image->set_extrinsic_matrix(cmex->get_extrinsic_matrix());
      }
    }


    for (const auto& prop : properties) {
      auto clap = std::dynamic_pointer_cast<Box_clap>(prop);
      if (clap) {
        image->set_resolution(clap->get_width_rounded(),
                              clap->get_height_rounded());

        if (image->has_intrinsic_matrix()) {
          image->get_intrinsic_matrix().apply_clap(clap.get(), image->get_width(), image->get_height());
        }
      }

      auto imir = std::dynamic_pointer_cast<Box_imir>(prop);
      if (imir) {
        if (!ispe_read) {
          return {heif_error_Invalid_input, heif_suberror_No_ispe_property};
        }

        image->get_intrinsic_matrix().apply_imir(imir.get(), image->get_width(), image->get_height());
      }

      auto irot = std::dynamic_pointer_cast<Box_irot>(prop);
      if (irot) {
        if (irot->get_rotation_ccw() == 90 ||
            irot->get_rotation_ccw() == 270) {
          if (!ispe_read) {
            return {heif_error_Invalid_input, heif_suberror_No_ispe_property};
          }

          // swap width and height
          image->set_resolution(image->get_height(),
                                image->get_width());
        }

        // TODO: apply irot to camera extrinsic matrix
      }
    }


    // --- assign GIMI content-ID to image

    if (auto box_gimi_content_id = image->get_property<Box_gimi_content_id>()) {
      image->set_gimi_sample_content_id(box_gimi_content_id->get_content_id());
    }
  }


  // --- remove auxiliary from top-level images and assign to their respective image

  auto iref_box = m_heif_file->get_iref_box();
  if (iref_box) {
    // m_top_level_images.clear();

    for (auto& pair : m_all_images) {
      auto& image = pair.second;

      std::vector<Box_iref::Reference> references = iref_box->get_references_from(image->get_id());

      for (const Box_iref::Reference& ref : references) {
        uint32_t type = ref.header.get_short_type();

        if (type == fourcc("thmb")) {
          // --- this is a thumbnail image, attach to the main image

          std::vector<heif_item_id> refs = ref.to_item_ID;
          for (heif_item_id ref: refs) {
            image->set_is_thumbnail();

            auto master_iter = m_all_images.find(ref);
            if (master_iter == m_all_images.end()) {
              return Error(heif_error_Invalid_input,
                          heif_suberror_Nonexisting_item_referenced,
                          "Thumbnail references a non-existing image");
            }

            if (master_iter->second->is_thumbnail()) {
              return Error(heif_error_Invalid_input,
                          heif_suberror_Nonexisting_item_referenced,
                          "Thumbnail references another thumbnail");
            }

            if (image.get() == master_iter->second.get()) {
              return Error(heif_error_Invalid_input,
                          heif_suberror_Nonexisting_item_referenced,
                          "Recursive thumbnail image detected");
            }
            master_iter->second->add_thumbnail(image);
          }
          remove_top_level_image(image);
        }
        else if (type == fourcc("auxl")) {

          // --- this is an auxiliary image
          //     check whether it is an alpha channel and attach to the main image if yes

          std::shared_ptr<Box_auxC> auxC_property = image->get_property<Box_auxC>();
          if (!auxC_property) {
            std::stringstream sstr;
            sstr << "No auxC property for image " << image->get_id();
            return Error(heif_error_Invalid_input,
                         heif_suberror_Auxiliary_image_type_unspecified,
                         sstr.str());
          }

          std::vector<heif_item_id> refs = ref.to_item_ID;

          // alpha channel

          if (auxC_property->get_aux_type() == "urn:mpeg:avc:2015:auxid:1" ||   // HEIF (avc)
              auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:1" ||  // HEIF (h265)
              auxC_property->get_aux_type() == "urn:mpeg:mpegB:cicp:systems:auxiliary:alpha") { // MIAF

            for (heif_item_id ref: refs) {
              auto master_iter = m_all_images.find(ref);
              if (master_iter == m_all_images.end()) {

                if (!m_heif_file->has_item_with_id(ref)) {
                  return Error(heif_error_Invalid_input,
                               heif_suberror_Nonexisting_item_referenced,
                               "Non-existing alpha image referenced");
                }

                continue;
              }

              auto master_img = master_iter->second;

              if (image.get() == master_img.get()) {
                return Error(heif_error_Invalid_input,
                            heif_suberror_Nonexisting_item_referenced,
                            "Recursive alpha image detected");
              }

              image->set_is_alpha_channel();
              master_img->set_alpha_channel(image);
            }
          }


          // depth channel

          if (auxC_property->get_aux_type() == "urn:mpeg:hevc:2015:auxid:2" || // HEIF
              auxC_property->get_aux_type() == "urn:mpeg:mpegB:cicp:systems:auxiliary:depth") { // AVIF
            image->set_is_depth_channel();

            for (heif_item_id ref: refs) {
              auto master_iter = m_all_images.find(ref);
              if (master_iter == m_all_images.end()) {

                if (!m_heif_file->has_item_with_id(ref)) {
                  return Error(heif_error_Invalid_input,
                               heif_suberror_Nonexisting_item_referenced,
                               "Non-existing depth image referenced");
                }

                continue;
              }
              if (image.get() == master_iter->second.get()) {
                return Error(heif_error_Invalid_input,
                            heif_suberror_Nonexisting_item_referenced,
                            "Recursive depth image detected");
              }
              master_iter->second->set_depth_channel(image);

              const auto& subtypes = auxC_property->get_subtypes();

              if (!subtypes.empty()) {
                std::vector<std::shared_ptr<SEIMessage>> sei_messages;
                Error err = decode_hevc_aux_sei_messages(subtypes, sei_messages);
                if (err) {
                  return err;
                }

                for (auto& msg : sei_messages) {
                  auto depth_msg = std::dynamic_pointer_cast<SEIMessage_depth_representation_info>(msg);
                  if (depth_msg) {
                    image->set_depth_representation_info(*depth_msg);
                  }
                }
              }
            }
          }


          // --- generic aux image

          image->set_is_aux_image(auxC_property->get_aux_type());

          for (heif_item_id ref: refs) {
            auto master_iter = m_all_images.find(ref);
            if (master_iter == m_all_images.end()) {

              if (!m_heif_file->has_item_with_id(ref)) {
                return Error(heif_error_Invalid_input,
                             heif_suberror_Nonexisting_item_referenced,
                             "Non-existing aux image referenced");
              }

              continue;
            }
            if (image.get() == master_iter->second.get()) {
              return Error(heif_error_Invalid_input,
                          heif_suberror_Nonexisting_item_referenced,
                          "Recursive aux image detected");
            }

            master_iter->second->add_aux_image(image);

            remove_top_level_image(image);
          }
        }
        else {
          // 'image' is a normal image, keep it as a top-level image
        }
      }
    }
  }


  // --- check that HEVC images have an hvcC property

  for (auto& pair : m_all_images) {
    auto& image = pair.second;

    if (image->get_item_error()) {
      continue;
    }

    std::shared_ptr<Box_infe> infe = m_heif_file->get_infe_box(image->get_id());
    if (infe->get_item_type_4cc() == fourcc("hvc1")) {

      auto ipma = m_heif_file->get_ipma_box();
      auto ipco = m_heif_file->get_ipco_box();

      if (!ipco->get_property_for_item_ID(image->get_id(), ipma, fourcc("hvcC"))) {
        return Error(heif_error_Invalid_input,
                     heif_suberror_No_hvcC_box,
                     "No hvcC property in hvc1 type image");
      }
    }
    if (infe->get_item_type_4cc() == fourcc("vvc1")) {

      auto ipma = m_heif_file->get_ipma_box();
      auto ipco = m_heif_file->get_ipco_box();

      if (!ipco->get_property_for_item_ID(image->get_id(), ipma, fourcc("vvcC"))) {
        return Error(heif_error_Invalid_input,
                     heif_suberror_No_vvcC_box,
                     "No vvcC property in vvc1 type image");
      }
    }
    // TODO: check for AV1, AVC, JPEG, J2K
  }


  // --- assign color profile from grid tiles to main image when main image has no profile assigned

  for (auto& pair : m_all_images) {
    auto& image = pair.second;
    auto id = pair.first;

    if (image->get_item_error()) {
      continue;
    }

    auto infe_box = m_heif_file->get_infe_box(id);
    if (!infe_box) {
      continue;
    }

    if (!iref_box) {
      break;
    }

    if (infe_box->get_item_type_4cc() == fourcc("grid")) {
      std::vector<heif_item_id> image_references = iref_box->get_references(id, fourcc("dimg"));

      if (image_references.empty()) {
        continue; // TODO: can this every happen?
      }

      auto tileId = image_references.front();

      auto iter = m_all_images.find(tileId);
      if (iter == m_all_images.end()) {
        continue; // invalid grid entry
      }

      auto tile_img = iter->second;
      if (image->get_color_profile_icc() == nullptr && tile_img->get_color_profile_icc()) {
        image->set_color_profile(tile_img->get_color_profile_icc());
      }

      if (!image->has_nclx_color_profile() && tile_img->has_nclx_color_profile()) {
        image->set_color_profile_nclx(tile_img->get_color_profile_nclx());
      }
    }
  }


  // --- read metadata and assign to image

  for (heif_item_id id : image_IDs) {
    uint32_t item_type = m_heif_file->get_item_type_4cc(id);
    std::string content_type = m_heif_file->get_content_type(id);

    // 'rgan': skip region annotations, handled next
    // 'iden': iden images are no metadata
    if (item_type_is_image(item_type, content_type) || item_type == fourcc("rgan")) {
      continue;
    }

    std::string item_uri_type = m_heif_file->get_item_uri_type(id);

    // we now assign all kinds of metadata to the image, not only 'Exif' and 'XMP'

    std::shared_ptr<ImageMetadata> metadata = std::make_shared<ImageMetadata>();
    metadata->item_id = id;
    metadata->item_type = fourcc_to_string(item_type);
    metadata->content_type = content_type;
    metadata->item_uri_type = std::move(item_uri_type);

    auto metadataResult = m_heif_file->get_uncompressed_item_data(id);
    if (!metadataResult) {
      if (item_type == fourcc("Exif") || item_type == fourcc("mime")) {
        // these item types should have data
        return metadataResult.error();
      }
      else {
        // anything else is probably something that we don't understand yet
        continue;
      }
    }
    else {
      metadata->m_data = *metadataResult;
    }

    // --- assign metadata to the image

    if (iref_box) {
      std::vector<heif_item_id> references = iref_box->get_references(id, fourcc("cdsc"));
      for (heif_item_id exif_image_id : references) {
        auto img_iter = m_all_images.find(exif_image_id);
        if (img_iter == m_all_images.end()) {
          if (!m_heif_file->has_item_with_id(exif_image_id)) {
            return Error(heif_error_Invalid_input,
                         heif_suberror_Nonexisting_item_referenced,
                         "Metadata assigned to non-existing image");
          }

          continue;
        }
        img_iter->second->add_metadata(metadata);
      }
    }
  }

  // --- set premultiplied alpha flag

  for (heif_item_id id : image_IDs) {
    if (iref_box) {
      std::vector<heif_item_id> references = iref_box->get_references(id, fourcc("prem"));
      for (heif_item_id ref : references) {
        (void)ref;

        heif_item_id color_image_id = id;
        auto img_iter = m_all_images.find(color_image_id);
        if (img_iter == m_all_images.end()) {
          return Error(heif_error_Invalid_input,
                       heif_suberror_Nonexisting_item_referenced,
                       "`prem` link assigned to non-existing image");
        }

        img_iter->second->set_is_premultiplied_alpha(true);
      }
    }
  }

  // --- read region item and assign to image(s)

  for (heif_item_id id : image_IDs) {
    uint32_t item_type = m_heif_file->get_item_type_4cc(id);
    if (item_type != fourcc("rgan")) {
      continue;
    }

    std::shared_ptr<RegionItem> region_item = std::make_shared<RegionItem>();
    region_item->item_id = id;

    Result regionDataResult = m_heif_file->get_uncompressed_item_data(id);
    if (!regionDataResult) {
      return regionDataResult.error();
    }
    region_item->parse(*regionDataResult, get_security_limits());

    if (iref_box) {
      std::vector<Box_iref::Reference> references = iref_box->get_references_from(id);
      for (const auto& ref : references) {
        if (ref.header.get_short_type() == fourcc("cdsc")) {
          std::vector<uint32_t> refs = ref.to_item_ID;
          for (uint32_t ref : refs) {
            uint32_t image_id = ref;
            auto img_iter = m_all_images.find(image_id);
            if (img_iter == m_all_images.end()) {
              return Error(heif_error_Invalid_input,
                           heif_suberror_Nonexisting_item_referenced,
                           "Region item assigned to non-existing image");
            }
            img_iter->second->add_region_item_id(id);
            m_region_items.push_back(region_item);
          }
        }

        /* When the geometry 'mask' of a region is represented by a mask stored in
        * another image item the image item containing the mask shall be identified
        * by an item reference of type 'mask' from the region item to the image item
        * containing the mask. */
        if (ref.header.get_short_type() == fourcc("mask")) {
          std::vector<uint32_t> refs = ref.to_item_ID;
          size_t mask_index = 0;
          for (int j = 0; j < region_item->get_number_of_regions(); j++) {
            if (region_item->get_regions()[j]->getRegionType() == heif_region_type_referenced_mask) {
              std::shared_ptr<RegionGeometry_ReferencedMask> mask_geometry = std::dynamic_pointer_cast<RegionGeometry_ReferencedMask>(region_item->get_regions()[j]);

              if (mask_index >= refs.size()) {
                return Error(heif_error_Invalid_input,
                             heif_suberror_Unspecified,
                             "Region mask reference with non-existing mask image reference");
              }

              uint32_t mask_image_id = refs[mask_index];
              if (!is_image(mask_image_id)) {
                return Error(heif_error_Invalid_input,
                             heif_suberror_Unspecified,
                             "Region mask referenced item is not an image");
              }

              auto mask_image = get_image(mask_image_id, true);
              if (auto error = mask_image->get_item_error()) {
                return error;
              }

              mask_geometry->referenced_item = mask_image_id;
              if (mask_geometry->width == 0) {
                mask_geometry->width = mask_image->get_ispe_width();
              }
              if (mask_geometry->height == 0) {
                mask_geometry->height = mask_image->get_ispe_height();
              }
              mask_index += 1;
              remove_top_level_image(mask_image);
            }
          }
        }
      }
    }
  }

  // --- read text item and assign to image(s)
  for (heif_item_id id : image_IDs) {
    uint32_t item_type = m_heif_file->get_item_type_4cc(id);
    if (item_type != fourcc("mime")) { // TODO: && content_type  starts with "text/" ?
      continue;
    }
    std::shared_ptr<TextItem> text_item = std::make_shared<TextItem>();
    text_item->set_item_id(id);

    auto textDataResult = m_heif_file->get_uncompressed_item_data(id);
    if (!textDataResult) {
      return textDataResult.error();
    }

    text_item->parse(*textDataResult);
    if (iref_box) {
      std::vector<Box_iref::Reference> references = iref_box->get_references_from(id);
      for (const auto& ref : references) {
        if (ref.header.get_short_type() == fourcc("text")) {
          std::vector<uint32_t> refs = ref.to_item_ID;
          for (uint32_t ref : refs) {
            uint32_t image_id = ref;
            auto img_iter = m_all_images.find(image_id);
            if (img_iter == m_all_images.end()) {
              return Error(heif_error_Invalid_input,
                           heif_suberror_Nonexisting_item_referenced,
                           "Text item assigned to non-existing image");
            }
            img_iter->second->add_text_item_id(id);
            m_text_items.push_back(text_item);
          }
        }
      }
    }
  }

  return Error::Ok;
}


bool HeifContext::has_alpha(heif_item_id ID) const
{
  auto imgIter = m_all_images.find(ID);
  if (imgIter == m_all_images.end()) {
    return false;
  }

  auto img = imgIter->second;

  // --- has the image an auxiliary alpha image?

  if (img->get_alpha_channel() != nullptr) {
    return true;
  }

  if (img->has_coded_alpha_channel()) {
    return true;
  }

  heif_colorspace colorspace;
  heif_chroma chroma;
  Error err = img->get_coded_image_colorspace(&colorspace, &chroma);
  if (err) {
    return false;
  }

  if (chroma == heif_chroma_interleaved_RGBA ||
      chroma == heif_chroma_interleaved_RRGGBBAA_BE ||
      chroma == heif_chroma_interleaved_RRGGBBAA_LE) {
    return true;
  }

  // --- if the image is a 'grid', check if there is alpha in any of the tiles

  // TODO: move this into ImageItem

  uint32_t image_type = m_heif_file->get_item_type_4cc(ID);
  if (image_type == fourcc("grid")) {

    Result gridDataResult = m_heif_file->get_uncompressed_item_data(ID);
    if (!gridDataResult) {
      return false;
    }

    ImageGrid grid;
    err = grid.parse(*gridDataResult);
    if (err) {
      return false;
    }


    auto iref_box = m_heif_file->get_iref_box();

    if (!iref_box) {
      return false;
    }

    std::vector<heif_item_id> image_references = iref_box->get_references(ID, fourcc("dimg"));

    if ((int) image_references.size() != grid.get_rows() * grid.get_columns()) {
      return false;
    }


    // --- check that all image IDs are valid images

    for (heif_item_id tile_id : image_references) {
      if (!is_image(tile_id)) {
        return false;
      }
    }

    // --- check whether at least one tile has an alpha channel

    bool has_alpha = false;

    for (heif_item_id tile_id : image_references) {
      auto iter = m_all_images.find(tile_id);
      if (iter == m_all_images.end()) {
        return false;
      }

      const std::shared_ptr<ImageItem> tileImg = iter->second;

      has_alpha |= tileImg->get_alpha_channel() != nullptr;
    }

    return has_alpha;
  }
  else {
    // TODO: what about overlays ?
    return false;
  }
}


Error HeifContext::get_id_of_non_virtual_child_image(heif_item_id id, heif_item_id& out) const
{
  uint32_t image_type = m_heif_file->get_item_type_4cc(id);
  if (image_type == fourcc("grid") ||
      image_type == fourcc("iden") ||
      image_type == fourcc("iovl")) {
    auto iref_box = m_heif_file->get_iref_box();
    if (!iref_box) {
      return Error(heif_error_Invalid_input,
                   heif_suberror_No_item_data,
                   "Derived image does not reference any other image items");
    }

    std::vector<heif_item_id> image_references = iref_box->get_references(id, fourcc("dimg"));

    // TODO: check whether this really can be recursive (e.g. overlay of grid images)

    if (image_references.empty() || image_references[0] == id) {
      return Error(heif_error_Invalid_input,
                   heif_suberror_No_item_data,
                   "Derived image does not reference any other image items");
    }
    else {
      return get_id_of_non_virtual_child_image(image_references[0], out);
    }
  }
  else {
    if (!m_all_images.contains(id)) {
      std::stringstream sstr;
      sstr << "Image item " << id << " referenced, but it does not exist\n";

      return Error(heif_error_Invalid_input,
        heif_suberror_Nonexisting_item_referenced,
        sstr.str());
    }
    else if (dynamic_cast<ImageItem_Error*>(m_all_images.find(id)->second.get())) {
      // Should er return an error here or leave it to the follow-up code to detect that?
    }

    out = id;
    return Error::Ok;
  }
}


Result<std::shared_ptr<HeifPixelImage>> HeifContext::decode_image(heif_item_id ID,
                                                                  heif_colorspace out_colorspace,
                                                                  heif_chroma out_chroma,
                                                                  const heif_decoding_options& options,
                                                                  bool decode_only_tile, uint32_t tx, uint32_t ty,
                                                                  std::set<heif_item_id> processed_ids) const
{
  std::shared_ptr<ImageItem> imgitem;
  if (m_all_images.contains(ID)) {
    imgitem = m_all_images.find(ID)->second;
  }

  // Note: this may happen, for example when an 'iden' image references a non-existing image item.
  if (imgitem == nullptr) {
    return Error(heif_error_Invalid_input, heif_suberror_Nonexisting_item_referenced);
  }


  auto decodingResult = imgitem->decode_image(options, decode_only_tile, tx, ty, processed_ids);
  if (!decodingResult) {
    return decodingResult.error();
  }

  std::shared_ptr<HeifPixelImage> img = *decodingResult;


  // --- convert to output chroma format

  auto img_result = convert_to_output_colorspace(img, out_colorspace, out_chroma, options);
  if (!img_result) {
    return img_result.error();
  }
  else {
    img = *img_result;
  }

  img->add_warnings(imgitem->get_decoding_warnings());

  return img;
}


bool nclx_color_profile_equal(std::optional<nclx_profile> a,
                              const heif_color_profile_nclx* b)
{
  if (!a && b==nullptr) {
    return true;
  }

  heif_color_profile_nclx* default_nclx = nullptr;

  if (!a || b==nullptr) {
    default_nclx = heif_nclx_color_profile_alloc();

    if (!a) {
      a = nclx_profile::defaults();
    }

    if (b==nullptr) {
      b = default_nclx;
    }
  }

  bool equal = true;
  if (a->m_matrix_coefficients != b->matrix_coefficients ||
      a->m_colour_primaries != b->color_primaries ||
      a->m_transfer_characteristics != b->transfer_characteristics ||
      a->m_full_range_flag != b->full_range_flag) {
    equal = false;
  }

  if (default_nclx) {
    heif_nclx_color_profile_free(default_nclx);
  }

  return equal;
}


Result<std::shared_ptr<HeifPixelImage>> HeifContext::convert_to_output_colorspace(std::shared_ptr<HeifPixelImage> img,
                                                                                  heif_colorspace out_colorspace,
                                                                                  heif_chroma out_chroma,
                                                                                  const heif_decoding_options& options) const
{
  heif_colorspace target_colorspace = (out_colorspace == heif_colorspace_undefined ?
                                       img->get_colorspace() :
                                       out_colorspace);

  heif_chroma target_chroma = (out_chroma == heif_chroma_undefined ?
                               img->get_chroma_format() : out_chroma);

  bool different_chroma = (target_chroma != img->get_chroma_format());
  bool different_colorspace = (target_colorspace != img->get_colorspace());

  uint8_t img_bpp = img->get_visual_image_bits_per_pixel();
  uint8_t converted_output_bpp = (options.convert_hdr_to_8bit && img_bpp > 8) ? 8 : 0 /* keep input depth */;

  nclx_profile img_nclx = img->get_color_profile_nclx_with_fallback();
  bool different_nclx = !nclx_color_profile_equal(img_nclx, options.output_image_nclx_profile);

  if (different_chroma ||
      different_colorspace ||
      converted_output_bpp ||
      different_nclx ||
      (img->has_alpha() && options.color_conversion_options_ext && options.color_conversion_options_ext->alpha_composition_mode != heif_alpha_composition_mode_none)) {

    nclx_profile output_profile;
    if (options.output_image_nclx_profile) {
      output_profile.set_matrix_coefficients(options.output_image_nclx_profile->matrix_coefficients);
      output_profile.set_colour_primaries(options.output_image_nclx_profile->color_primaries);
      output_profile.set_full_range_flag(options.output_image_nclx_profile->full_range_flag);
    }
    else {
      output_profile.set_sRGB_defaults();
    }

    return convert_colorspace(img, target_colorspace, target_chroma, output_profile, converted_output_bpp,
                                         options.color_conversion_options, options.color_conversion_options_ext,
                                         get_security_limits());
  }
  else {
    return img;
  }
}


Result<std::shared_ptr<HeifPixelImage>>
create_alpha_image_from_image_alpha_channel(const std::shared_ptr<HeifPixelImage>& image,
                                            const heif_security_limits* limits)
{
  // --- generate alpha image

  std::shared_ptr<HeifPixelImage> alpha_image = std::make_shared<HeifPixelImage>();
  alpha_image->create(image->get_width(), image->get_height(),
                      heif_colorspace_monochrome, heif_chroma_monochrome);

  if (image->has_channel(heif_channel_Alpha)) {
    alpha_image->copy_new_plane_from(image, heif_channel_Alpha, heif_channel_Y, limits);
  }
  else if (image->get_chroma_format() == heif_chroma_interleaved_RGBA) {
    if (auto err = alpha_image->extract_alpha_from_RGBA(image, limits)) {
      return err;
    }
  }
  // TODO: 16 bit

  // --- set nclx profile with full-range flag

  nclx_profile nclx = nclx_profile::undefined();
  nclx.set_full_range_flag(true); // this is the default, but just to be sure in case the defaults change
  alpha_image->set_color_profile_nclx(nclx);

  return alpha_image;
}


Result<std::shared_ptr<ImageItem>> HeifContext::encode_image(const std::shared_ptr<HeifPixelImage>& pixel_image,
                                heif_encoder* encoder,
                                const heif_encoding_options& in_options,
                                heif_image_input_class input_class)
{
  std::shared_ptr<ImageItem> output_image_item = ImageItem::alloc_for_compression_format(this, encoder->plugin->compression_format);


#if 0
  // TODO: the hdlr box is not the right place for comments
  // m_heif_file->set_hdlr_library_info(encoder->plugin->get_plugin_name());

    case heif_compression_mask: {
      error = encode_image_as_mask(pixel_image,
                                  encoder,
                                  options,
                                  input_class,
                                  out_image);
    }
      break;

    default:
      return Error(heif_error_Encoder_plugin_error, heif_suberror_Unsupported_codec);
  }
#endif


  // --- check whether we have to convert the image color space

  // The reason for doing the color conversion here is that the input might be an RGBA image and the color conversion
  // will extract the alpha plane anyway. We can reuse that plane below instead of having to do a new conversion.

  heif_encoding_options options = in_options;

  std::shared_ptr<HeifPixelImage> colorConvertedImage;

  if (output_image_item->get_encoder()) {
    if (const auto* nclx = output_image_item->get_encoder()->get_forced_output_nclx()) {
      options.output_nclx_profile = const_cast<heif_color_profile_nclx*>(nclx);
    }

    Result<std::shared_ptr<HeifPixelImage>> srcImageResult;
    srcImageResult = output_image_item->get_encoder()->convert_colorspace_for_encoding(pixel_image,
                                                                                       encoder,
                                                                                       options.output_nclx_profile,
                                                                                       &options.color_conversion_options,
                                                                                       get_security_limits());
    if (!srcImageResult) {
      return srcImageResult.error();
    }

    colorConvertedImage = *srcImageResult;
  }
  else {
    colorConvertedImage = pixel_image;
  }

  Error err = output_image_item->encode_to_item(this,
                                                colorConvertedImage,
                                                encoder, options, input_class);
  if (err) {
    return err;
  }

  insert_image_item(output_image_item->get_id(), output_image_item);


  // --- if there is an alpha channel, add it as an additional image

  if (options.save_alpha_channel &&
      colorConvertedImage->has_alpha() &&
      output_image_item->get_auxC_alpha_channel_type() != nullptr) { // does not need a separate alpha aux image

    // --- generate alpha image
    // TODO: can we directly code a monochrome image instead of the dummy color channels?

    std::shared_ptr<HeifPixelImage> alpha_image;
    auto alpha_image_result = create_alpha_image_from_image_alpha_channel(colorConvertedImage, get_security_limits());
    if (!alpha_image_result) {
      return alpha_image_result.error();
    }

    alpha_image = *alpha_image_result;


    // --- encode the alpha image

    auto alphaEncodingResult = encode_image(alpha_image, encoder, options,
                         heif_image_input_class_alpha);
    if (!alphaEncodingResult) {
      return alphaEncodingResult.error();
    }

    std::shared_ptr<ImageItem> heif_alpha_image = *alphaEncodingResult;

    m_heif_file->add_iref_reference(heif_alpha_image->get_id(), fourcc("auxl"), {output_image_item->get_id()});
    m_heif_file->set_auxC_property(heif_alpha_image->get_id(), output_image_item->get_auxC_alpha_channel_type());

    if (pixel_image->is_premultiplied_alpha()) {
      m_heif_file->add_iref_reference(output_image_item->get_id(), fourcc("prem"), {heif_alpha_image->get_id()});
    }
  }

  std::vector<std::shared_ptr<Box>> properties;
  err = m_heif_file->get_properties(output_image_item->get_id(), properties);
  if (err) {
    return err;
  }
  output_image_item->set_properties(properties);

  //m_heif_file->set_brand(encoder->plugin->compression_format,
  //                       output_image_item->is_miaf_compatible());

  return output_image_item;
}


void HeifContext::set_primary_image(const std::shared_ptr<ImageItem>& image)
{
  // update heif context

  if (m_primary_image) {
    m_primary_image->set_primary(false);
  }

  image->set_primary(true);
  m_primary_image = image;


  // update pitm box in HeifFile

  m_heif_file->set_primary_item_id(image->get_id());
}


Error HeifContext::assign_thumbnail(const std::shared_ptr<ImageItem>& master_image,
                                    const std::shared_ptr<ImageItem>& thumbnail_image)
{
  m_heif_file->add_iref_reference(thumbnail_image->get_id(),
                                  fourcc("thmb"), {master_image->get_id()});

  return Error::Ok;
}


Result<std::shared_ptr<ImageItem>> HeifContext::encode_thumbnail(const std::shared_ptr<HeifPixelImage>& image,
                                                                 heif_encoder* encoder,
                                                                 const heif_encoding_options& options,
                                                                 int bbox_size)
{
  int orig_width = image->get_width();
  int orig_height = image->get_height();

  int thumb_width, thumb_height;

  if (orig_width <= bbox_size && orig_height <= bbox_size) {
    // original image is smaller than thumbnail size -> do not encode any thumbnail

    return Error::Ok;
  }
  else if (orig_width > orig_height) {
    thumb_height = orig_height * bbox_size / orig_width;
    thumb_width = bbox_size;
  }
  else {
    thumb_width = orig_width * bbox_size / orig_height;
    thumb_height = bbox_size;
  }


  // round size to even width and height

  thumb_width &= ~1;
  thumb_height &= ~1;


  std::shared_ptr<HeifPixelImage> thumbnail_image;
  Error error = image->scale_nearest_neighbor(thumbnail_image, thumb_width, thumb_height, get_security_limits());
  if (error) {
    return error;
  }

  auto encodingResult = encode_image(thumbnail_image,
                       encoder, options,
                       heif_image_input_class_thumbnail);
  if (!encodingResult) {
    return encodingResult.error();
  }

  return *encodingResult;
}


Error HeifContext::add_exif_metadata(const std::shared_ptr<ImageItem>& master_image, const void* data, int size)
{
  // find location of TIFF header
  uint32_t offset = 0;
  const char* tiffmagic1 = "MM\0*";
  const char* tiffmagic2 = "II*\0";
  while (offset + 4 < (unsigned int) size) {
    if (!memcmp((uint8_t*) data + offset, tiffmagic1, 4)) break;
    if (!memcmp((uint8_t*) data + offset, tiffmagic2, 4)) break;
    offset++;
  }
  if (offset >= (unsigned int) size) {
    return Error(heif_error_Usage_error,
                 heif_suberror_Invalid_parameter_value,
                 "Could not find location of TIFF header in Exif metadata.");
  }


  std::vector<uint8_t> data_array;
  data_array.resize(size + 4);
  data_array[0] = (uint8_t) ((offset >> 24) & 0xFF);
  data_array[1] = (uint8_t) ((offset >> 16) & 0xFF);
  data_array[2] = (uint8_t) ((offset >> 8) & 0xFF);
  data_array[3] = (uint8_t) ((offset) & 0xFF);
  memcpy(data_array.data() + 4, data, size);


  return add_generic_metadata(master_image,
                              data_array.data(), (int) data_array.size(),
                              fourcc("Exif"), nullptr, nullptr, heif_metadata_compression_off, nullptr);
}


Error HeifContext::add_XMP_metadata(const std::shared_ptr<ImageItem>& master_image, const void* data, int size,
                                    heif_metadata_compression compression)
{
  return add_generic_metadata(master_image, data, size, fourcc("mime"), "application/rdf+xml", nullptr, compression, nullptr);
}


Error HeifContext::add_generic_metadata(const std::shared_ptr<ImageItem>& master_image, const void* data, int size,
                                        uint32_t item_type, const char* content_type, const char* item_uri_type, heif_metadata_compression compression,
                                        heif_item_id* out_item_id)
{
  // create an infe box describing what kind of data we are storing (this also creates a new ID)

  auto metadata_infe_box = m_heif_file->add_new_infe_box(item_type);
  metadata_infe_box->set_hidden_item(true);
  if (content_type != nullptr) {
    metadata_infe_box->set_content_type(content_type);
  }

  heif_item_id metadata_id = metadata_infe_box->get_item_ID();
  if (out_item_id) {
    *out_item_id = metadata_id;
  }


  // we assign this data to the image

  m_heif_file->add_iref_reference(metadata_id,
                                  fourcc("cdsc"), {master_image->get_id()});


  // --- metadata compression

  if (compression == heif_metadata_compression_auto) {
    compression = heif_metadata_compression_off; // currently, we don't use header compression by default
  }

  // only set metadata compression for MIME type data which has 'content_encoding' field
  if (compression != heif_metadata_compression_off &&
      item_type != fourcc("mime")) {
    // TODO: error, compression not supported
  }


  std::vector<uint8_t> data_array;
  if (compression == heif_metadata_compression_zlib) {
#if HAVE_ZLIB
    data_array = compress_zlib((const uint8_t*) data, size);
    metadata_infe_box->set_content_encoding("compress_zlib");
#else
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_header_compression_method);
#endif
  }
  else if (compression == heif_metadata_compression_deflate) {
#if HAVE_ZLIB
    data_array = compress_zlib((const uint8_t*) data, size);
    metadata_infe_box->set_content_encoding("deflate");
#else
    return Error(heif_error_Unsupported_feature,
                 heif_suberror_Unsupported_header_compression_method);
#endif
  }
  else {
    // uncompressed data, plain copy

    data_array.resize(size);
    memcpy(data_array.data(), data, size);
  }

  // copy the data into the file, store the pointer to it in an iloc box entry

  m_heif_file->append_iloc_data(metadata_id, data_array, 0);

  return Error::Ok;
}


heif_property_id HeifContext::add_property(heif_item_id targetItem, std::shared_ptr<Box> property, bool essential)
{
  heif_property_id id;

  if (auto img = get_image(targetItem, false)) {
    id = img->add_property(property, essential);
  }
  else {
    id = m_heif_file->add_property(targetItem, property, essential);
  }

  return id;
}


Result<heif_item_id> HeifContext::add_pyramid_group(const std::vector<heif_item_id>& layer_item_ids)
{
  struct pymd_entry
  {
    std::shared_ptr<ImageItem> item;
    uint32_t width = 0;
  };

  // --- sort all images by size

  std::vector<pymd_entry> pymd_entries;
  for (auto id : layer_item_ids) {
    auto image_item = get_image(id, true);
    if (auto error = image_item->get_item_error()) {
      return error;
    }

    pymd_entry entry;
    entry.item = image_item;
    entry.width = image_item->get_width();
    pymd_entries.emplace_back(entry);
  }

  std::sort(pymd_entries.begin(), pymd_entries.end(), [](const pymd_entry& a, const pymd_entry& b) {
    return a.width < b.width;
  });


  // --- generate pymd box

  auto pymd = std::make_shared<Box_pymd>();
  std::vector<Box_pymd::LayerInfo> layers;
  std::vector<heif_item_id> ids;

  auto base_item = pymd_entries.back().item;

  uint32_t tile_w=0, tile_h=0;
  base_item->get_tile_size(tile_w, tile_h);

  uint32_t last_width=0, last_height=0;

  for (const auto& entry : pymd_entries) {
    auto layer_item = entry.item;

    if (false) {
      // according to pymd definition, we should check that all layers have the same tile size
      uint32_t item_tile_w = 0, item_tile_h = 0;
      base_item->get_tile_size(item_tile_w, item_tile_h);
      if (item_tile_w != tile_w || item_tile_h != tile_h) {
        // TODO: add warning that tile sizes are not the same
      }
    }

    heif_image_tiling tiling = layer_item->get_heif_image_tiling();

    if (tiling.image_width < last_width || tiling.image_height < last_height) {
      return Error{
        heif_error_Invalid_input,
        heif_suberror_Invalid_parameter_value,
        "Multi-resolution pyramid images have to be provided ordered from smallest to largest."
      };
    }

    last_width = tiling.image_width;
    last_height = tiling.image_height;

    Box_pymd::LayerInfo layer{};
    layer.layer_binning = (uint16_t)(base_item->get_width() / tiling.image_width);
    layer.tiles_in_layer_row_minus1 = static_cast<uint16_t>(tiling.num_rows - 1);
    layer.tiles_in_layer_column_minus1 = static_cast<uint16_t>(tiling.num_columns - 1);
    layers.push_back(layer);
    ids.push_back(layer_item->get_id());
  }

  heif_item_id group_id = m_heif_file->get_unused_item_id();

  pymd->set_group_id(group_id);
  pymd->set_layers((uint16_t)tile_w, (uint16_t)tile_h, layers, ids);

  m_heif_file->add_entity_group_box(pymd);

  // add back-references to base image

  for (size_t i = 0; i < ids.size() - 1; i++) {
    m_heif_file->add_iref_reference(ids[i], fourcc("base"), {ids.back()});
  }

  return {group_id};
}


Result<heif_property_id> HeifContext::add_text_property(heif_item_id itemId, const std::string& language)
{
  if (find_property<Box_elng>(itemId)) {
    return Error{
      heif_error_Usage_error,
      heif_suberror_Unspecified,
      "Item already has an 'elng' language property."
    };
  }

  auto elng = std::make_shared<Box_elng>();
  elng->set_lang(std::string(language));

  heif_property_id id = add_property(itemId, elng, false);
  return id;
}


Error HeifContext::interpret_heif_file_sequences()
{
  m_tracks.clear();


  // --- reference all non-hidden images

  auto moov = m_heif_file->get_moov_box();
  assert(moov);

  auto mvhd = moov->get_child_box<Box_mvhd>();
  if (!mvhd) {
    assert(false); // TODO
  }

  auto tracks = moov->get_child_boxes<Box_trak>();
  for (const auto& track_box : tracks) {
    auto trackResult = Track::alloc_track(this, track_box);
    bool skip_track = false;

    if (auto err = trackResult.error()) {
      if (err.error_code == heif_error_Unsupported_feature &&
          err.sub_error_code == heif_suberror_Unsupported_track_type) {
        // ignore error, skip track
        skip_track = true;
      }
      else {
        return trackResult.error();
      }
    }

    if (!skip_track) {
      assert(*trackResult);
      auto track = *trackResult;
      m_tracks.insert({track->get_id(), track});

      if (track->is_visual_track() && m_visual_track_id == 0) {
        m_visual_track_id = track->get_id();
      }
    }
  }

  // --- post-parsing initialization

  std::vector<std::shared_ptr<Track>> all_tracks;
  for (auto& track : m_tracks) {
   all_tracks.push_back(track.second);
  }

  for (auto& track : m_tracks) {
    Error err = track.second->initialize_after_parsing(this, all_tracks);
    if (err) {
      return err;
    }
  }

  return Error::Ok;
}


std::vector<uint32_t> HeifContext::get_track_IDs() const
{
  std::vector<uint32_t> ids;

  for (const auto& track : m_tracks) {
    ids.push_back(track.first);
  }

  return ids;
}


Result<std::shared_ptr<Track>> HeifContext::get_track(uint32_t track_id)
{
  assert(has_sequence());

  if (track_id != 0) {
    auto iter = m_tracks.find(track_id);
    if (iter == m_tracks.end()) {
      return Error{heif_error_Usage_error,
                   heif_suberror_Unspecified,
                   "Invalid track id"};
    }

    return iter->second;
  }

  if (m_visual_track_id != 0) {
    return m_tracks[m_visual_track_id];
  }

  return m_tracks.begin()->second;
}


Result<std::shared_ptr<const Track>> HeifContext::get_track(uint32_t track_id) const
{
  auto result = const_cast<HeifContext*>(this)->get_track(track_id);
  if (!result) {
    return result.error();
  }
  else {
    Result<std::shared_ptr<const Track>> my_result(*result);
    return my_result;
  }
}


uint32_t HeifContext::get_sequence_timescale() const
{
  auto mvhd = m_heif_file->get_mvhd_box();
  if (!mvhd) {
    return 0;
  }

  return mvhd->get_time_scale();
}


void HeifContext::set_sequence_timescale(uint32_t timescale)
{
  get_heif_file()->init_for_sequence();

  auto mvhd = m_heif_file->get_mvhd_box();

  /* unnecessary, since mvhd duration is set during writing

  uint32_t old_timescale = mvhd->get_time_scale();
  if (old_timescale != 0) {
    uint64_t scaled_duration = mvhd->get_duration() * timescale / old_timescale;
    mvhd->set_duration(scaled_duration);
  }
  */

  mvhd->set_time_scale(timescale);
}


void HeifContext::set_number_of_sequence_repetitions(uint32_t repetitions)
{
  m_sequence_repetitions = repetitions;
}


uint64_t HeifContext::get_sequence_duration() const
{
  auto mvhd = m_heif_file->get_mvhd_box();
  if (!mvhd) {
    return 0;
  }

  return mvhd->get_duration();
}


Result<std::shared_ptr<Track_Visual>> HeifContext::add_visual_sequence_track(const TrackOptions* options,
                                                                             uint32_t handler_type,
                                                                             uint16_t width, uint16_t height)
{
  m_heif_file->init_for_sequence();

  std::shared_ptr<Track_Visual> trak = std::make_shared<Track_Visual>(this, 0, width, height, options, handler_type);
  m_tracks.insert({trak->get_id(), trak});

  return trak;
}


Result<std::shared_ptr<class Track_Metadata>> HeifContext::add_uri_metadata_sequence_track(const TrackOptions* options,
                                                                                           std::string uri)
{
  m_heif_file->init_for_sequence();

  std::shared_ptr<Track_Metadata> trak = std::make_shared<Track_Metadata>(this, 0, uri, options);
  m_tracks.insert({trak->get_id(), trak});

  return trak;
}

std::shared_ptr<TextItem> HeifContext::add_text_item(const char* content_type, const char* text)
{
  std::shared_ptr<Box_infe> box = m_heif_file->add_new_infe_box(fourcc("mime"));
  box->set_hidden_item(true);
  box->set_content_type(std::string(content_type));
  auto textItem = std::make_shared<TextItem>(box->get_item_ID(), text);
  add_text_item(textItem);
  return textItem;
}
