/*
 * GO interface to libheif
 * Copyright (c) 2018 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of heif, an example application using libheif.
 *
 * heif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * heif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with heif.  If not, see <http://www.gnu.org/licenses/>.
 */

package heif

/*
#cgo pkg-config: libheif
#include <stdlib.h>
#include <string.h> // We use 'memcpy'
#include <libheif/heif.h>
*/
import "C"

import (
	"fmt"
	"image"
	"image/color"
	"io"
	"io/ioutil"
	"runtime"
	"unsafe"
)

func GetVersion() string {
	return C.GoString(C.heif_get_version())
}

type Compression C.enum_heif_compression_format

const (
	CompressionUndefined = C.heif_compression_undefined
	CompressionHEVC      = C.heif_compression_HEVC
	CompressionAV1       = C.heif_compression_AV1
	CompressionAVC       = C.heif_compression_AVC
	CompressionJPEG      = C.heif_compression_JPEG
)

type Chroma C.enum_heif_chroma

const (
	ChromaUndefined              = C.heif_chroma_undefined
	ChromaMonochrome             = C.heif_chroma_monochrome
	Chroma420                    = C.heif_chroma_420
	Chroma422                    = C.heif_chroma_422
	Chroma444                    = C.heif_chroma_444
	ChromaInterleavedRGB         = C.heif_chroma_interleaved_RGB
	ChromaInterleavedRGBA        = C.heif_chroma_interleaved_RGBA
	ChromaInterleavedRRGGBBAA_BE = C.heif_chroma_interleaved_RRGGBBAA_BE
	ChromaInterleavedRRGGBBAA_LE = C.heif_chroma_interleaved_RRGGBBAA_LE
	ChromaInterleavedRRGGBB_BE   = C.heif_chroma_interleaved_RRGGBB_BE
	ChromaInterleavedRRGGBB_LE   = C.heif_chroma_interleaved_RRGGBB_LE

	ChromaInterleaved24Bit = C.heif_chroma_interleaved_24bit
	ChromaInterleaved32Bit = C.heif_chroma_interleaved_32bit
)

type Colorspace C.enum_heif_colorspace

const (
	ColorspaceUndefined  = C.heif_colorspace_undefined
	ColorspaceYCbCr      = C.heif_colorspace_YCbCr
	ColorspaceRGB        = C.heif_colorspace_RGB
	ColorspaceMonochrome = C.heif_colorspace_monochrome
)

type Channel C.enum_heif_channel

const (
	ChannelY           = C.heif_channel_Y
	ChannelCb          = C.heif_channel_Cb
	ChannelCr          = C.heif_channel_Cr
	ChannelR           = C.heif_channel_R
	ChannelG           = C.heif_channel_G
	ChannelB           = C.heif_channel_B
	ChannelAlpha       = C.heif_channel_Alpha
	ChannelInterleaved = C.heif_channel_interleaved
)

type ProgressStep C.enum_heif_progress_step

const (
	ProgressStepTotal    = C.heif_progress_step_total
	ProgressStepLoadTile = C.heif_progress_step_load_tile
)

type LosslessMode int

const (
	LosslessModeDisabled LosslessMode = iota
	LosslessModeEnabled
)

type LoggingLevel int

const (
	LoggingLevelNone LoggingLevel = iota
	LoggingLevelBasic
	LoggingLevelAdvanced
	LoggingLevelFull
)

// --- HeifError

type ErrorCode C.enum_heif_error_code

const (
	ErrorOK = C.heif_error_Ok

	// Input file does not exist.
	ErrorInputDoesNotExist = C.heif_error_Input_does_not_exist

	// Error in input file. Corrupted or invalid content.
	errorInvalidInput = C.heif_error_Invalid_input

	// Input file type is not supported.
	ErrorUnsupportedFiletype = C.heif_error_Unsupported_filetype

	// Image requires an unsupported decoder feature.
	ErrorUnsupportedFeature = C.heif_error_Unsupported_feature

	// Library API has been used in an invalid way.
	ErrorUsage = C.heif_error_Usage_error

	// Could not allocate enough memory.
	ErrorMemoryAllocation = C.heif_error_Memory_allocation_error

	// The decoder plugin generated an error
	ErrorDecoderPlugin = C.heif_error_Decoder_plugin_error

	// The decoder plugin generated an error
	ErrorEncoderPlugin = C.heif_error_Encoder_plugin_error

	// Error during encoding or when writing to the output
	ErrorEncoding = C.heif_error_Encoding_error

	// Application has asked for a color profile type that does not exist
	ErrorColorProfileDoesNotExist = C.heif_error_Color_profile_does_not_exist
)

type ErrorSubcode C.enum_heif_suberror_code

const (
	// no further information available
	SuberrorUnspecified = C.heif_suberror_Unspecified

	// --- Invalid_input ---

	// End of data reached unexpectedly.
	SuberrorEndOfData = C.heif_suberror_End_of_data

	// Size of box (defined in header) is wrong
	SuberrorInvalidBoxSize = C.heif_suberror_Invalid_box_size

	// Mandatory 'ftyp' box is missing
	SuberrorNoFtypBox = C.heif_suberror_No_ftyp_box

	SuberrorNoIdatBox = C.heif_suberror_No_idat_box

	SuberrorNoMetaBox = C.heif_suberror_No_meta_box

	SuberrorNoHdlrBox = C.heif_suberror_No_hdlr_box

	SuberrorNoHvcCBox = C.heif_suberror_No_hvcC_box

	SuberrorNoPitmBox = C.heif_suberror_No_pitm_box

	SuberrorNoIpcoBox = C.heif_suberror_No_ipco_box

	SuberrorNoIpmaBox = C.heif_suberror_No_ipma_box

	SuberrorNoIlocBox = C.heif_suberror_No_iloc_box

	SuberrorNoIinfBox = C.heif_suberror_No_iinf_box

	SuberrorNoIprpBox = C.heif_suberror_No_iprp_box

	SuberrorNoIrefBox = C.heif_suberror_No_iref_box

	SuberrorNoPictHandler = C.heif_suberror_No_pict_handler

	// An item property referenced in the 'ipma' box is not existing in the 'ipco' container.
	SuberrorIpmaBoxReferencesNonexistingProperty = C.heif_suberror_Ipma_box_references_nonexisting_property

	// No properties have been assigned to an item.
	SuberrorNoPropertiesAssignedToItem = C.heif_suberror_No_properties_assigned_to_item

	// Image has no (compressed) data
	SuberrorNoItemData = C.heif_suberror_No_item_data

	// Invalid specification of image grid (tiled image)
	SuberrorInvalidGridData = C.heif_suberror_Invalid_grid_data

	// Tile-images in a grid image are missing
	SuberrorMissingGridImages = C.heif_suberror_Missing_grid_images

	SuberrorNoAV1CBox = C.heif_suberror_No_av1C_box

	SuberrorInvalidCleanAperture = C.heif_suberror_Invalid_clean_aperture

	// Invalid specification of overlay image
	SuberrorInvalidOverlayData = C.heif_suberror_Invalid_overlay_data

	// Overlay image completely outside of visible canvas area
	SuberrorOverlayImageOutsideOfCanvas = C.heif_suberror_Overlay_image_outside_of_canvas

	SuberrorAuxiliaryImageTypeUnspecified = C.heif_suberror_Auxiliary_image_type_unspecified

	SuberrorNoOrInvalidPrimaryItem = C.heif_suberror_No_or_invalid_primary_item

	SuberrorNoInfeBox = C.heif_suberror_No_infe_box

	SuberrorUnknownColorProfileType = C.heif_suberror_Unknown_color_profile_type

	SuberrorWrongTileImageChromaFormat = C.heif_suberror_Wrong_tile_image_chroma_format

	SuberrorInvalidFractionalNumber = C.heif_suberror_Invalid_fractional_number

	SuberrorInvalidImageSize = C.heif_suberror_Invalid_image_size

	// --- Memory_allocation_error ---

	// A security limit preventing unreasonable memory allocations was exceeded by the input file.
	// Please check whether the file is valid. If it is, contact us so that we could increase the
	// security limits further.
	SuberrorSecurityLimitExceeded = C.heif_suberror_Security_limit_exceeded

	// --- Usage_error ---

	// An item ID was used that is not present in the file.
	SuberrorNonexistingItemReferenced = C.heif_suberror_Nonexisting_item_referenced // also used for Invalid_input

	// An API argument was given a NULL pointer, which is not allowed for that function.
	SuberrorNullPointerArgument = C.heif_suberror_Null_pointer_argument

	// Image channel referenced that does not exist in the image
	SuberrorNonexistingImageChannelReferenced = C.heif_suberror_Nonexisting_image_channel_referenced

	// The version of the passed plugin is not supported.
	SuberrorUnsupportedPluginVersion = C.heif_suberror_Unsupported_plugin_version

	// The version of the passed writer is not supported.
	SuberrorUnsupportedWriterVersion = C.heif_suberror_Unsupported_writer_version

	// The given (encoder) parameter name does not exist.
	SuberrorUnsupportedParameter = C.heif_suberror_Unsupported_parameter

	// The value for the given parameter is not in the valid range.
	SuberrorInvalidParameterValue = C.heif_suberror_Invalid_parameter_value

	SuberrorInvalidPixiBox = C.heif_suberror_Invalid_pixi_box

	SuberrorWrongTileImagePixelDepth = C.heif_suberror_Wrong_tile_image_pixel_depth

	SuberrorUnknownNCLXColorPrimaries = C.heif_suberror_Unknown_NCLX_color_primaries

	SuberrorUnknownNCLXTransferCharacteristics = C.heif_suberror_Unknown_NCLX_transfer_characteristics

	SuberrorUnknownNCLXMatrixCoefficients = C.heif_suberror_Unknown_NCLX_matrix_coefficients

	// --- Unsupported_feature ---

	// Image was coded with an unsupported compression method.
	SuberrorUnsupportedCodec = C.heif_suberror_Unsupported_codec

	// Image is specified in an unknown way, e.g. as tiled grid image (which is supported)
	SuberrorUnsupportedImageType = C.heif_suberror_Unsupported_image_type

	SuberrorUnsupportedDataVersion = C.heif_suberror_Unsupported_data_version

	// The conversion of the source image to the requested chroma / colorspace is not supported.
	SuberrorUnsupportedColorConversion = C.heif_suberror_Unsupported_color_conversion

	SuberrorUnsupportedItemConstructionMethod = C.heif_suberror_Unsupported_item_construction_method

	// --- Encoder_plugin_error ---

	SuberrorUnsupportedBitDepth = C.heif_suberror_Unsupported_bit_depth

	// --- Encoding_error ---

	SuberrorCannotWriteOutputData = C.heif_suberror_Cannot_write_output_data
)

type HeifError struct {
	Code    ErrorCode
	Subcode ErrorSubcode
	Message string
}

func (e *HeifError) Error() string {
	return e.Message
}

func convertHeifError(cerror C.struct_heif_error) error {
	if cerror.code == ErrorOK {
		return nil
	}

	return &HeifError{
		Code:    ErrorCode(cerror.code),
		Subcode: ErrorSubcode(cerror.subcode),
		Message: C.GoString(cerror.message),
	}
}

func convertItemIDs(ids []C.heif_item_id, count int) []int {
	result := make([]int, count)
	for i := 0; i < count; i++ {
		result[i] = int(ids[i])
	}
	return result
}

// --- Context

type Context struct {
	context *C.struct_heif_context
}

func NewContext() (*Context, error) {
	ctx := &Context{
		context: C.heif_context_alloc(),
	}
	if ctx.context == nil {
		return nil, fmt.Errorf("Could not allocate context")
	}

	runtime.SetFinalizer(ctx, freeHeifContext)
	return ctx, nil
}

func freeHeifContext(c *Context) {
	C.heif_context_free(c.context)
	c.context = nil
}

func (c *Context) ReadFromFile(filename string) error {
	c_filename := C.CString(filename)
	defer C.free(unsafe.Pointer(c_filename))

	err := C.heif_context_read_from_file(c.context, c_filename, nil)
	keepAlive(c)
	return convertHeifError(err)
}

func (c *Context) ReadFromMemory(data []byte) error {
	// TODO: Use reader API internally.
	err := C.heif_context_read_from_memory(c.context, unsafe.Pointer(&data[0]), C.size_t(len(data)), nil)
	keepAlive(c)
	return convertHeifError(err)
}

type Encoder struct {
	encoder *C.struct_heif_encoder
	id      string
	name    string
}

func (e *Encoder) ID() string {
	return e.id
}

func (e *Encoder) Name() string {
	return e.name
}

func (e *Encoder) SetQuality(q int) error {
	err := C.heif_encoder_set_lossy_quality(e.encoder, C.int(q))
	keepAlive(e)
	return convertHeifError(err)
}

func (e *Encoder) SetLossless(l LosslessMode) error {
	err := C.heif_encoder_set_lossless(e.encoder, C.int(l))
	keepAlive(e)
	return convertHeifError(err)
}

func (e *Encoder) SetLoggingLevel(l LoggingLevel) error {
	err := C.heif_encoder_set_logging_level(e.encoder, C.int(l))
	keepAlive(e)
	return convertHeifError(err)
}

func freeHeifEncoder(enc *Encoder) {
	C.heif_encoder_release(enc.encoder)
	enc.encoder = nil
}

func (c *Context) convertEncoderDescriptor(d *C.struct_heif_encoder_descriptor) (*Encoder, error) {
	cid := C.heif_encoder_descriptor_get_id_name(d)
	cname := C.heif_encoder_descriptor_get_name(d)
	enc := &Encoder{
		id:   C.GoString(cid),
		name: C.GoString(cname),
	}
	err := C.heif_context_get_encoder(c.context, d, &enc.encoder)
	keepAlive(c)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}
	runtime.SetFinalizer(enc, freeHeifEncoder)
	return enc, nil
}

func (c *Context) NewEncoder(compression Compression) (*Encoder, error) {
	const max = 1
	descriptors := make([]*C.struct_heif_encoder_descriptor, max)
	num := int(C.heif_context_get_encoder_descriptors(c.context, uint32(compression), nil, &descriptors[0], C.int(max)))
	keepAlive(c)
	if num == 0 {
		return nil, fmt.Errorf("no encoder for compression %v", compression)
	}
	return c.convertEncoderDescriptor(descriptors[0])
}

func (c *Context) WriteToFile(filename string) error {
	err := C.heif_context_write_to_file(c.context, C.CString(filename))
	keepAlive(c)
	return convertHeifError(err)
}

func (c *Context) GetNumberOfTopLevelImages() int {
	i := int(C.heif_context_get_number_of_top_level_images(c.context))
	keepAlive(c)
	return i
}

func (c *Context) IsTopLevelImageID(ID int) bool {
	ok := C.heif_context_is_top_level_image_ID(c.context, C.heif_item_id(ID)) != 0
	keepAlive(c)
	return ok
}

func (c *Context) GetListOfTopLevelImageIDs() []int {
	num := int(C.heif_context_get_number_of_top_level_images(c.context))
	keepAlive(c)
	if num == 0 {
		return []int{}
	}

	origIDs := make([]C.heif_item_id, num)
	C.heif_context_get_list_of_top_level_image_IDs(c.context, &origIDs[0], C.int(num))
	keepAlive(c)
	return convertItemIDs(origIDs, num)
}

func (c *Context) GetPrimaryImageID() (int, error) {
	var id C.heif_item_id
	err := C.heif_context_get_primary_image_ID(c.context, &id)
	keepAlive(c)
	if err := convertHeifError(err); err != nil {
		return 0, err
	}
	return int(id), nil
}

// --- ImageHandle

type ImageHandle struct {
	handle *C.struct_heif_image_handle
}

func freeHeifImageHandle(c *ImageHandle) {
	C.heif_image_handle_release(c.handle)
	c.handle = nil
}

func (c *Context) GetPrimaryImageHandle() (*ImageHandle, error) {
	var handle ImageHandle
	err := C.heif_context_get_primary_image_handle(c.context, &handle.handle)
	keepAlive(c)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}
	runtime.SetFinalizer(&handle, freeHeifImageHandle)
	return &handle, convertHeifError(err)
}

func (c *Context) GetImageHandle(id int) (*ImageHandle, error) {
	var handle ImageHandle
	err := C.heif_context_get_image_handle(c.context, C.heif_item_id(id), &handle.handle)
	keepAlive(c)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}
	runtime.SetFinalizer(&handle, freeHeifImageHandle)
	return &handle, nil
}

func (h *ImageHandle) IsPrimaryImage() bool {
	ok := C.heif_image_handle_is_primary_image(h.handle) != 0
	keepAlive(h)
	return ok
}

func (h *ImageHandle) GetWidth() int {
	i := int(C.heif_image_handle_get_width(h.handle))
	keepAlive(h)
	return i
}

func (h *ImageHandle) GetHeight() int {
	i := int(C.heif_image_handle_get_height(h.handle))
	keepAlive(h)
	return i
}

func (h *ImageHandle) HasAlphaChannel() bool {
	ok := C.heif_image_handle_has_alpha_channel(h.handle) != 0
	keepAlive(h)
	return ok
}

func (h *ImageHandle) HasDepthImage() bool {
	ok := C.heif_image_handle_has_depth_image(h.handle) != 0
	keepAlive(h)
	return ok
}

func (h *ImageHandle) GetNumberOfDepthImages() int {
	i := int(C.heif_image_handle_get_number_of_depth_images(h.handle))
	keepAlive(h)
	return i
}

func (h *ImageHandle) GetListOfDepthImageIDs() []int {
	num := int(C.heif_image_handle_get_number_of_depth_images(h.handle))
	keepAlive(h)
	if num == 0 {
		return []int{}
	}

	origIDs := make([]C.heif_item_id, num)
	C.heif_image_handle_get_list_of_depth_image_IDs(h.handle, &origIDs[0], C.int(num))
	keepAlive(h)
	return convertItemIDs(origIDs, num)
}

func (h *ImageHandle) GetDepthImageHandle(depth_image_id int) (*ImageHandle, error) {
	var handle ImageHandle
	err := C.heif_image_handle_get_depth_image_handle(h.handle, C.heif_item_id(depth_image_id), &handle.handle)
	keepAlive(h)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}

	runtime.SetFinalizer(&handle, freeHeifImageHandle)
	return &handle, nil
}

func (h *ImageHandle) GetNumberOfThumbnails() int {
	i := int(C.heif_image_handle_get_number_of_thumbnails(h.handle))
	keepAlive(h)
	return i
}

func (h *ImageHandle) GetListOfThumbnailIDs() []int {
	num := int(C.heif_image_handle_get_number_of_thumbnails(h.handle))
	keepAlive(h)
	if num == 0 {
		return []int{}
	}

	origIDs := make([]C.heif_item_id, num)
	C.heif_image_handle_get_list_of_thumbnail_IDs(h.handle, &origIDs[0], C.int(num))
	keepAlive(h)
	return convertItemIDs(origIDs, num)
}

func (h *ImageHandle) GetThumbnail(thumbnail_id int) (*ImageHandle, error) {
	var handle ImageHandle
	err := C.heif_image_handle_get_thumbnail(h.handle, C.heif_item_id(thumbnail_id), &handle.handle)
	keepAlive(h)
	runtime.SetFinalizer(&handle, freeHeifImageHandle)
	return &handle, convertHeifError(err)
}

// TODO: EXIF metadata

// --- Image

type DecodingOptions struct {
	options *C.struct_heif_decoding_options
}

func NewDecodingOptions() (*DecodingOptions, error) {
	options := &DecodingOptions{
		options: C.heif_decoding_options_alloc(),
	}
	if options.options == nil {
		return nil, fmt.Errorf("Could not allocate decoding options")
	}

	runtime.SetFinalizer(options, freeHeifDecodingOptions)
	return options, nil
}

func freeHeifDecodingOptions(options *DecodingOptions) {
	C.heif_decoding_options_free(options.options)
	options.options = nil
}

type Image struct {
	image *C.struct_heif_image
}

func NewImage(width, height int, colorspace Colorspace, chroma Chroma) (*Image, error) {
	var image Image
	err := C.heif_image_create(C.int(width), C.int(height), uint32(colorspace), uint32(chroma), &image.image)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}
	runtime.SetFinalizer(&image, freeHeifImage)
	return &image, nil
}

func freeHeifImage(image *Image) {
	C.heif_image_release(image.image)
	image.image = nil
}

func (h *ImageHandle) DecodeImage(colorspace Colorspace, chroma Chroma, options *DecodingOptions) (*Image, error) {
	var image Image

	var opt *C.struct_heif_decoding_options
	if options != nil {
		opt = options.options
	}

	err := C.heif_decode_image(h.handle, &image.image, uint32(colorspace), uint32(chroma), opt)
	keepAlive(h)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}

	runtime.SetFinalizer(&image, freeHeifImage)
	return &image, nil
}

func (img *Image) GetColorspace() Colorspace {
	cs := Colorspace(C.heif_image_get_colorspace(img.image))
	keepAlive(img)
	return cs
}

func (img *Image) GetChromaFormat() Chroma {
	c := Chroma(C.heif_image_get_chroma_format(img.image))
	keepAlive(img)
	return c
}

func (img *Image) GetWidth(channel Channel) int {
	i := int(C.heif_image_get_width(img.image, uint32(channel)))
	keepAlive(img)
	return i
}

func (img *Image) GetHeight(channel Channel) int {
	i := int(C.heif_image_get_height(img.image, uint32(channel)))
	keepAlive(img)
	return i
}

func (img *Image) GetBitsPerPixel(channel Channel) int {
	i := int(C.heif_image_get_bits_per_pixel(img.image, uint32(channel)))
	keepAlive(img)
	return i
}

func (img *Image) GetBitsPerPixelRange(channel Channel) int {
	i := int(C.heif_image_get_bits_per_pixel_range(img.image, uint32(channel)))
	keepAlive(img)
	return i
}

func (img *Image) GetImage() (image.Image, error) {
	var i image.Image
	cf := img.GetChromaFormat()
	switch cs := img.GetColorspace(); cs {
	case ColorspaceYCbCr:
		var subsample image.YCbCrSubsampleRatio
		switch cf {
		case Chroma420:
			subsample = image.YCbCrSubsampleRatio420
		case Chroma422:
			subsample = image.YCbCrSubsampleRatio422
		case Chroma444:
			subsample = image.YCbCrSubsampleRatio444
		default:
			return nil, fmt.Errorf("Unsupported YCbCr chroma format: %v", cf)
		}
		y, err := img.GetPlane(ChannelY)
		if err != nil {
			return nil, err
		}
		cb, err := img.GetPlane(ChannelCb)
		if err != nil {
			return nil, err
		}
		cr, err := img.GetPlane(ChannelCr)
		if err != nil {
			return nil, err
		}
		i = &image.YCbCr{
			Y:              y.Plane,
			Cb:             cb.Plane,
			Cr:             cr.Plane,
			YStride:        y.Stride,
			CStride:        cb.Stride,
			SubsampleRatio: subsample,
			Rect: image.Rectangle{
				Min: image.Point{
					X: 0,
					Y: 0,
				},
				Max: image.Point{
					X: img.GetWidth(ChannelY),
					Y: img.GetHeight(ChannelY),
				},
			},
		}
	case ColorspaceRGB:
		switch cf {
		case Chroma444:
			r, err := img.GetPlane(ChannelR)
			if err != nil {
				return nil, err
			}
			g, err := img.GetPlane(ChannelG)
			if err != nil {
				return nil, err
			}
			b, err := img.GetPlane(ChannelB)
			if err != nil {
				return nil, err
			}
			width := img.GetWidth(ChannelR)
			height := img.GetHeight(ChannelR)
			read_pos_r := 0
			read_pos_g := 0
			read_pos_b := 0
			write_pos := 0
			var rgba []byte
			var stride int
			if bpp := img.GetBitsPerPixelRange(ChannelR); bpp > 8 {
				// NOTE: We only support the same bits per pixel on all components.
				stride = width * 8
				rgba = make([]byte, height*stride)
				stride_add_r := r.Stride - width*2
				stride_add_g := g.Stride - width*2
				stride_add_b := b.Stride - width*2
				if bpp == 16 {
					for y := 0; y < height; y++ {
						for x := 0; x < width; x++ {
							rgba[write_pos] = r.Plane[read_pos_r]
							rgba[write_pos+1] = r.Plane[read_pos_r+1]
							rgba[write_pos+2] = g.Plane[read_pos_g]
							rgba[write_pos+3] = g.Plane[read_pos_g+1]
							rgba[write_pos+4] = b.Plane[read_pos_b]
							rgba[write_pos+5] = b.Plane[read_pos_b+1]
							rgba[write_pos+6] = 0xff
							rgba[write_pos+7] = 0xff
							read_pos_r += 2
							read_pos_g += 2
							read_pos_b += 2
							write_pos += 8
						}
						read_pos_r += stride_add_r
						read_pos_g += stride_add_g
						read_pos_b += stride_add_b
					}
				} else {
					for y := 0; y < height; y++ {
						for x := 0; x < width; x++ {
							r_value := (int16(r.Plane[read_pos_r+1]) << 8) | int16(r.Plane[read_pos_r])
							r_value = (r_value << (16 - uint(bpp))) | (r_value >> (2*uint(bpp) - 16))
							rgba[write_pos] = byte(r_value >> 8)
							rgba[write_pos+1] = byte(r_value & 0xff)
							g_value := (int16(g.Plane[read_pos_g+1]) << 8) | int16(g.Plane[read_pos_g])
							g_value = (g_value << (16 - uint(bpp))) | (g_value >> (2*uint(bpp) - 16))
							rgba[write_pos+2] = byte(g_value >> 8)
							rgba[write_pos+3] = byte(g_value & 0xff)
							b_value := (int16(b.Plane[read_pos_b+1]) << 8) | int16(b.Plane[read_pos_b])
							b_value = (b_value << (16 - uint(bpp))) | (b_value >> (2*uint(bpp) - 16))
							rgba[write_pos+4] = byte(b_value >> 8)
							rgba[write_pos+5] = byte(b_value & 0xff)
							rgba[write_pos+6] = 0xff
							rgba[write_pos+7] = 0xff
							read_pos_r += 2
							read_pos_g += 2
							read_pos_b += 2
							write_pos += 8
						}
						read_pos_r += stride_add_r
						read_pos_g += stride_add_g
						read_pos_b += stride_add_b
					}
				}

				i = &image.RGBA64{
					Pix:    rgba,
					Stride: stride,
					Rect: image.Rectangle{
						Min: image.Point{
							X: 0,
							Y: 0,
						},
						Max: image.Point{
							X: width,
							Y: height,
						},
					},
				}
			} else {
				stride = width * 4
				rgba = make([]byte, height*stride)
				stride_add_r := r.Stride - width
				stride_add_g := g.Stride - width
				stride_add_b := b.Stride - width
				for y := 0; y < height; y++ {
					for x := 0; x < width; x++ {
						rgba[write_pos] = r.Plane[read_pos_r]
						rgba[write_pos+1] = g.Plane[read_pos_g]
						rgba[write_pos+2] = b.Plane[read_pos_b]
						rgba[write_pos+3] = 0xff
						read_pos_r++
						read_pos_g++
						read_pos_b++
						write_pos += 4
					}
					read_pos_r += stride_add_r
					read_pos_g += stride_add_g
					read_pos_b += stride_add_b
				}

				i = &image.RGBA{
					Pix:    rgba,
					Stride: stride,
					Rect: image.Rectangle{
						Min: image.Point{
							X: 0,
							Y: 0,
						},
						Max: image.Point{
							X: width,
							Y: height,
						},
					},
				}
			}
		case ChromaInterleavedRGB:
			rgb, err := img.GetPlane(ChannelInterleaved)
			if err != nil {
				return nil, err
			}
			width := img.GetWidth(ChannelInterleaved)
			height := img.GetHeight(ChannelInterleaved)
			rgba := make([]byte, width*height*4)
			read_pos := 0
			write_pos := 0
			stride_add := rgb.Stride - width*3
			for y := 0; y < height; y++ {
				for x := 0; x < width; x++ {
					rgba[write_pos] = rgb.Plane[read_pos]
					rgba[write_pos+1] = rgb.Plane[read_pos+1]
					rgba[write_pos+2] = rgb.Plane[read_pos+2]
					rgba[write_pos+3] = 0xff
					read_pos += 3
					write_pos += 4
				}
				read_pos += stride_add
			}
			i = &image.RGBA{
				Pix:    rgba,
				Stride: width * 4,
				Rect: image.Rectangle{
					Min: image.Point{
						X: 0,
						Y: 0,
					},
					Max: image.Point{
						X: width,
						Y: height,
					},
				},
			}
		case ChromaInterleavedRGBA:
			rgba, err := img.GetPlane(ChannelInterleaved)
			if err != nil {
				return nil, err
			}
			i = &image.RGBA{
				Pix:    rgba.Plane,
				Stride: rgba.Stride,
				Rect: image.Rectangle{
					Min: image.Point{
						X: 0,
						Y: 0,
					},
					Max: image.Point{
						X: img.GetWidth(ChannelInterleaved),
						Y: img.GetHeight(ChannelInterleaved),
					},
				},
			}
		case ChromaInterleavedRRGGBB_BE:
			rgb, err := img.GetPlane(ChannelInterleaved)
			if err != nil {
				return nil, err
			}
			width := img.GetWidth(ChannelInterleaved)
			height := img.GetHeight(ChannelInterleaved)
			rgba := make([]byte, width*height*8)
			read_pos := 0
			write_pos := 0
			stride_add := rgb.Stride - width*6
			if bpp := img.GetBitsPerPixelRange(ChannelInterleaved); bpp != 16 {
				for y := 0; y < height; y++ {
					for x := 0; x < width; x++ {
						r_value := (int16(rgb.Plane[read_pos]) << 8) | int16(rgb.Plane[read_pos+1])
						r_value = (r_value << (16 - uint(bpp))) | (r_value >> (2*uint(bpp) - 16))
						rgba[write_pos] = byte(r_value >> 8)
						rgba[write_pos+1] = byte(r_value & 0xff)
						g_value := (int16(rgb.Plane[read_pos+2]) << 8) | int16(rgb.Plane[read_pos+3])
						g_value = (g_value << (16 - uint(bpp))) | (g_value >> (2*uint(bpp) - 16))
						rgba[write_pos+2] = byte(g_value >> 8)
						rgba[write_pos+3] = byte(g_value & 0xff)
						b_value := (int16(rgb.Plane[read_pos+4]) << 8) | int16(rgb.Plane[read_pos+5])
						b_value = (b_value << (16 - uint(bpp))) | (b_value >> (2*uint(bpp) - 16))
						rgba[write_pos+4] = byte(b_value >> 8)
						rgba[write_pos+5] = byte(b_value & 0xff)
						rgba[write_pos+6] = 0xff
						rgba[write_pos+7] = 0xff
						read_pos += 6
						write_pos += 8
					}
					read_pos += stride_add
				}
			} else {
				for y := 0; y < height; y++ {
					for x := 0; x < width; x++ {
						rgba[write_pos] = rgb.Plane[read_pos]
						rgba[write_pos+1] = rgb.Plane[read_pos+1]
						rgba[write_pos+2] = rgb.Plane[read_pos+2]
						rgba[write_pos+3] = rgb.Plane[read_pos+3]
						rgba[write_pos+4] = rgb.Plane[read_pos+4]
						rgba[write_pos+5] = rgb.Plane[read_pos+5]
						rgba[write_pos+6] = 0xff
						rgba[write_pos+7] = 0xff
						read_pos += 6
						write_pos += 8
					}
					read_pos += stride_add
				}
			}
			i = &image.RGBA64{
				Pix:    rgba,
				Stride: width * 4,
				Rect: image.Rectangle{
					Min: image.Point{
						X: 0,
						Y: 0,
					},
					Max: image.Point{
						X: width,
						Y: height,
					},
				},
			}
		case ChromaInterleavedRRGGBBAA_BE:
			rgba, err := img.GetPlane(ChannelInterleaved)
			if err != nil {
				return nil, err
			}
			width := img.GetWidth(ChannelInterleaved)
			height := img.GetHeight(ChannelInterleaved)
			var plane []byte
			if bpp := img.GetBitsPerPixelRange(ChannelInterleaved); bpp != 16 {
				read_pos := 0
				write_pos := 0
				stride_add := rgba.Stride - width*8
				plane = make([]byte, width*height*8)
				for y := 0; y < height; y++ {
					for x := 0; x < width; x++ {
						r_value := (int16(rgba.Plane[read_pos]) << 8) | int16(rgba.Plane[read_pos+1])
						r_value = (r_value << (16 - uint(bpp))) | (r_value >> (2*uint(bpp) - 16))
						plane[write_pos] = byte(r_value >> 8)
						plane[write_pos+1] = byte(r_value & 0xff)
						g_value := (int16(rgba.Plane[read_pos+2]) << 8) | int16(rgba.Plane[read_pos+3])
						g_value = (g_value << (16 - uint(bpp))) | (g_value >> (2*uint(bpp) - 16))
						plane[write_pos+2] = byte(g_value >> 8)
						plane[write_pos+3] = byte(g_value & 0xff)
						b_value := (int16(rgba.Plane[read_pos+4]) << 8) | int16(rgba.Plane[read_pos+5])
						b_value = (b_value << (16 - uint(bpp))) | (b_value >> (2*uint(bpp) - 16))
						plane[write_pos+4] = byte(b_value >> 8)
						plane[write_pos+5] = byte(b_value & 0xff)
						a_value := (int16(rgba.Plane[read_pos+6]) << 8) | int16(rgba.Plane[read_pos+7])
						a_value = (a_value << (16 - uint(bpp))) | (a_value >> (2*uint(bpp) - 16))
						plane[write_pos+6] = byte(a_value >> 8)
						plane[write_pos+7] = byte(a_value & 0xff)
						read_pos += 8
						write_pos += 8
					}
					read_pos += stride_add
				}
			} else {
				plane = rgba.Plane
			}
			i = &image.RGBA64{
				Pix:    plane,
				Stride: rgba.Stride,
				Rect: image.Rectangle{
					Min: image.Point{
						X: 0,
						Y: 0,
					},
					Max: image.Point{
						X: width,
						Y: height,
					},
				},
			}
		default:
			return nil, fmt.Errorf("Unsupported RGB chroma format: %v", cf)
		}
	default:
		return nil, fmt.Errorf("Unsupported colorspace: %v", cs)
	}

	return i, nil
}

type ImageAccess struct {
	Plane    []byte
	planePtr unsafe.Pointer
	Stride   int
	height   int

	image *Image // need this reference to make sure the image is not GC'ed while we access it
}

func (i *ImageAccess) setData(data []byte, stride int) {
	// Handle common case directly
	if stride == i.Stride {
		dstP := uintptr(i.planePtr)
		srcP := uintptr(unsafe.Pointer(&data[0]))
		C.memcpy(unsafe.Pointer(dstP), unsafe.Pointer(srcP), C.size_t(i.height*stride))
	} else {
		for y := 0; y < i.height; y++ {
			dstP := uintptr(i.planePtr) + uintptr(y*i.Stride)
			srcP := uintptr(unsafe.Pointer(&data[0])) + uintptr(y*stride)
			C.memcpy(unsafe.Pointer(dstP), unsafe.Pointer(srcP), C.size_t(stride))
		}
	}
	i.Plane = C.GoBytes(i.planePtr, C.int(i.height*i.Stride))
}

func (img *Image) GetPlane(channel Channel) (*ImageAccess, error) {
	height := C.heif_image_get_height(img.image, uint32(channel))
	keepAlive(img)
	if height == -1 {
		return nil, fmt.Errorf("No such channel %v", channel)
	}

	var stride C.int
	plane := C.heif_image_get_plane(img.image, uint32(channel), &stride)
	keepAlive(img)
	if plane == nil {
		return nil, fmt.Errorf("No such channel %v", channel)
	}

	ptr := unsafe.Pointer(plane)
	size := stride * height
	access := &ImageAccess{
		Plane:    C.GoBytes(ptr, size),
		planePtr: ptr,
		Stride:   int(stride),
		height:   int(height),
		image:    img,
	}
	return access, nil
}

func (img *Image) NewPlane(channel Channel, width, height, depth int) (*ImageAccess, error) {
	err := C.heif_image_add_plane(img.image, uint32(channel), C.int(width), C.int(height), C.int(depth))
	keepAlive(img)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}
	return img.GetPlane(channel)
}

func (img *Image) ScaleImage(width int, height int) (*Image, error) {
	var scaled_image Image
	err := C.heif_image_scale_image(img.image, &scaled_image.image, C.int(width), C.int(height), nil)
	keepAlive(img)
	if err := convertHeifError(err); err != nil {
		return nil, err
	}

	runtime.SetFinalizer(&scaled_image, freeHeifImage)
	return &scaled_image, nil
}

// --- High-level encoding API.

type EncodingOptions struct {
	options *C.struct_heif_encoding_options
}

func NewEncodingOptions() (*EncodingOptions, error) {
	options := &EncodingOptions{
		options: C.heif_encoding_options_alloc(),
	}
	if options.options == nil {
		return nil, fmt.Errorf("Could not allocate encoding options")
	}

	runtime.SetFinalizer(options, freeHeifEncodingOptions)
	return options, nil
}

func freeHeifEncodingOptions(options *EncodingOptions) {
	C.heif_encoding_options_free(options.options)
	options.options = nil
}

func imageFromRGBA(i *image.RGBA) (*Image, error) {
	min := i.Bounds().Min
	max := i.Bounds().Max
	w := max.X - min.X
	h := max.Y - min.Y

	out, err := NewImage(w, h, ColorspaceRGB, ChromaInterleavedRGBA)
	if err != nil {
		return nil, fmt.Errorf("failed to create image: %v", err)
	}

	p, err := out.NewPlane(ChannelInterleaved, w, h, 8)
	if err != nil {
		return nil, fmt.Errorf("failed to add plane: %v", err)
	}
	p.setData([]byte(i.Pix), w*4)

	return out, nil
}

func imageFromRGBA64(i *image.RGBA64) (*Image, error) {
	min := i.Bounds().Min
	max := i.Bounds().Max
	w := max.X - min.X
	h := max.Y - min.Y

	out, err := NewImage(w, h, ColorspaceRGB, ChromaInterleavedRRGGBBAA_BE)
	if err != nil {
		return nil, fmt.Errorf("failed to create image: %v", err)
	}

	p, err := out.NewPlane(ChannelInterleaved, w, h, 10)
	if err != nil {
		return nil, fmt.Errorf("failed to add plane: %v", err)
	}

	pix := make([]byte, w*h*8)
	read_pos := 0
	write_pos := 0
	for y := 0; y < h; y++ {
		for x := 0; x < w; x++ {
			r := (uint16(i.Pix[read_pos]) << 8) | uint16(i.Pix[read_pos+1])
			r = r >> 6
			pix[write_pos] = byte(r >> 8)
			pix[write_pos+1] = byte(r & 0xff)
			read_pos += 2
			g := (uint16(i.Pix[read_pos]) << 8) | uint16(i.Pix[read_pos+1])
			g = g >> 6
			pix[write_pos+2] = byte(g >> 8)
			pix[write_pos+3] = byte(g & 0xff)
			read_pos += 2
			b := (uint16(i.Pix[read_pos]) << 8) | uint16(i.Pix[read_pos+1])
			b = b >> 6
			pix[write_pos+4] = byte(b >> 8)
			pix[write_pos+5] = byte(b & 0xff)
			read_pos += 2
			a := (uint16(i.Pix[read_pos]) << 8) | uint16(i.Pix[read_pos+1])
			a = a >> 6
			pix[write_pos+6] = byte(a >> 8)
			pix[write_pos+7] = byte(a & 0xff)
			pix[write_pos+6] = byte(a >> 8)
			pix[write_pos+7] = byte(a & 0xff)
			read_pos += 2
			write_pos += 8
		}
	}
	p.setData(pix, w*8)

	return out, nil
}

func imageFromGray(i *image.Gray) (*Image, error) {
	min := i.Bounds().Min
	max := i.Bounds().Max
	w := max.X - min.X
	h := max.Y - min.Y

	out, err := NewImage(w, h, ColorspaceYCbCr, ChromaMonochrome)
	if err != nil {
		return nil, fmt.Errorf("failed to create image: %v", err)
	}

	const depth = 8
	pY, err := out.NewPlane(ChannelY, w, h, depth)
	if err != nil {
		return nil, fmt.Errorf("failed to add Y plane: %v", err)
	}
	pY.setData([]byte(i.Pix), i.Stride)

	return out, nil
}

func imageFromYCbCr(i *image.YCbCr) (*Image, error) {
	min := i.Bounds().Min
	max := i.Bounds().Max
	w := max.X - min.X
	h := max.Y - min.Y

	var cm Chroma
	switch sr := i.SubsampleRatio; sr {
	case image.YCbCrSubsampleRatio420:
		cm = Chroma420
	default:
		return nil, fmt.Errorf("unsupported subsample ratio: %s", sr.String())
	}

	out, err := NewImage(w, h, ColorspaceYCbCr, cm)
	if err != nil {
		return nil, fmt.Errorf("failed to create image: %v", err)
	}

	const depth = 8
	pY, err := out.NewPlane(ChannelY, w, h, depth)
	if err != nil {
		return nil, fmt.Errorf("failed to add Y plane: %v", err)
	}
	pY.setData([]byte(i.Y), i.YStride)

	// TODO: Might need to be updated for other SubsampleRatio values.
	halfW, halfH := (w+1)/2, (h+1)/2
	pCb, err := out.NewPlane(ChannelCb, halfW, halfH, depth)
	if err != nil {
		return nil, fmt.Errorf("failed to add Cb plane: %v", err)
	}
	pCb.setData([]byte(i.Cb), i.CStride)
	pCr, err := out.NewPlane(ChannelCr, halfW, halfH, depth)
	if err != nil {
		return nil, fmt.Errorf("failed to add Cr plane: %v", err)
	}
	pCr.setData([]byte(i.Cr), i.CStride)

	return out, nil
}

func EncodeFromImage(img image.Image, compression Compression, quality int, lossless LosslessMode, logging LoggingLevel) (*Context, error) {
	var out *Image

	switch i := img.(type) {
	default:
		return nil, fmt.Errorf("unsupported image type: %T", i)
	case *image.RGBA:
		tmp, err := imageFromRGBA(i)
		if err != nil {
			return nil, fmt.Errorf("failed to create image: %v", err)
		}
		out = tmp
	case *image.RGBA64:
		tmp, err := imageFromRGBA64(i)
		if err != nil {
			return nil, fmt.Errorf("failed to create image: %v", err)
		}
		out = tmp
	case *image.Gray:
		tmp, err := imageFromGray(i)
		if err != nil {
			return nil, fmt.Errorf("failed to create image: %v", err)
		}
		out = tmp
	case *image.YCbCr:
		tmp, err := imageFromYCbCr(i)
		if err != nil {
			return nil, fmt.Errorf("failed to create image: %v", err)
		}
		out = tmp
	}

	ctx, err := NewContext()
	if err != nil {
		return nil, fmt.Errorf("failed to create HEIF context: %v", err)
	}

	enc, err := ctx.NewEncoder(compression)
	if err != nil {
		return nil, fmt.Errorf("failed to create encoder: %v", err)
	}

	if err := enc.SetQuality(quality); err != nil {
		return nil, fmt.Errorf("failed to set quality: %v", err)
	}
	if err := enc.SetLossless(lossless); err != nil {
		return nil, fmt.Errorf("failed to set lossless mode: %v", err)
	}
	if err := enc.SetLoggingLevel(logging); err != nil {
		return nil, fmt.Errorf("failed to set logging level: %v", err)
	}

	encOpts, err := NewEncodingOptions()
	if err != nil {
		return nil, fmt.Errorf("failed to get encoding options: %v", err)
	}

	var handle ImageHandle
	err2 := C.heif_context_encode_image(ctx.context, out.image, enc.encoder, encOpts.options, &handle.handle)
	keepAlive(ctx)
	keepAlive(out)
	keepAlive(enc)
	keepAlive(encOpts)
	if err := convertHeifError(err2); err != nil {
		return nil, fmt.Errorf("failed to encode image: %v", err)
	}
	runtime.SetFinalizer(&handle, freeHeifImageHandle)

	return ctx, nil
}

// --- High-level decoding API, always decodes primary image (if present).

func decodePrimaryImageFromReader(r io.Reader) (*ImageHandle, error) {
	ctx, err := NewContext()
	if err != nil {
		return nil, err
	}

	data, err := ioutil.ReadAll(r)
	if err != nil {
		return nil, err
	}

	if err := ctx.ReadFromMemory(data); err != nil {
		return nil, err
	}

	handle, err := ctx.GetPrimaryImageHandle()
	if err != nil {
		return nil, err
	}

	return handle, nil
}

func decodeImage(r io.Reader) (image.Image, error) {
	handle, err := decodePrimaryImageFromReader(r)
	if err != nil {
		return nil, err
	}

	img, err := handle.DecodeImage(ColorspaceUndefined, ChromaUndefined, nil)
	if err != nil {
		return nil, err
	}

	return img.GetImage()
}

func decodeConfig(r io.Reader) (image.Config, error) {
	var config image.Config
	handle, err := decodePrimaryImageFromReader(r)
	if err != nil {
		return config, err
	}

	config = image.Config{
		ColorModel: color.YCbCrModel,
		Width:      handle.GetWidth(),
		Height:     handle.GetHeight(),
	}
	return config, nil
}

func init() {
	image.RegisterFormat("heif", "????ftypheic", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftypheim", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftypheis", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftypheix", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftyphevc", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftyphevm", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftyphevs", decodeImage, decodeConfig)
	image.RegisterFormat("heif", "????ftypmif1", decodeImage, decodeConfig)
	image.RegisterFormat("avif", "????ftypavif", decodeImage, decodeConfig)
	image.RegisterFormat("avif", "????ftypavis", decodeImage, decodeConfig)
}
