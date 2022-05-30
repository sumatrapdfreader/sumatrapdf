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

#include "error.h"

#include <cassert>

// static
const char heif::Error::kSuccess[] = "Success";
const char* cUnknownError = "Unknown error";


heif::Error::Error() = default;


heif::Error::Error(heif_error_code c,
                   heif_suberror_code sc,
                   const std::string& msg)
    : error_code(c),
      sub_error_code(sc),
      message(msg)
{
}


const char* heif::Error::get_error_string(heif_error_code err)
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
  }

  assert(false);
  return "Unknown error";
}

const char* heif::Error::get_error_string(heif_suberror_code err)
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
    case heif_suberror_No_av1C_box:
      return "No 'av1C' box";
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


      // --- Memory_allocation_error ---

    case heif_suberror_Security_limit_exceeded:
      return "Security limit exceeded";

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

      // --- Encoder_plugin_error --

    case heif_suberror_Unsupported_bit_depth:
      return "Unsupported bit depth";

      // --- Encoding_error --

    case heif_suberror_Cannot_write_output_data:
      return "Cannot write output data";
  }

  assert(false);
  return cUnknownError;
}


heif_error heif::Error::error_struct(ErrorBuffer* error_buffer) const
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
