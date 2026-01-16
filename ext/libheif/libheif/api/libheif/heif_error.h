/*
 * HEIF codec.
 * Copyright (c) 2017-2023 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_ERROR_H
#define LIBHEIF_HEIF_ERROR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>


enum heif_error_code
{
  // Everything ok, no error occurred.
  heif_error_Ok = 0,

  // Input file does not exist.
  heif_error_Input_does_not_exist = 1,

  // Error in input file. Corrupted or invalid content.
  heif_error_Invalid_input = 2,

  // Input file type is not supported.
  heif_error_Unsupported_filetype = 3,

  // Image requires an unsupported decoder feature.
  heif_error_Unsupported_feature = 4,

  // Library API has been used in an invalid way.
  heif_error_Usage_error = 5,

  // Could not allocate enough memory.
  heif_error_Memory_allocation_error = 6,

  // The decoder plugin generated an error
  heif_error_Decoder_plugin_error = 7,

  // The encoder plugin generated an error
  heif_error_Encoder_plugin_error = 8,

  // Error during encoding or when writing to the output
  heif_error_Encoding_error = 9,

  // Application has asked for a color profile type that does not exist
  heif_error_Color_profile_does_not_exist = 10,

  // Error loading a dynamic plugin
  heif_error_Plugin_loading_error = 11,

  // Operation has been canceled
  heif_error_Canceled = 12,

  heif_error_End_of_sequence = 13
};


enum heif_suberror_code
{
  // no further information available
  heif_suberror_Unspecified = 0,

  // --- Invalid_input ---

  // End of data reached unexpectedly.
  heif_suberror_End_of_data = 100,

  // Size of box (defined in header) is wrong
  heif_suberror_Invalid_box_size = 101,

  // Mandatory 'ftyp' box is missing
  heif_suberror_No_ftyp_box = 102,

  heif_suberror_No_idat_box = 103,

  heif_suberror_No_meta_box = 104,

  heif_suberror_No_hdlr_box = 105,

  heif_suberror_No_hvcC_box = 106,

  heif_suberror_No_pitm_box = 107,

  heif_suberror_No_ipco_box = 108,

  heif_suberror_No_ipma_box = 109,

  heif_suberror_No_iloc_box = 110,

  heif_suberror_No_iinf_box = 111,

  heif_suberror_No_iprp_box = 112,

  heif_suberror_No_iref_box = 113,

  heif_suberror_No_pict_handler = 114,

  // An item property referenced in the 'ipma' box is not existing in the 'ipco' container.
  heif_suberror_Ipma_box_references_nonexisting_property = 115,

  // No properties have been assigned to an item.
  heif_suberror_No_properties_assigned_to_item = 116,

  // Image has no (compressed) data
  heif_suberror_No_item_data = 117,

  // Invalid specification of image grid (tiled image)
  heif_suberror_Invalid_grid_data = 118,

  // Tile-images in a grid image are missing
  heif_suberror_Missing_grid_images = 119,

  heif_suberror_Invalid_clean_aperture = 120,

  // Invalid specification of overlay image
  heif_suberror_Invalid_overlay_data = 121,

  // Overlay image completely outside of visible canvas area
  heif_suberror_Overlay_image_outside_of_canvas = 122,

  heif_suberror_Auxiliary_image_type_unspecified = 123,

  heif_suberror_No_or_invalid_primary_item = 124,

  heif_suberror_No_infe_box = 125,

  heif_suberror_Unknown_color_profile_type = 126,

  heif_suberror_Wrong_tile_image_chroma_format = 127,

  heif_suberror_Invalid_fractional_number = 128,

  heif_suberror_Invalid_image_size = 129,

  heif_suberror_Invalid_pixi_box = 130,

  heif_suberror_No_av1C_box = 131,

  heif_suberror_Wrong_tile_image_pixel_depth = 132,

  heif_suberror_Unknown_NCLX_color_primaries = 133,

  heif_suberror_Unknown_NCLX_transfer_characteristics = 134,

  heif_suberror_Unknown_NCLX_matrix_coefficients = 135,

  // Invalid specification of region item
  heif_suberror_Invalid_region_data = 136,

  // Image has no ispe property
  heif_suberror_No_ispe_property = 137,

  heif_suberror_Camera_intrinsic_matrix_undefined = 138,

  heif_suberror_Camera_extrinsic_matrix_undefined = 139,

  // Invalid JPEG 2000 codestream - usually a missing marker
  heif_suberror_Invalid_J2K_codestream = 140,

  heif_suberror_No_vvcC_box = 141,

  // icbr is only needed in some situations, this error is for those cases
  heif_suberror_No_icbr_box = 142,

  heif_suberror_No_avcC_box = 143,

  // we got a mini box, but could not read it properly
  heif_suberror_Invalid_mini_box = 149,

  // Decompressing generic compression or header compression data failed (e.g. bitstream corruption)
  heif_suberror_Decompression_invalid_data = 150,

  heif_suberror_No_moov_box = 151,

  // --- Memory_allocation_error ---

  // A security limit preventing unreasonable memory allocations was exceeded by the input file.
  // Please check whether the file is valid. If it is, contact us so that we could increase the
  // security limits further.
  heif_suberror_Security_limit_exceeded = 1000,

  // There was an error from the underlying compression / decompression library.
  // One possibility is lack of resources (e.g. memory).
  heif_suberror_Compression_initialisation_error = 1001,

  // --- Usage_error ---

  // An item ID was used that is not present in the file.
  heif_suberror_Nonexisting_item_referenced = 2000, // also used for Invalid_input

  // An API argument was given a NULL pointer, which is not allowed for that function.
  heif_suberror_Null_pointer_argument = 2001,

  // Image channel referenced that does not exist in the image
  heif_suberror_Nonexisting_image_channel_referenced = 2002,

  // The version of the passed plugin is not supported.
  heif_suberror_Unsupported_plugin_version = 2003,

  // The version of the passed writer is not supported.
  heif_suberror_Unsupported_writer_version = 2004,

  // The given (encoder) parameter name does not exist.
  heif_suberror_Unsupported_parameter = 2005,

  // The value for the given parameter is not in the valid range.
  heif_suberror_Invalid_parameter_value = 2006,

  // Error in property specification
  heif_suberror_Invalid_property = 2007,

  // Image reference cycle found in iref
  heif_suberror_Item_reference_cycle = 2008,


  // --- Unsupported_feature ---

  // Image was coded with an unsupported compression method.
  heif_suberror_Unsupported_codec = 3000,

  // Image is specified in an unknown way, e.g. as tiled grid image (which is supported)
  heif_suberror_Unsupported_image_type = 3001,

  heif_suberror_Unsupported_data_version = 3002,

  // The conversion of the source image to the requested chroma / colorspace is not supported.
  heif_suberror_Unsupported_color_conversion = 3003,

  heif_suberror_Unsupported_item_construction_method = 3004,

  heif_suberror_Unsupported_header_compression_method = 3005,

  // Generically compressed data used an unsupported compression method
  heif_suberror_Unsupported_generic_compression_method = 3006,

  heif_suberror_Unsupported_essential_property = 3007,

  heif_suberror_Unsupported_track_type = 3008,

  // --- Encoder_plugin_error ---

  heif_suberror_Unsupported_bit_depth = 4000,


  // --- Encoding_error ---

  heif_suberror_Cannot_write_output_data = 5000,

  heif_suberror_Encoder_initialization = 5001,
  heif_suberror_Encoder_encoding = 5002,
  heif_suberror_Encoder_cleanup = 5003,

  heif_suberror_Too_many_regions = 5004,


  // --- Plugin loading error ---

  heif_suberror_Plugin_loading_error = 6000,         // a specific plugin file cannot be loaded
  heif_suberror_Plugin_is_not_loaded = 6001,         // trying to remove a plugin that is not loaded
  heif_suberror_Cannot_read_plugin_directory = 6002, // error while scanning the directory for plugins
  heif_suberror_No_matching_decoder_installed = 6003 // no decoder found for that compression format
};


typedef struct heif_error
{
  // main error category
  enum heif_error_code code;

  // more detailed error code
  enum heif_suberror_code subcode;

  // textual error message (is always defined, you do not have to check for NULL)
  const char* message;
} heif_error;

// Default success return value. Intended for use in user-supplied callback functions.
LIBHEIF_API extern const heif_error heif_error_success;


#ifdef __cplusplus
}
#endif

#endif
