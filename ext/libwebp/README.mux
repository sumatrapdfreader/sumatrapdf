          __   __  ____  ____  ____  __ __  _     __ __
         /  \\/  \/  _ \/  _ \/  _ \/  \  \/ \___/_ / _\
         \       /   __/  _  \   __/      /  /  (_/  /__
          \__\__/\_____/_____/__/  \__//_/\_____/__/___/v1.0.3


Description:
============

WebPMux: set of two libraries 'Mux' and 'Demux' for creation, extraction and
manipulation of an extended format WebP file, which can have features like
color profile, metadata and animation. Reference command-line tools 'webpmux'
and 'vwebp' as well as the WebP container specification
'doc/webp-container-spec.txt' are also provided in this package.

WebP Mux tool:
==============

The examples/ directory contains a tool (webpmux) for manipulating WebP
files. The webpmux tool can be used to create an extended format WebP file and
also to extract or strip relevant data from such a file.

A list of options is available using the -help command line flag:

> webpmux -help
Usage: webpmux -get GET_OPTIONS INPUT -o OUTPUT
       webpmux -set SET_OPTIONS INPUT -o OUTPUT
       webpmux -duration DURATION_OPTIONS [-duration ...]
               INPUT -o OUTPUT
       webpmux -strip STRIP_OPTIONS INPUT -o OUTPUT
       webpmux -frame FRAME_OPTIONS [-frame...] [-loop LOOP_COUNT]
               [-bgcolor BACKGROUND_COLOR] -o OUTPUT
       webpmux -info INPUT
       webpmux [-h|-help]
       webpmux -version
       webpmux argument_file_name

GET_OPTIONS:
 Extract relevant data:
   icc       get ICC profile
   exif      get EXIF metadata
   xmp       get XMP metadata
   frame n   get nth frame

SET_OPTIONS:
 Set color profile/metadata:
   icc  file.icc     set ICC profile
   exif file.exif    set EXIF metadata
   xmp  file.xmp     set XMP metadata
   where:    'file.icc' contains the ICC profile to be set,
             'file.exif' contains the EXIF metadata to be set
             'file.xmp' contains the XMP metadata to be set

DURATION_OPTIONS:
 Set duration of selected frames:
   duration            set duration for each frames
   duration,frame      set duration of a particular frame
   duration,start,end  set duration of frames in the
                        interval [start,end])
   where: 'duration' is the duration in milliseconds
          'start' is the start frame index
          'end' is the inclusive end frame index
           The special 'end' value '0' means: last frame.

STRIP_OPTIONS:
 Strip color profile/metadata:
   icc       strip ICC profile
   exif      strip EXIF metadata
   xmp       strip XMP metadata

FRAME_OPTIONS(i):
 Create animation:
   file_i +di+[xi+yi[+mi[bi]]]
   where:    'file_i' is the i'th animation frame (WebP format),
             'di' is the pause duration before next frame,
             'xi','yi' specify the image offset for this frame,
             'mi' is the dispose method for this frame (0 or 1),
             'bi' is the blending method for this frame (+b or -b)

LOOP_COUNT:
 Number of times to repeat the animation.
 Valid range is 0 to 65535 [Default: 0 (infinite)].

BACKGROUND_COLOR:
 Background color of the canvas.
  A,R,G,B
  where:    'A', 'R', 'G' and 'B' are integers in the range 0 to 255 specifying
            the Alpha, Red, Green and Blue component values respectively
            [Default: 255,255,255,255]

INPUT & OUTPUT are in WebP format.

Note: The nature of EXIF, XMP and ICC data is not checked and is assumed to be
valid.

Note: if a single file name is passed as the argument, the arguments will be
tokenized from this file. The file name must not start with the character '-'.

Visualization tool:
===================

The examples/ directory also contains a tool (vwebp) for viewing WebP files.
It decodes the image and visualizes it using OpenGL. See the libwebp README
for details on building and running this program.

Mux API:
========
The Mux API contains methods for adding data to and reading data from WebP
files. This API currently supports XMP/EXIF metadata, ICC profile and animation.
Other features may be added in subsequent releases.

Example#1 (pseudo code): Creating a WebPMux object with image data, color
profile and XMP metadata.

  int copy_data = 0;
  WebPMux* mux = WebPMuxNew();
  // ... (Prepare image data).
  WebPMuxSetImage(mux, &image, copy_data);
  // ... (Prepare ICC profile data).
  WebPMuxSetChunk(mux, "ICCP", &icc_profile, copy_data);
  // ... (Prepare XMP metadata).
  WebPMuxSetChunk(mux, "XMP ", &xmp, copy_data);
  // Get data from mux in WebP RIFF format.
  WebPMuxAssemble(mux, &output_data);
  WebPMuxDelete(mux);
  // ... (Consume output_data; e.g. write output_data.bytes to file).
  WebPDataClear(&output_data);


Example#2 (pseudo code): Get image and color profile data from a WebP file.

  int copy_data = 0;
  // ... (Read data from file).
  WebPMux* mux = WebPMuxCreate(&data, copy_data);
  WebPMuxGetFrame(mux, 1, &image);
  // ... (Consume image; e.g. call WebPDecode() to decode the data).
  WebPMuxGetChunk(mux, "ICCP", &icc_profile);
  // ... (Consume icc_profile).
  WebPMuxDelete(mux);
  free(data);


For a detailed Mux API reference, please refer to the header file
(src/webp/mux.h).

Demux API:
==========
The Demux API enables extraction of images and extended format data from
WebP files. This API currently supports reading of XMP/EXIF metadata, ICC
profile and animated images. Other features may be added in subsequent
releases.

Code example: Demuxing WebP data to extract all the frames, ICC profile
and EXIF/XMP metadata.

  WebPDemuxer* demux = WebPDemux(&webp_data);
  uint32_t width = WebPDemuxGetI(demux, WEBP_FF_CANVAS_WIDTH);
  uint32_t height = WebPDemuxGetI(demux, WEBP_FF_CANVAS_HEIGHT);
  // ... (Get information about the features present in the WebP file).
  uint32_t flags = WebPDemuxGetI(demux, WEBP_FF_FORMAT_FLAGS);

  // ... (Iterate over all frames).
  WebPIterator iter;
  if (WebPDemuxGetFrame(demux, 1, &iter)) {
    do {
      // ... (Consume 'iter'; e.g. Decode 'iter.fragment' with WebPDecode(),
      // ... and get other frame properties like width, height, offsets etc.
      // ... see 'struct WebPIterator' below for more info).
    } while (WebPDemuxNextFrame(&iter));
    WebPDemuxReleaseIterator(&iter);
  }

  // ... (Extract metadata).
  WebPChunkIterator chunk_iter;
  if (flags & ICCP_FLAG) WebPDemuxGetChunk(demux, "ICCP", 1, &chunk_iter);
  // ... (Consume the ICC profile in 'chunk_iter.chunk').
  WebPDemuxReleaseChunkIterator(&chunk_iter);
  if (flags & EXIF_FLAG) WebPDemuxGetChunk(demux, "EXIF", 1, &chunk_iter);
  // ... (Consume the EXIF metadata in 'chunk_iter.chunk').
  WebPDemuxReleaseChunkIterator(&chunk_iter);
  if (flags & XMP_FLAG) WebPDemuxGetChunk(demux, "XMP ", 1, &chunk_iter);
  // ... (Consume the XMP metadata in 'chunk_iter.chunk').
  WebPDemuxReleaseChunkIterator(&chunk_iter);
  WebPDemuxDelete(demux);


For a detailed Demux API reference, please refer to the header file
(src/webp/demux.h).

AnimEncoder API:
================
The AnimEncoder API can be used to create animated WebP images.

Code example:

  WebPAnimEncoderOptions enc_options;
  WebPAnimEncoderOptionsInit(&enc_options);
  // ... (Tune 'enc_options' as needed).
  WebPAnimEncoder* enc = WebPAnimEncoderNew(width, height, &enc_options);
  while(<there are more frames>) {
    WebPConfig config;
    WebPConfigInit(&config);
    // ... (Tune 'config' as needed).
    WebPAnimEncoderAdd(enc, frame, duration, &config);
  }
  WebPAnimEncoderAssemble(enc, webp_data);
  WebPAnimEncoderDelete(enc);
  // ... (Write the 'webp_data' to a file, or re-mux it further).


For a detailed AnimEncoder API reference, please refer to the header file
(src/webp/mux.h).

AnimDecoder API:
================
This AnimDecoder API allows decoding (possibly) animated WebP images.

Code Example:

  WebPAnimDecoderOptions dec_options;
  WebPAnimDecoderOptionsInit(&dec_options);
  // Tune 'dec_options' as needed.
  WebPAnimDecoder* dec = WebPAnimDecoderNew(webp_data, &dec_options);
  WebPAnimInfo anim_info;
  WebPAnimDecoderGetInfo(dec, &anim_info);
  for (uint32_t i = 0; i < anim_info.loop_count; ++i) {
    while (WebPAnimDecoderHasMoreFrames(dec)) {
      uint8_t* buf;
      int timestamp;
      WebPAnimDecoderGetNext(dec, &buf, &timestamp);
      // ... (Render 'buf' based on 'timestamp').
      // ... (Do NOT free 'buf', as it is owned by 'dec').
    }
    WebPAnimDecoderReset(dec);
  }
  const WebPDemuxer* demuxer = WebPAnimDecoderGetDemuxer(dec);
  // ... (Do something using 'demuxer'; e.g. get EXIF/XMP/ICC data).
  WebPAnimDecoderDelete(dec);

For a detailed AnimDecoder API reference, please refer to the header file
(src/webp/demux.h).


Bugs:
=====

Please report all bugs to the issue tracker:
    https://bugs.chromium.org/p/webp
Patches welcome! See this page to get started:
    http://www.webmproject.org/code/contribute/submitting-patches/

Discuss:
========

Email: webp-discuss@webmproject.org
Web: http://groups.google.com/a/webmproject.org/group/webp-discuss
