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

#include "error.h"

#include <cassert>
#include <cstring>


const heif_error heif_error_null_pointer_argument {
  heif_error_Usage_error,
  heif_suberror_Null_pointer_argument,
  "NULL argument passed"
};


// static
const char Error::kSuccess[] = "Success";
const char* cUnknownError = "Unknown error";


const Error Error::Ok(heif_error_Ok);

const Error Error::InternalError{heif_error_Unsupported_feature, // TODO: use better value
                                 heif_suberror_Unspecified,
                                 "Internal error"};


Error::Error() = default;


Error::Error(heif_error_code c,
                   heif_suberror_code sc,
                   const std::string& msg)
    : error_code(c),
      sub_error_code(sc),
      message(msg)
{
}


// Replacement for C++20 std::string::starts_with()
static bool starts_with(const std::string& str, const std::string& prefix) {
    if (str.length() < prefix.length()) {
        return false;
    }

  return str.compare(0, prefix.size(), prefix) == 0;
}


Error Error::from_heif_error(const heif_error& c_error)
{
  // unpack the concatenated error message and extract the last part only

  const char* err_string = get_error_string(c_error.code);
  const char* sub_err_string = get_error_string(c_error.subcode);

  std::string msg = c_error.message;
  if (starts_with(msg, err_string)) {
    msg = msg.substr(strlen(err_string));

    if (starts_with(msg, ": ")) {
      msg = msg.substr(2);
    }

    if (starts_with(msg, sub_err_string)) {
      msg = msg.substr(strlen(sub_err_string));

      if (starts_with(msg, ": ")) {
        msg = msg.substr(2);
      }
    }
  }

  return {c_error.code, c_error.subcode, msg};
}


const char* Error::get_error_string(heif_error_code err)
{
  switch (err) {
    case heif_error_Ok:
      return "Success";
    case heif_error_Input_does_not_exist:
      return "Input file does not exist";
    case heif_error_Invalid_input:
      return "Invalid input";
    case heif_error_Unsupported_filetype:
      return "Unsupported file-type";
    case heif_error_Unsupported_feature:
      return "Unsupported feature";
    case heif_error_Usage_error:
      return "Usage error";
    case heif_error_Memory_allocation_error:
      return "Memory allocation error";
    case heif_error_Decoder_plugin_error:
      return "Decoder plugin generated an error";
    case heif_error_Encoder_plugin_error:
      return "Encoder plugin generated an error";
    case heif_error_Encoding_error:
      return "Error during encoding or writing output file";
    case heif_error_Color_profile_does_not_exist:
      return "Color profile does not exist";
    case heif_error_Plugin_loading_error:
      return "Error while loading plugin";
    case heif_error_Canceled:
      return "Canceled by user";
    case heif_error_End_of_sequence:
      return "End of sequence";
  }

  assert(false);
  return "Unknown error";
}

const char* Error::get_error_string(heif_suberror_code err)
{
  switch (err) {
    case heif_suberror_Unspecified:
      return "Unspecified";

      // --- Invalid_input ---

    case heif_suberror_End_of_data:
      return "Unexpected end of file";
    case heif_suberror_Invalid_box_size:
      return "Invalid box size";
    case heif_suberror_Invalid_grid_data:
      return "Invalid grid data";
    case heif_suberror_Missing_grid_images:
      return "Missing grid images";
    case heif_suberror_No_ftyp_box:
      return "No 'ftyp' box";
    case heif_suberror_No_idat_box:
      return "No 'idat' box";
    case heif_suberror_No_meta_box:
      return "No 'meta' box";
    case heif_suberror_No_hdlr_box:
      return "No 'hdlr' box";
    case heif_suberror_No_hvcC_box:
      return "No 'hvcC' box";
    case heif_suberror_No_vvcC_box:
      return "No 'vvcC' box";
    case heif_suberror_No_av1C_box:
      return "No 'av1C' box";
    case heif_suberror_No_avcC_box:
      return "No 'avcC' box";
    case heif_suberror_No_pitm_box:
      return "No 'pitm' box";
    case heif_suberror_No_ipco_box:
      return "No 'ipco' box";
    case heif_suberror_No_ipma_box:
      return "No 'ipma' box";
    case heif_suberror_No_iloc_box:
      return "No 'iloc' box";
    case heif_suberror_No_iinf_box:
      return "No 'iinf' box";
    case heif_suberror_No_iprp_box:
      return "No 'iprp' box";
    case heif_suberror_No_iref_box:
      return "No 'iref' box";
    case heif_suberror_No_infe_box:
      return "No 'infe' box";
    case heif_suberror_No_pict_handler:
      return "Not a 'pict' handler";
    case heif_suberror_Ipma_box_references_nonexisting_property:
      return "'ipma' box references a non-existing property";
    case heif_suberror_No_properties_assigned_to_item:
      return "No properties assigned to item";
    case heif_suberror_No_item_data:
      return "Item has no data";
    case heif_suberror_Invalid_clean_aperture:
      return "Invalid clean-aperture specification";
    case heif_suberror_Invalid_overlay_data:
      return "Invalid overlay data";
    case heif_suberror_Overlay_image_outside_of_canvas:
      return "Overlay image outside of canvas area";
    case heif_suberror_Auxiliary_image_type_unspecified:
      return "Type of auxiliary image unspecified";
    case heif_suberror_No_or_invalid_primary_item:
      return "No or invalid primary item";
    case heif_suberror_Unknown_color_profile_type:
      return "Unknown color profile type";
    case heif_suberror_Wrong_tile_image_chroma_format:
      return "Wrong tile image chroma format";
    case heif_suberror_Invalid_fractional_number:
      return "Invalid fractional number";
    case heif_suberror_Invalid_image_size:
      return "Invalid image size";
    case heif_suberror_Invalid_pixi_box:
      return "Invalid pixi box";
    case heif_suberror_Wrong_tile_image_pixel_depth:
      return "Wrong tile image pixel depth";
    case heif_suberror_Unknown_NCLX_color_primaries:
      return "Unknown NCLX color primaries";
    case heif_suberror_Unknown_NCLX_transfer_characteristics:
      return "Unknown NCLX transfer characteristics";
    case heif_suberror_Unknown_NCLX_matrix_coefficients:
      return "Unknown NCLX matrix coefficients";
    case heif_suberror_Invalid_region_data:
      return "Invalid region item data";
    case heif_suberror_No_ispe_property:
      return "Image has no 'ispe' property";
    case heif_suberror_Camera_intrinsic_matrix_undefined:
      return "Camera intrinsic matrix undefined";
    case heif_suberror_Camera_extrinsic_matrix_undefined:
      return "Camera extrinsic matrix undefined";
    case heif_suberror_Invalid_J2K_codestream:
      return "Invalid JPEG 2000 codestream";
    case heif_suberror_Decompression_invalid_data:
      return "Invalid data in generic compression inflation";
    case heif_suberror_No_moov_box:
      return "No 'moov' box";
    case heif_suberror_No_icbr_box:
      return "No 'icbr' box";
    case heif_suberror_Invalid_mini_box:
      return "Unsupported or invalid 'mini' box";


      // --- Memory_allocation_error ---

    case heif_suberror_Security_limit_exceeded:
      return "Security limit exceeded";
    case heif_suberror_Compression_initialisation_error:
      return "Compression initialisation method error";

      // --- Usage_error ---

    case heif_suberror_Nonexisting_item_referenced:
      return "Non-existing item ID referenced";
    case heif_suberror_Null_pointer_argument:
      return "NULL argument received";
    case heif_suberror_Nonexisting_image_channel_referenced:
      return "Non-existing image channel referenced";
    case heif_suberror_Unsupported_plugin_version:
      return "The version of the passed plugin is not supported";
    case heif_suberror_Unsupported_writer_version:
      return "The version of the passed writer is not supported";
    case heif_suberror_Unsupported_parameter:
      return "Unsupported parameter";
    case heif_suberror_Invalid_parameter_value:
      return "Invalid parameter value";
    case heif_suberror_Invalid_property:
      return "Invalid property";
    case heif_suberror_Item_reference_cycle:
      return "Image reference cycle";

      // --- Unsupported_feature ---

    case heif_suberror_Unsupported_codec:
      return "Unsupported codec";
    case heif_suberror_Unsupported_image_type:
      return "Unsupported image type";
    case heif_suberror_Unsupported_data_version:
      return "Unsupported data version";
    case heif_suberror_Unsupported_color_conversion:
      return "Unsupported color conversion";
    case heif_suberror_Unsupported_item_construction_method:
      return "Unsupported item construction method";
    case heif_suberror_Unsupported_header_compression_method:
      return "Unsupported header compression method";
    case heif_suberror_Unsupported_generic_compression_method:
      return "Unsupported generic compression method";
    case heif_suberror_Unsupported_essential_property:
      return "Unsupported essential item property";
    case heif_suberror_Unsupported_track_type:
      return "Unsupported track type";

      // --- Encoder_plugin_error --

    case heif_suberror_Unsupported_bit_depth:
      return "Unsupported bit depth";

      // --- Encoding_error --

    case heif_suberror_Cannot_write_output_data:
      return "Cannot write output data";
    case heif_suberror_Encoder_initialization:
      return "Initialization problem";
    case heif_suberror_Encoder_encoding:
      return "Encoding problem";
    case heif_suberror_Encoder_cleanup:
      return "Cleanup problem";
    case heif_suberror_Too_many_regions:
      return "Too many regions (>255) in an 'rgan' item.";

      // --- Plugin_loading_error ---

    case heif_suberror_Plugin_loading_error:
      return "Plugin file cannot be loaded";
    case heif_suberror_Plugin_is_not_loaded:
      return "Trying to remove a plugin that is not loaded";
    case heif_suberror_Cannot_read_plugin_directory:
      return "Error while scanning the directory for plugins";
    case heif_suberror_No_matching_decoder_installed:
#if ENABLE_PLUGIN_LOADING
      return "No decoding plugin installed for this compression format";
#else
      return "Support for this compression format has not been built in";
#endif
  }

  assert(false);
  return cUnknownError;
}


heif_error Error::error_struct(ErrorBuffer* error_buffer) const
{
  if (error_buffer) {
    if (error_code == heif_error_Ok) {
      error_buffer->set_success();
    }
    else {
      std::stringstream sstr;
      sstr << get_error_string(error_code) << ": "
           << get_error_string(sub_error_code);
      if (!message.empty()) {
        sstr << ": " << message;
      }

      error_buffer->set_error(sstr.str());
    }
  }

  heif_error err;
  err.code = error_code;
  err.subcode = sub_error_code;
  if (error_buffer) {
    err.message = error_buffer->get_error();
  }
  else {
    err.message = cUnknownError;
  }
  return err;
}
