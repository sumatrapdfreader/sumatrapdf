/*
 * HEIF codec.
 * Copyright (c) 2025 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_HEIF_SEQUENCES_H
#define LIBHEIF_HEIF_SEQUENCES_H

#include "libheif/heif.h"

#ifdef __cplusplus
extern "C" {
#endif

// forward declaration of structs defined in heif_tai_timestamps.h
typedef struct heif_tai_clock_info heif_tai_clock_info;
typedef struct heif_tai_timestamp_packet heif_tai_timestamp_packet;


// --- reading sequence tracks

/**
 * Check whether there is an image sequence in the HEIF file.
 *
 * @return A boolean whether there is an image sequence in the HEIF file.
 */
LIBHEIF_API
int heif_context_has_sequence(const heif_context*);

/**
 * Get the timescale (clock ticks per second) for timing values in the sequence.
 *
 * @note Each track may have its independent timescale.
 *
 * @return Clock ticks per second. Returns 0 if there is no sequence in the file.
 */
LIBHEIF_API
uint32_t heif_context_get_sequence_timescale(const heif_context*);

/**
 * Get the total duration of the sequence in timescale clock ticks.
 * Use `heif_context_get_sequence_timescale()` to get the clock ticks per second.
 *
 * @return Sequence duration in clock ticks. Returns 0 if there is no sequence in the file.
 */
LIBHEIF_API
uint64_t heif_context_get_sequence_duration(const heif_context*);


// A track, which may be an image sequence, a video track or a metadata track.
typedef struct heif_track heif_track;

/**
 * Free a `heif_track` object received from libheif.
 * Passing NULL is ok.
 */
LIBHEIF_API
void heif_track_release(heif_track*);

/**
 * Get the number of tracks in the HEIF file.
 *
 * @return Number of tracks or 0 if there is no sequence in the HEIF file.
 */
LIBHEIF_API
int heif_context_number_of_sequence_tracks(const heif_context*);

/**
 * Returns the IDs for each of the tracks stored in the HEIF file.
 * The output array must have heif_context_number_of_sequence_tracks() entries.
 */
LIBHEIF_API
void heif_context_get_track_ids(const heif_context* ctx, uint32_t out_track_id_array[]);

/**
 * Get the ID of the passed track.
 * The track ID will never be 0.
 */
LIBHEIF_API
uint32_t heif_track_get_id(const heif_track* track);

/**
 * Get the heif_track object for the given track ID.
 * If you pass `id=0`, the first visual track will be returned.
 * If there is no track with the given ID or if 0 is passed and there is no visual track, NULL will be returned.
 *
 * @note Tracks never have a zero ID. This is why we can use this as a special value to find the first visual track.
 *
 * @param id Track id or 0 for the first visual track.
 * @return heif_track object. You must free this after use.
 */
// Use id=0 for the first visual track.
LIBHEIF_API
heif_track* heif_context_get_track(const heif_context*, uint32_t id);


typedef uint32_t heif_track_type;

enum heif_track_type_4cc
{
  heif_track_type_video = heif_fourcc('v', 'i', 'd', 'e'),
  heif_track_type_image_sequence = heif_fourcc('p', 'i', 'c', 't'),
  heif_track_type_auxiliary = heif_fourcc('a', 'u', 'x', 'v'),
  heif_track_type_metadata = heif_fourcc('m', 'e', 't', 'a')
};

/**
 * Get the four-cc track handler type.
 * Typical codes are "vide" for video sequences, "pict" for image sequences, "meta" for metadata tracks.
 * These are defined in heif_track_type_4cc, but files may also contain other types.
 *
 * @return four-cc handler type
 */
LIBHEIF_API
heif_track_type heif_track_get_track_handler_type(const heif_track*);


enum heif_auxiliary_track_info_type
{
  heif_auxiliary_track_info_type_unknown = 0,
  heif_auxiliary_track_info_type_alpha = 1
};

LIBHEIF_API
enum heif_auxiliary_track_info_type heif_track_get_auxiliary_info_type(const heif_track*);

LIBHEIF_API
const char* heif_track_get_auxiliary_info_type_urn(const heif_track*);

LIBHEIF_API
int heif_track_has_alpha_channel(const heif_track*);

/**
 * Get the timescale (clock ticks per second) for this track.
 * Note that this can be different from the timescale used at sequence level.
 *
 * @return clock ticks per second
 */
LIBHEIF_API
uint32_t heif_track_get_timescale(const heif_track*);


// --- reading visual tracks

/**
 * Get the image resolution of the track.
 * If the passed track is no visual track, an error is returned.
 */
LIBHEIF_API
heif_error heif_track_get_image_resolution(const heif_track*, uint16_t* out_width, uint16_t* out_height);

/**
 * Decode the next image in the passed sequence track.
 * If there is no more image in the sequence, `heif_error_End_of_sequence` is returned.
 * The parameters `colorspace`, `chroma` and `options` are similar to heif_decode_image().
 * If you want to let libheif decide the output colorspace and chroma, set these parameters
 * to heif_colorspace_undefined / heif_chroma_undefined. Usually, libheif will return the
 * image in the input colorspace, but it may also modify it for example when it has to rotate the image.
 * If you want to get the image in a specific colorspace/chroma format, you can specify this
 * and libheif will convert the image to match this format.
 */
LIBHEIF_API
heif_error heif_track_decode_next_image(heif_track* track,
                                        heif_image** out_img,
                                        enum heif_colorspace colorspace,
                                        enum heif_chroma chroma,
                                        const heif_decoding_options* options);

/**
 * Get the image display duration in clock ticks of this track.
 * Make sure to use the timescale of the track and not the timescale of the total sequence.
 */
LIBHEIF_API
uint32_t heif_image_get_duration(const heif_image*);


// --- reading metadata track samples

/**
 * Get the "sample entry type" of the first sample sample cluster in the track.
 * In the case of metadata tracks, this will usually be "urim" for "URI Meta Sample Entry".
 * The exact URI can then be obtained with 'heif_track_get_urim_sample_entry_uri_of_first_cluster'.
 */
LIBHEIF_API
uint32_t heif_track_get_sample_entry_type_of_first_cluster(const heif_track*);

/**
 * Get the URI of the first sample cluster in an 'urim' track.
 * Only call this for tracks with 'urim' sample entry types. It will return an error otherwise.
 *
 * @param out_uri A string with the URI will be returned. Free this string with `heif_string_release()`.
 */
LIBHEIF_API
heif_error heif_track_get_urim_sample_entry_uri_of_first_cluster(const heif_track*,
                                                                 const char** out_uri);


/** Sequence sample object that can hold any raw byte data.
 * Use this to store and read raw metadata samples.
 */
typedef struct heif_raw_sequence_sample heif_raw_sequence_sample;

/**
 * Get the next raw sample from the (metadata) sequence track.
 * You have to free the returned sample with heif_raw_sequence_sample_release().
 */
LIBHEIF_API
heif_error heif_track_get_next_raw_sequence_sample(heif_track*,
                                                   heif_raw_sequence_sample** out_sample);

/**
 * Release a heif_raw_sequence_sample object.
 * You may pass NULL.
 */
LIBHEIF_API
void heif_raw_sequence_sample_release(heif_raw_sequence_sample*);

/**
 * Get a pointer to the data of the (metadata) sample.
 * The data pointer stays valid until the heif_raw_sequence_sample object is released.
 *
 * @param out_array_size Size of the returned array (may be NULL).
 */
LIBHEIF_API
const uint8_t* heif_raw_sequence_sample_get_data(const heif_raw_sequence_sample*, size_t* out_array_size);

/**
 * Return the size of the raw data contained in the sample.
 * This is the same as returned through the 'out_array_size' parameter of 'heif_raw_sequence_sample_get_data()'.
 */
LIBHEIF_API
size_t heif_raw_sequence_sample_get_data_size(const heif_raw_sequence_sample*);

/**
 * Get the sample duration in clock ticks of this track.
 * Make sure to use the timescale of the track and not the timescale of the total sequence.
 */
LIBHEIF_API
uint32_t heif_raw_sequence_sample_get_duration(const heif_raw_sequence_sample*);


// --- writing sequences

/**
 * Set an independent global timescale for the sequence.
 * If no timescale is set with this function, the timescale of the first track will be used.
 */
LIBHEIF_API
void heif_context_set_sequence_timescale(heif_context*, uint32_t timescale);

// Number of times the sequence should be played in total (default = 1).
// Can be set to heif_sequence_maximum_number_of_repetitions.
LIBHEIF_API
void heif_context_set_number_of_sequence_repetitions(heif_context*, uint32_t number_of_repetitions);

#define heif_sequence_maximum_number_of_repetitions 0

/**
 * Specifies whether a 'sample auxiliary info' is stored with the samples.
 * The difference between `heif_sample_aux_info_presence_optional` and `heif_sample_aux_info_presence_mandatory`
 * is that `heif_sample_aux_info_presence_mandatory` will throw an error if the data is missing when writing a sample.
 */
enum heif_sample_aux_info_presence
{
  heif_sample_aux_info_presence_none = 0,
  heif_sample_aux_info_presence_optional = 1,
  heif_sample_aux_info_presence_mandatory = 2
};


typedef struct heif_track_options heif_track_options;

/**
 * Allocate track options object that is required to set options for a new track.
 * When you create a new track, you can also pass a NULL heif_track_options pointer, in which case the default options are used.
 */
LIBHEIF_API
heif_track_options* heif_track_options_alloc(void);

LIBHEIF_API
void heif_track_options_release(heif_track_options*);

/**
 * Set the track specific timescale. This is the number of clock ticks per second.
 * The default is 90,000 Hz.
 * @param timescale
 */
LIBHEIF_API
void heif_track_options_set_timescale(heif_track_options*, uint32_t timescale);

/**
 * Set whether the aux-info data should be stored interleaved with the sequence samples.
 * Default is: false.
 *
 * If 'true', the aux_info data blocks will be interleaved with the compressed image.
 * This has the advantage that the aux_info is localized near the image data.
 *
 * If 'false', all aux_info will be written as one block after the compressed image data.
 * This has the advantage that no aux_info offsets have to be written.
 *
 * Note: currently ignored. Interleaved writing is disabled.
 */
LIBHEIF_API
void heif_track_options_set_interleaved_sample_aux_infos(heif_track_options*, int interleaved_flag);

LIBHEIF_API
heif_error heif_track_options_enable_sample_tai_timestamps(heif_track_options*,
                                                           const heif_tai_clock_info*,
                                                           enum heif_sample_aux_info_presence);

LIBHEIF_API
void heif_track_options_enable_sample_gimi_content_ids(heif_track_options*,
                                                       enum heif_sample_aux_info_presence);

/**
 * Set the GIMI format track ID string. If NULL is passed, no track ID is saved.
 * @param track_id
 */
LIBHEIF_API
void heif_track_options_set_gimi_track_id(heif_track_options*,
                                          const char* track_id);


// --- writing visual tracks

enum heif_sequence_gop_structure
{
  // Only independently decodable keyframes.
  heif_sequence_gop_structure_intra_only,

  // No frame reordering, usually an IPPPP structure.
  heif_sequence_gop_structure_lowdelay,

  // All frame types are allowed, including frame reordering, to achieve
  // the best compression ratio.
  heif_sequence_gop_structure_unrestricted
};


typedef struct heif_sequence_encoding_options
{
  uint8_t version;

  // version 1 options

  // Set this to the NCLX parameters to be used in the output images or set to NULL
  // when the same parameters as in the input images should be used.
  const heif_color_profile_nclx* output_nclx_profile;

  heif_color_conversion_options color_conversion_options;

  // version 2 options

  enum heif_sequence_gop_structure gop_structure;
  int keyframe_distance_min; // 0 - undefined
  int keyframe_distance_max; // 0 - undefined

  int save_alpha_channel;
} heif_sequence_encoding_options;


LIBHEIF_API
heif_sequence_encoding_options* heif_sequence_encoding_options_alloc(void);

LIBHEIF_API
void heif_sequence_encoding_options_release(heif_sequence_encoding_options*);

/**
 * Add a visual track to the sequence.
 * The track ID is assigned automatically.
 *
 * @param width Image resolution width
 * @param height Image resolution height
 * @param track_type Has to be heif_track_type_video or heif_track_type_image_sequence
 * @param track_options Optional track creation options. If NULL, default options will be used.
 * @param encoding_options Options for sequence encoding. If NULL, default options will be used.
 * @param out_track Output parameter to receive the track object for the just created track.
 * @return
 */
LIBHEIF_API
heif_error heif_context_add_visual_sequence_track(heif_context*,
                                                  uint16_t width, uint16_t height,
                                                  heif_track_type track_type,
                                                  const heif_track_options* track_options,
                                                  const heif_sequence_encoding_options* encoding_options,
                                                  heif_track** out_track);

/**
 * Set the image display duration in the track's timescale units.
 */
LIBHEIF_API
void heif_image_set_duration(heif_image*, uint32_t duration);

/**
 * Encode the image into a visual track.
 * If the passed track is no visual track, an error will be returned.
 *
 * @param image                     The input image to append to the sequence.
 * @param encoder                   The encoder used for encoding the image.
 * @param sequence_encoding_options Options for sequence encoding. If NULL, default options will be used.
 */
LIBHEIF_API
heif_error heif_track_encode_sequence_image(heif_track*,
                                            const heif_image* image,
                                            heif_encoder* encoder,
                                            const heif_sequence_encoding_options* sequence_encoding_options);

/**
 * When all sequence frames have been sent, you can to call this function to let the
 * library know that no more frames will follow. This is strongly recommended, but optional
 * for backwards compatibility.
 * If you do not end the sequence explicitly with this function, it will be closed
 * automatically when the HEIF file is written. Using this function has the advantage
 * that you can free the {@link heif_encoder} afterwards (with {@link heif_encoder_release}).
 * If you do not use call function, you have to keep the {@link heif_encoder} alive
 * until the HEIF file is written.
 */
LIBHEIF_API
heif_error heif_track_encode_end_of_sequence(heif_track*,
                                             heif_encoder* encoder);

// --- metadata tracks

/**
 * Add a metadata track.
 * The track is created as a 'urim' "URI Meta Sample Entry".
 * The track content type is specified by the 'uri' parameter.
 *
 * @param uri       Track content type.
 * @param options   Optional track creation options. If NULL, default options will be used.
 * @param out_track Returns the created track object. If this is not NULL, you have to
 *                  free the returned track with {@link heif_track_release}.
 */
LIBHEIF_API
heif_error heif_context_add_uri_metadata_sequence_track(heif_context*,
                                                        const char* uri,
                                                        const heif_track_options* options,
                                                        heif_track** out_track);

/**
 * Allocate a new heif_raw_sequence_sample object.
 * Free with heif_raw_sequence_sample_release().
 */
LIBHEIF_API
heif_raw_sequence_sample* heif_raw_sequence_sample_alloc(void);

/**
 * Set the raw sequence sample data.
 */
LIBHEIF_API
heif_error heif_raw_sequence_sample_set_data(heif_raw_sequence_sample*, const uint8_t* data, size_t size);

/**
 * Set the sample duration in track timescale units.
 */
LIBHEIF_API
void heif_raw_sequence_sample_set_duration(heif_raw_sequence_sample*, uint32_t duration);

/**
 * Add a raw sequence sample (usually a metadata sample) to the (metadata) track.
 */
LIBHEIF_API
heif_error heif_track_add_raw_sequence_sample(heif_track*,
                                              const heif_raw_sequence_sample*);


// --- sample auxiliary data

/**
 * Contains the type of sample auxiliary data assigned to the track samples.
 */
typedef struct heif_sample_aux_info_type
{
  uint32_t type;
  uint32_t parameter;
} heif_sample_aux_info_type;

/**
 * Returns how many different types of sample auxiliary data units are assigned to this track's samples.
 */
LIBHEIF_API
int heif_track_get_number_of_sample_aux_infos(const heif_track*);

/**
 * Get get the list of sample auxiliary data types used in the track.
 * The passed array has to have heif_track_get_number_of_sample_aux_infos() entries.
 */
LIBHEIF_API
void heif_track_get_sample_aux_info_types(const heif_track*, heif_sample_aux_info_type out_types[]);


// --- GIMI content IDs

/**
 * Get the GIMI content ID for the track (as a whole).
 * If there is no content ID, nullptr is returned.
 *
 * @return The returned string has to be released with `heif_string_release()`.
 */
LIBHEIF_API
const char* heif_track_get_gimi_track_content_id(const heif_track*);

/**
 * Get the GIMI content ID stored in the image sample.
 * If there is no content ID, NULL is returned.
 * @return
 */
LIBHEIF_API
const char* heif_image_get_gimi_sample_content_id(const heif_image*);

/**
 * Get the GIMI content ID stored in the metadata sample.
 * If there is no content ID, NULL is returned.
 * @return
 */
LIBHEIF_API
const char* heif_raw_sequence_sample_get_gimi_sample_content_id(const heif_raw_sequence_sample*);

/**
 * Set the GIMI content ID for an image sample. It will be stored as SAI.
 * When passing NULL, a previously set ID will be removed.
 */
LIBHEIF_API
void heif_image_set_gimi_sample_content_id(heif_image*, const char* contentID);

/**
 * Set the GIMI content ID for a (metadata) sample. It will be stored as SAI.
 * When passing NULL, a previously set ID will be removed.
 */
LIBHEIF_API
void heif_raw_sequence_sample_set_gimi_sample_content_id(heif_raw_sequence_sample*, const char* contentID);


// --- TAI timestamps

// Note: functions for setting timestamps on images are in heif_tai_timestamps.h

/**
 * Returns whether the raw (metadata) sample has a TAI timestamp attached to it (stored as SAI).
 *
 * @return boolean flag whether a TAI exists for this sample.
 */
LIBHEIF_API
int heif_raw_sequence_sample_has_tai_timestamp(const heif_raw_sequence_sample*);

/**
 * Get the TAI timestamp of the (metadata) sample.
 * If there is no timestamp assigned to it, NULL will be returned.
 *
 * @note You should NOT free the returned timestamp with 'heif_tai_timestamp_packet_release()'.
 *       The returned struct stays valid until the heif_raw_sequence_sample is released.
 */
LIBHEIF_API
const heif_tai_timestamp_packet* heif_raw_sequence_sample_get_tai_timestamp(const heif_raw_sequence_sample*);

/**
 * Set the TAI timestamp for a raw sequence sample.
 * The timestamp will be copied, you can release it after calling this function.
 */
LIBHEIF_API
void heif_raw_sequence_sample_set_tai_timestamp(heif_raw_sequence_sample* sample,
                                                const heif_tai_timestamp_packet* timestamp);

/**
 * Returns the TAI clock info of the track.
 * If there is no TAI clock info, NULL is returned.
 * You should NOT free the returned heif_tai_clock_info.
 * The structure stays valid until the heif_track object is released.
 */
LIBHEIF_API
const heif_tai_clock_info* heif_track_get_tai_clock_info_of_first_cluster(heif_track*);


// --- track references

enum heif_track_reference_type
{
  heif_track_reference_type_description = heif_fourcc('c', 'd', 's', 'c'), // track_description
  heif_track_reference_type_thumbnails = heif_fourcc('t', 'h', 'm', 'b'), // thumbnails
  heif_track_reference_type_auxiliary = heif_fourcc('a', 'u', 'x', 'l') // auxiliary data (e.g. depth maps or alpha channel)
};

/**
 * Add a reference between tracks.
 * 'reference_type' can be one of the four-cc codes listed in heif_track_reference_type or any other type.
 */
LIBHEIF_API
void heif_track_add_reference_to_track(heif_track*, uint32_t reference_type, const heif_track* to_track);

/**
 * Return the number of different reference types used in this track's tref box.
 */
LIBHEIF_API
size_t heif_track_get_number_of_track_reference_types(const heif_track*);

/**
 * List the reference types used in this track.
 * The passed array must have heif_track_get_number_of_track_reference_types() entries.
 */
LIBHEIF_API
void heif_track_get_track_reference_types(const heif_track*, uint32_t out_reference_types[]);

/**
 * Get the number of references of the passed type.
 */
LIBHEIF_API
size_t heif_track_get_number_of_track_reference_of_type(const heif_track*, uint32_t reference_type);

/**
 * List the track ids this track points to with the passed reference type.
 * The passed array must have heif_track_get_number_of_track_reference_of_type() entries.
 */
LIBHEIF_API
size_t heif_track_get_references_from_track(const heif_track*, uint32_t reference_type, uint32_t out_to_track_id[]);

/**
 * Find tracks that are referring to the current track through the passed reference_type.
 * The found track IDs will be filled into the passed array, but no more than `array_size` entries will be filled.
 *
 * @return number of tracks found. If this is equal to 'array_size', you should ask again with a larger array size to be sure you got all tracks.
 */
LIBHEIF_API
size_t heif_track_find_referring_tracks(const heif_track*, uint32_t reference_type, uint32_t out_track_id[], size_t array_size);

#ifdef __cplusplus
}
#endif

#endif
