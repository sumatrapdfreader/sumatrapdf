/*
  libheif example application "heif".

  MIT License

  Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <getopt.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <algorithm>
#include <vector>
#include <string>

#include <libheif/heif.h>

#if HAVE_LIBJPEG
extern "C" {
// Prevent duplicate definition for libjpeg-turbo v2.0
// Note: these 'undef's are only a workaround for a libjpeg-turbo-v2.0 bug and
// should be removed again later. Bug has been fixed in libjpeg-turbo-v2.0.1.
#include <jconfig.h>
#if defined(LIBJPEG_TURBO_VERSION_NUMBER) && LIBJPEG_TURBO_VERSION_NUMBER == 2000000
#undef HAVE_STDDEF_H
#undef HAVE_STDLIB_H
#endif
#include <jpeglib.h>
}
#endif

#if HAVE_LIBPNG
extern "C" {
#include <png.h>
}
#endif

#include <assert.h>

#define JPEG_ICC_MARKER  (JPEG_APP0+2)  /* JPEG marker code for ICC */
#define JPEG_ICC_OVERHEAD_LEN  14        /* size of non-profile data in APP2 */

int master_alpha = 1;
int thumb_alpha = 1;
int list_encoders = 0;
int two_colr_boxes = 0;
int premultiplied_alpha = 0;
const char* encoderId = nullptr;

uint16_t nclx_matrix_coefficients = 6;
uint16_t nclx_colour_primaries = 2;
uint16_t nclx_transfer_characteristic = 2;
int nclx_full_range = true;

const int OPTION_NCLX_MATRIX_COEFFICIENTS = 1000;
const int OPTION_NCLX_COLOUR_PRIMARIES = 1001;
const int OPTION_NCLX_TRANSFER_CHARACTERISTIC = 1002;
const int OPTION_NCLX_FULL_RANGE_FLAG = 1003;

static struct option long_options[] = {
    {(char* const) "help",                    no_argument,       0,              'h'},
    {(char* const) "quality",                 required_argument, 0,              'q'},
    {(char* const) "output",                  required_argument, 0,              'o'},
    {(char* const) "lossless",                no_argument,       0,              'L'},
    {(char* const) "thumb",                   required_argument, 0,              't'},
    {(char* const) "verbose",                 no_argument,       0,              'v'},
    {(char* const) "params",                  no_argument,       0,              'P'},
    {(char* const) "no-alpha",                no_argument,       &master_alpha,  0},
    {(char* const) "no-thumb-alpha",          no_argument,       &thumb_alpha,   0},
    {(char* const) "list-encoders",           no_argument,       &list_encoders, 1},
    {(char* const) "encoder",                 no_argument,       0,              'e'},
    {(char* const) "bit-depth",               required_argument, 0,              'b'},
    {(char* const) "even-size",               no_argument,       0,              'E'},
    {(char* const) "avif",                    no_argument,       0,              'A'},
    {(char* const) "matrix_coefficients",     required_argument, 0,              OPTION_NCLX_MATRIX_COEFFICIENTS},
    {(char* const) "colour_primaries",        required_argument, 0,              OPTION_NCLX_COLOUR_PRIMARIES},
    {(char* const) "transfer_characteristic", required_argument, 0,              OPTION_NCLX_TRANSFER_CHARACTERISTIC},
    {(char* const) "full_range_flag",         required_argument, 0,              OPTION_NCLX_FULL_RANGE_FLAG},
    {(char* const) "enable-two-colr-boxes",   no_argument,       &two_colr_boxes, 1},
    {(char* const) "premultiplied-alpha",     no_argument,       &premultiplied_alpha, 1},
    {0, 0,                                                       0,              0}
};

void show_help(const char* argv0)
{
  std::cerr << " heif-enc  libheif version: " << heif_get_version() << "\n"
            << "----------------------------------------\n"
            << "Usage: heif-enc [options] image.jpeg ...\n"
            << "\n"
            << "When specifying multiple source images, they will all be saved into the same HEIF/AVIF file.\n"
            << "\n"
            << "When using the x265 encoder, you may pass it any of its parameters by\n"
            << "prefixing the parameter name with 'x265:'. Hence, to set the 'ctu' parameter,\n"
            << "you will have to set 'x265:ctu' in libheif (e.g.: -p x265:ctu=64).\n"
            << "Note that there is no checking for valid parameters when using the prefix.\n"
            << "\n"
            << "Options:\n"
            << "  -h, --help        show help\n"
            << "  -q, --quality     set output quality (0-100) for lossy compression\n"
            << "  -L, --lossless    generate lossless output (-q has no effect)\n"
            << "  -t, --thumb #     generate thumbnail with maximum size # (default: off)\n"
            << "      --no-alpha    do not save alpha channel\n"
            << "      --no-thumb-alpha  do not save alpha channel in thumbnail image\n"
            << "  -o, --output      output filename (optional)\n"
            << "  -v, --verbose     enable logging output (more -v will increase logging level)\n"
            << "  -P, --params      show all encoder parameters\n"
            << "  -b, --bit-depth # bit-depth of generated HEIF/AVIF file when using 16-bit PNG input (default: 10 bit)\n"
            << "  -p                set encoder parameter (NAME=VALUE)\n"
            << "  -A, --avif        encode as AVIF\n"
            << "      --list-encoders   list all available encoders for the selected output format\n"
            << "  -e, --encoder     ID select encoder to use (the IDs can be listed with --list-encoders)\n"
            << "  -E, --even-size   [deprecated] crop images to even width and height (odd sizes are not decoded correctly by some software)\n"
            << "  --matrix_coefficients     nclx profile: color conversion matrix coefficients, default=6 (see h.273)\n"
            << "  --colour_primaries        nclx profile: color primaries (see h.273)\n"
            << "  --transfer_characteristic nclx profile: transfer characteristics (see h.273)\n"
            << "  --full_range_flag         nclx profile: full range flag, default: 1\n"
            << "  --enable-two-colr-boxes   will write both an ICC and an nclx color profile if both are present\n"
            << "  --premultiplied-alpha     input image has premultiplied alpha\n"
            << "\n"
            << "Note: to get lossless encoding, you need this set of options:\n"
            << "  -L                       switch encoder to lossless mode\n"
            << "  -p chroma=444            switch off chroma subsampling\n"
            << "  --matrix_coefficients=0  encode in RGB color-space\n";
}


#if HAVE_LIBJPEG

static bool JPEGMarkerIsIcc(jpeg_saved_marker_ptr marker)
{
  return
      marker->marker == JPEG_ICC_MARKER &&
      marker->data_length >= JPEG_ICC_OVERHEAD_LEN &&
      /* verify the identifying string */
      GETJOCTET(marker->data[0]) == 0x49 &&
      GETJOCTET(marker->data[1]) == 0x43 &&
      GETJOCTET(marker->data[2]) == 0x43 &&
      GETJOCTET(marker->data[3]) == 0x5F &&
      GETJOCTET(marker->data[4]) == 0x50 &&
      GETJOCTET(marker->data[5]) == 0x52 &&
      GETJOCTET(marker->data[6]) == 0x4F &&
      GETJOCTET(marker->data[7]) == 0x46 &&
      GETJOCTET(marker->data[8]) == 0x49 &&
      GETJOCTET(marker->data[9]) == 0x4C &&
      GETJOCTET(marker->data[10]) == 0x45 &&
      GETJOCTET(marker->data[11]) == 0x0;
}

boolean ReadICCProfileFromJPEG(j_decompress_ptr cinfo,
                               JOCTET** icc_data_ptr,
                               unsigned int* icc_data_len)
{
  jpeg_saved_marker_ptr marker;
  int num_markers = 0;
  int seq_no;
  JOCTET* icc_data;
  unsigned int total_length;
#define MAX_SEQ_NO  255        /* sufficient since marker numbers are bytes */
  char marker_present[MAX_SEQ_NO + 1];      /* 1 if marker found */
  unsigned int data_length[MAX_SEQ_NO + 1]; /* size of profile data in marker */
  unsigned int data_offset[MAX_SEQ_NO + 1]; /* offset for data in marker */

  *icc_data_ptr = NULL;        /* avoid confusion if FALSE return */
  *icc_data_len = 0;

  /* This first pass over the saved markers discovers whether there are
   * any ICC markers and verifies the consistency of the marker numbering.
   */

  for (seq_no = 1; seq_no <= MAX_SEQ_NO; seq_no++)
    marker_present[seq_no] = 0;

  for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
    if (JPEGMarkerIsIcc(marker)) {
      if (num_markers == 0)
        num_markers = GETJOCTET(marker->data[13]);
      else if (num_markers != GETJOCTET(marker->data[13]))
        return FALSE;        /* inconsistent num_markers fields */
      seq_no = GETJOCTET(marker->data[12]);
      if (seq_no <= 0 || seq_no > num_markers)
        return FALSE;        /* bogus sequence number */
      if (marker_present[seq_no])
        return FALSE;        /* duplicate sequence numbers */
      marker_present[seq_no] = 1;
      data_length[seq_no] = marker->data_length - JPEG_ICC_OVERHEAD_LEN;
    }
  }

  if (num_markers == 0)
    return FALSE;

  /* Check for missing markers, count total space needed,
   * compute offset of each marker's part of the data.
   */

  total_length = 0;
  for (seq_no = 1; seq_no <= num_markers; seq_no++) {
    if (marker_present[seq_no] == 0)
      return FALSE;        /* missing sequence number */
    data_offset[seq_no] = total_length;
    total_length += data_length[seq_no];
  }

  if (total_length <= 0)
    return FALSE;        /* found only empty markers? */

  /* Allocate space for assembled data */
  icc_data = (JOCTET*) malloc(total_length * sizeof(JOCTET));
  if (icc_data == NULL)
    return FALSE;        /* oops, out of memory */

  /* and fill it in */
  for (marker = cinfo->marker_list; marker != NULL; marker = marker->next) {
    if (JPEGMarkerIsIcc(marker)) {
      JOCTET FAR* src_ptr;
      JOCTET* dst_ptr;
      unsigned int length;
      seq_no = GETJOCTET(marker->data[12]);
      dst_ptr = icc_data + data_offset[seq_no];
      src_ptr = marker->data + JPEG_ICC_OVERHEAD_LEN;
      length = data_length[seq_no];
      while (length--) {
        *dst_ptr++ = *src_ptr++;
      }
    }
  }

  *icc_data_ptr = icc_data;
  *icc_data_len = total_length;

  return TRUE;
}


std::shared_ptr<heif_image> loadJPEG(const char* filename)
{
  struct heif_image* image = nullptr;


  // ### Code copied from LibVideoGfx and slightly modified to use HeifPixelImage

  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;

  // to store embedded icc profile
  uint32_t iccLen;
  uint8_t* iccBuffer = NULL;

  // open input file

  FILE* infile;
  if ((infile = fopen(filename, "rb")) == NULL) {
    std::cerr << "Can't open " << filename << "\n";
    exit(1);
  }


  // initialize decompressor

  jpeg_create_decompress(&cinfo);

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_stdio_src(&cinfo, infile);

  /* Adding this part to prepare for icc profile reading. */
  jpeg_save_markers(&cinfo, JPEG_APP0 + 2, 0xFFFF);

  jpeg_read_header(&cinfo, TRUE);

  boolean embeddedIccFlag = ReadICCProfileFromJPEG(&cinfo, &iccBuffer, &iccLen);

  if (cinfo.jpeg_color_space == JCS_GRAYSCALE) {
    cinfo.out_color_space = JCS_GRAYSCALE;

    jpeg_start_decompress(&cinfo);

    JSAMPARRAY buffer;
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, cinfo.output_width * cinfo.output_components, 1);


    // create destination image

    struct heif_error err = heif_image_create(cinfo.output_width, cinfo.output_height,
                                              heif_colorspace_monochrome,
                                              heif_chroma_monochrome,
                                              &image);
    (void) err;
    // TODO: handle error

    heif_image_add_plane(image, heif_channel_Y, cinfo.output_width, cinfo.output_height, 8);

    int y_stride;
    uint8_t* py = heif_image_get_plane(image, heif_channel_Y, &y_stride);


    // read the image

    while (cinfo.output_scanline < cinfo.output_height) {
      (void) jpeg_read_scanlines(&cinfo, buffer, 1);

      memcpy(py + (cinfo.output_scanline - 1) * y_stride, *buffer, cinfo.output_width);
    }
  }
  else {
    cinfo.out_color_space = JCS_YCbCr;

    jpeg_start_decompress(&cinfo);

    JSAMPARRAY buffer;
    buffer = (*cinfo.mem->alloc_sarray)
        ((j_common_ptr) &cinfo, JPOOL_IMAGE, cinfo.output_width * cinfo.output_components, 1);


    // create destination image

    struct heif_error err = heif_image_create(cinfo.output_width, cinfo.output_height,
                                              heif_colorspace_YCbCr,
                                              heif_chroma_420,
                                              &image);
    (void) err;

    heif_image_add_plane(image, heif_channel_Y, cinfo.output_width, cinfo.output_height, 8);
    heif_image_add_plane(image, heif_channel_Cb, (cinfo.output_width + 1) / 2, (cinfo.output_height + 1) / 2, 8);
    heif_image_add_plane(image, heif_channel_Cr, (cinfo.output_width + 1) / 2, (cinfo.output_height + 1) / 2, 8);

    int y_stride;
    int cb_stride;
    int cr_stride;
    uint8_t* py = heif_image_get_plane(image, heif_channel_Y, &y_stride);
    uint8_t* pcb = heif_image_get_plane(image, heif_channel_Cb, &cb_stride);
    uint8_t* pcr = heif_image_get_plane(image, heif_channel_Cr, &cr_stride);

    // read the image

    //printf("jpeg size: %d %d\n",cinfo.output_width, cinfo.output_height);

    while (cinfo.output_scanline < cinfo.output_height) {
      JOCTET* bufp;

      (void) jpeg_read_scanlines(&cinfo, buffer, 1);

      bufp = buffer[0];

      int y = cinfo.output_scanline - 1;

      for (unsigned int x = 0; x < cinfo.output_width; x += 2) {
        py[y * y_stride + x] = *bufp++;
        pcb[y / 2 * cb_stride + x / 2] = *bufp++;
        pcr[y / 2 * cr_stride + x / 2] = *bufp++;

        if (x + 1 < cinfo.output_width) {
          py[y * y_stride + x + 1] = *bufp++;
        }

        bufp += 2;
      }


      if (cinfo.output_scanline < cinfo.output_height) {
        (void) jpeg_read_scanlines(&cinfo, buffer, 1);

        bufp = buffer[0];

        y = cinfo.output_scanline - 1;

        for (unsigned int x = 0; x < cinfo.output_width; x++) {
          py[y * y_stride + x] = *bufp++;
          bufp += 2;
        }
      }
    }
  }

  if (embeddedIccFlag && iccLen > 0) {
    heif_image_set_raw_color_profile(image, "prof", iccBuffer, (size_t) iccLen);
  }

  // cleanup
  free(iccBuffer);
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  fclose(infile);

  return std::shared_ptr<heif_image>(image,
                                     [](heif_image* img) { heif_image_release(img); });
}

#else
std::shared_ptr<heif_image> loadJPEG(const char* filename)
{
  std::cerr << "Cannot load JPEG because libjpeg support was not compiled.\n";
  exit(1);

  return nullptr;
}
#endif


#if HAVE_LIBPNG

static void
user_read_fn(png_structp png_ptr, png_bytep data, png_size_t length)
{
  FILE* fh = (FILE*) png_get_io_ptr(png_ptr);
  size_t n = fread((char*) data, length, 1, fh);
  (void) n;
} // user_read_data


std::shared_ptr<heif_image> loadPNG(const char* filename, int output_bit_depth)
{
  FILE* fh = fopen(filename, "rb");
  if (!fh) {
    std::cerr << "Can't open " << filename << "\n";
    exit(1);
  }


  // ### Code copied from LibVideoGfx and slightly modified to use HeifPixelImage

  struct heif_image* image = nullptr;

  png_structp png_ptr;
  png_infop info_ptr;
  png_uint_32 width, height;
  int bit_depth, color_type, interlace_type;
  int compression_type;
  png_charp name;
#if (PNG_LIBPNG_VER < 10500)
  png_charp png_profile_data;
#else
  png_bytep png_profile_data;
#endif
  uint8_t* profile_data = nullptr;
  png_uint_32 profile_length = 5;

  /* Create and initialize the png_struct with the desired error handler
   * functions.  If you want to use the default stderr and longjump method,
   * you can supply NULL for the last three parameters.  We also supply the
   * the compiler header file version, so that we know if the application
   * was compiled with a compatible version of the library.  REQUIRED
   */
  png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  assert(png_ptr != NULL);

  /* Allocate/initialize the memory for image information.  REQUIRED. */
  info_ptr = png_create_info_struct(png_ptr);
  if (info_ptr == NULL) {
    png_destroy_read_struct(&png_ptr, (png_infopp) NULL, (png_infopp) NULL);
    assert(false); // , "could not create info_ptr");
  } // if

  /* Set error handling if you are using the setjmp/longjmp method (this is
   * the normal method of doing things with libpng).  REQUIRED unless you
   * set up your own error handlers in the png_create_read_struct() earlier.
   */
  if (setjmp(png_jmpbuf(png_ptr))) {
    /* Free all of the memory associated with the png_ptr and info_ptr */
    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);
    /* If we get here, we had a problem reading the file */
    assert(false); // , "fatal error in png library");
  } // if

  /* If you are using replacement read functions, instead of calling
   * png_init_io() here you would call: */
  png_set_read_fn(png_ptr, (void*) fh, user_read_fn);
  /* where user_io_ptr is a structure you want available to the callbacks */

  /* The call to png_read_info() gives us all of the information from the
   * PNG file before the first IDAT (image data chunk).  REQUIRED
   */
  png_read_info(png_ptr, info_ptr);

  png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type,
               &interlace_type, NULL, NULL);

  if (png_get_valid(png_ptr, info_ptr, PNG_INFO_iCCP)) {
    if (PNG_INFO_iCCP ==
        png_get_iCCP(png_ptr, info_ptr, &name, &compression_type, &png_profile_data, &profile_length) &&
        profile_length > 0) {
      profile_data = (uint8_t*) malloc(profile_length);
      if (profile_data) {
        memcpy(profile_data, png_profile_data, profile_length);
      }
    }
  }
  /**** Set up the data transformations you want.  Note that these are all
   **** optional.  Only call them if you want/need them.  Many of the
   **** transformations only work on specific types of images, and many
   **** are mutually exclusive.
   ****/

  // \TODO
  //      /* Strip alpha bytes from the input data without combining with the
  //       * background (not recommended).
  //       */
  //      png_set_strip_alpha(png_ptr);

  /* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
   * byte into separate bytes (useful for paletted and grayscale images).
   */
  png_set_packing(png_ptr);


  /* Expand paletted colors into true RGB triplets */
  if (color_type == PNG_COLOR_TYPE_PALETTE)
    png_set_expand(png_ptr);

  /* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
  if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
    png_set_expand(png_ptr);

  /* Set the background color to draw transparent and alpha images over.
   * It is possible to set the red, green, and blue components directly
   * for paletted images instead of supplying a palette index.  Note that
   * even if the PNG file supplies a background, you are not required to
   * use it - you should use the (solid) application background if it has one.
   */

#if 0
  // \TODO 0 is index in color lookup table - correct? used already?
  png_color_16 my_background = {0, 255, 255, 255, 255};
  png_color_16 *image_background;

  if (png_get_bKGD(png_ptr, info_ptr, &image_background))
    png_set_background(png_ptr, image_background, PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
  else
    png_set_background(png_ptr, &my_background, PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0);
#endif


  /* Optional call to gamma correct and add the background to the palette
   * and update info structure.  REQUIRED if you are expecting libpng to
   * update the palette for you (ie you selected such a transform above).
   */
  png_read_update_info(png_ptr, info_ptr);

  /* Allocate the memory to hold the image using the fields of info_ptr. */

  /* The easiest way to read the image: */
  uint8_t** row_pointers = new png_bytep[height];
  assert(row_pointers != NULL);

  for (uint32_t y = 0; y < height; y++) {
    row_pointers[y] = (png_bytep) malloc(png_get_rowbytes(png_ptr, info_ptr));
    assert(row_pointers[y] != NULL);
  } // for

  /* Now it's time to read the image.  One of these methods is REQUIRED */
  png_read_image(png_ptr, row_pointers);

  /* read rest of file, and get additional chunks in info_ptr - REQUIRED */
  png_read_end(png_ptr, info_ptr);

  /* clean up after the read, and free any memory allocated - REQUIRED */
  png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) NULL);


  // OK, now we should have the png image in some way in
  // row_pointers, have fun with it

  int band = 0;
  switch (color_type) {
    case PNG_COLOR_TYPE_GRAY:
    case PNG_COLOR_TYPE_GRAY_ALPHA:
      band = 1;
      break;
    case PNG_COLOR_TYPE_PALETTE:
    case PNG_COLOR_TYPE_RGB:
    case PNG_COLOR_TYPE_RGB_ALPHA:
      band = 3;
      break;
    default:
      assert(false); // , "unknown color type in png image.");
  } // switch




  struct heif_error err;

  bool has_alpha = (color_type & PNG_COLOR_MASK_ALPHA);

  if (band == 1 && bit_depth==8) {
    err = heif_image_create((int) width, (int) height,
                            heif_colorspace_monochrome,
                            heif_chroma_monochrome,
                            &image);
    (void) err;

    heif_image_add_plane(image, heif_channel_Y, (int) width, (int) height, 8);

    int y_stride;
    int a_stride;
    uint8_t* py = heif_image_get_plane(image, heif_channel_Y, &y_stride);
    uint8_t* pa = nullptr;

    if (has_alpha) {
      heif_image_add_plane(image, heif_channel_Alpha, (int) width, (int) height, 8);

      pa = heif_image_get_plane(image, heif_channel_Alpha, &a_stride);
    }


    for (uint32_t y = 0; y < height; y++) {
      uint8_t* p = row_pointers[y];

      if (has_alpha) {
        for (uint32_t x = 0; x < width; x++) {
          py[y * y_stride + x] = *p++;
          pa[y * a_stride + x] = *p++;
        }
      }
      else {
        memcpy(&py[y * y_stride], p, width);
      }
    }
  }
  else if (band == 1) {
    assert(bit_depth>8);

    err = heif_image_create((int) width, (int) height,
                            heif_colorspace_monochrome,
                            heif_chroma_monochrome,
                            &image);
    (void) err;

    int bdShift = 16 - output_bit_depth;

    heif_image_add_plane(image, heif_channel_Y, (int) width, (int) height, output_bit_depth);

    int y_stride;
    int a_stride = 0;
    uint16_t* py = (uint16_t*)heif_image_get_plane(image, heif_channel_Y, &y_stride);
    uint16_t* pa = nullptr;

    if (has_alpha) {
      heif_image_add_plane(image, heif_channel_Alpha, (int) width, (int) height, output_bit_depth);

      pa = (uint16_t*)heif_image_get_plane(image, heif_channel_Alpha, &a_stride);
    }

    y_stride /= 2;
    a_stride /= 2;

    for (uint32_t y = 0; y < height; y++) {
      uint8_t* p = row_pointers[y];

      if (has_alpha) {
        for (uint32_t x = 0; x < width; x++) {
          uint16_t vp = (uint16_t) (((p[0] << 8) | p[1]) >> bdShift);
          uint16_t va = (uint16_t) (((p[2] << 8) | p[3]) >> bdShift);

          py[x + y * y_stride] = vp;
          pa[x + y * y_stride] = va;

          p += 4;
        }
      }
      else {
        for (uint32_t x = 0; x < width; x++) {
          uint16_t vp = (uint16_t) (((p[0] << 8) | p[1]) >> bdShift);

          py[x + y * y_stride] = vp;

          p += 2;
        }
      }
    }
  }
  else if (bit_depth == 8) {
    err = heif_image_create((int) width, (int) height,
                            heif_colorspace_RGB,
                            has_alpha ? heif_chroma_interleaved_RGBA : heif_chroma_interleaved_RGB,
                            &image);
    (void) err;

    heif_image_add_plane(image, heif_channel_interleaved, (int) width, (int) height,
                         has_alpha ? 32 : 24);

    int stride;
    uint8_t* p = heif_image_get_plane(image, heif_channel_interleaved, &stride);

    for (uint32_t y = 0; y < height; y++) {
      if (has_alpha) {
        memcpy(p + y * stride, row_pointers[y], width * 4);
      }
      else {
        memcpy(p + y * stride, row_pointers[y], width * 3);
      }
    }
  }
  else {
    err = heif_image_create((int) width, (int) height,
                            heif_colorspace_RGB,
                            has_alpha ?
                            heif_chroma_interleaved_RRGGBBAA_LE :
                            heif_chroma_interleaved_RRGGBB_LE,
                            &image);
    (void) err;

    int bdShift = 16 - output_bit_depth;

    heif_image_add_plane(image, heif_channel_interleaved, (int) width, (int) height, output_bit_depth);

    int stride;
    uint8_t* p_out = (uint8_t*) heif_image_get_plane(image, heif_channel_interleaved, &stride);

    for (uint32_t y = 0; y < height; y++) {
      uint8_t* p = row_pointers[y];

      uint32_t nVal = (has_alpha ? 4 : 3) * width;

      for (uint32_t x = 0; x < nVal; x++) {
        uint16_t v = (uint16_t) (((p[0] << 8) | p[1]) >> bdShift);
        p_out[2 * x + y * stride + 1] = (uint8_t) (v >> 8);
        p_out[2 * x + y * stride + 0] = (uint8_t) (v & 0xFF);
        p += 2;
      }
    }
  }

  if (profile_data && profile_length > 0) {
    heif_image_set_raw_color_profile(image, "prof", profile_data, (size_t) profile_length);
  }

  free(profile_data);
  for (uint32_t y = 0; y < height; y++) {
    free(row_pointers[y]);
  } // for

  delete[] row_pointers;

  return std::shared_ptr<heif_image>(image,
                                     [](heif_image* img) { heif_image_release(img); });
}

#else
std::shared_ptr<heif_image> loadPNG(const char* filename, int output_bit_depth)
{
  std::cerr << "Cannot load PNG because libpng support was not compiled.\n";
  exit(1);

  return nullptr;
}
#endif


std::shared_ptr<heif_image> loadY4M(const char* filename)
{
  struct heif_image* image = nullptr;


  // open input file

  std::ifstream istr(filename, std::ios_base::binary);
  if (istr.fail()) {
    std::cerr << "Can't open " << filename << "\n";
    exit(1);
  }


  std::string header;
  getline(istr, header);

  if (header.find("YUV4MPEG2 ") != 0) {
    std::cerr << "Input is not a Y4M file.\n";
    exit(1);
  }

  int w = -1;
  int h = -1;

  size_t pos = 0;
  for (;;) {
    pos = header.find(' ', pos + 1) + 1;
    if (pos == std::string::npos) {
      break;
    }

    size_t end = header.find_first_of(" \n", pos + 1);
    if (end == std::string::npos) {
      break;
    }

    if (end - pos <= 1) {
      std::cerr << "Header format error in Y4M file.\n";
      exit(1);
    }

    char tag = header[pos];
    std::string value = header.substr(pos + 1, end - pos - 1);
    if (tag == 'W') {
      w = atoi(value.c_str());
    }
    else if (tag == 'H') {
      h = atoi(value.c_str());
    }
  }

  std::string frameheader;
  getline(istr, frameheader);

  if (frameheader != "FRAME") {
    std::cerr << "Y4M misses the frame header.\n";
    exit(1);
  }

  if (w < 0 || h < 0) {
    std::cerr << "Y4M has invalid frame size.\n";
    exit(1);
  }

  struct heif_error err = heif_image_create(w, h,
                                            heif_colorspace_YCbCr,
                                            heif_chroma_420,
                                            &image);
  (void) err;
  // TODO: handle error

  heif_image_add_plane(image, heif_channel_Y, w, h, 8);
  heif_image_add_plane(image, heif_channel_Cb, (w + 1) / 2, (h + 1) / 2, 8);
  heif_image_add_plane(image, heif_channel_Cr, (w + 1) / 2, (h + 1) / 2, 8);

  int y_stride, cb_stride, cr_stride;
  uint8_t* py = heif_image_get_plane(image, heif_channel_Y, &y_stride);
  uint8_t* pcb = heif_image_get_plane(image, heif_channel_Cb, &cb_stride);
  uint8_t* pcr = heif_image_get_plane(image, heif_channel_Cr, &cr_stride);

  for (int y = 0; y < h; y++) {
    istr.read((char*) (py + y * y_stride), w);
  }

  for (int y = 0; y < (h + 1) / 2; y++) {
    istr.read((char*) (pcb + y * cb_stride), (w + 1) / 2);
  }

  for (int y = 0; y < (h + 1) / 2; y++) {
    istr.read((char*) (pcr + y * cr_stride), (w + 1) / 2);
  }

  return std::shared_ptr<heif_image>(image,
                                     [](heif_image* img) { heif_image_release(img); });
}


void list_encoder_parameters(heif_encoder* encoder)
{
  std::cerr << "Parameters for encoder `" << heif_encoder_get_name(encoder) << "`:\n";

  const struct heif_encoder_parameter* const* params = heif_encoder_list_parameters(encoder);
  for (int i = 0; params[i]; i++) {
    const char* name = heif_encoder_parameter_get_name(params[i]);

    switch (heif_encoder_parameter_get_type(params[i])) {
      case heif_encoder_parameter_type_integer: {
        heif_error error;

        std::cerr << "  " << name;

        if (heif_encoder_has_default(encoder, name)) {
          int value;
          error = heif_encoder_get_parameter_integer(encoder, name, &value);
          (void) error;

          std::cerr << ", default=" << value;
        }

        int have_minimum, have_maximum, minimum, maximum, num_valid_values;
        const int* valid_values = nullptr;
        error = heif_encoder_parameter_integer_valid_values(encoder, name,
                                                            &have_minimum, &have_maximum,
                                                            &minimum, &maximum,
                                                            &num_valid_values,
                                                            &valid_values);

        if (have_minimum || have_maximum) {  // TODO: only one is set
          std::cerr << ", [" << minimum << ";" << maximum << "]";
        }

        if (num_valid_values > 0) {
          std::cerr << ", {";

          for (int p=0;p<num_valid_values;p++) {
            if (p>0) {
              std::cerr << ", ";
            }

            std::cerr << valid_values[p];
          }

          std::cerr << "}";
        }

        std::cerr << "\n";
      }
        break;

      case heif_encoder_parameter_type_boolean: {
        heif_error error;
        std::cerr << "  " << name;

        if (heif_encoder_has_default(encoder, name)) {
          int value;
          error = heif_encoder_get_parameter_boolean(encoder, name, &value);
          (void) error;

          std::cerr << ", default=" << (value ? "true" : "false");
        }

        std::cerr << "\n";
      }
        break;

      case heif_encoder_parameter_type_string: {
        heif_error error;
        std::cerr << "  " << name;

        if (heif_encoder_has_default(encoder, name)) {
          const int value_size = 50;
          char value[value_size];
          error = heif_encoder_get_parameter_string(encoder, name, value, value_size);
          (void) error;

          std::cerr << ", default=" << value;
        }

        const char* const* valid_options;
        error = heif_encoder_parameter_string_valid_values(encoder, name, &valid_options);

        if (valid_options) {
          std::cerr << ", { ";
          for (int i = 0; valid_options[i]; i++) {
            if (i > 0) { std::cerr << ","; }
            std::cerr << valid_options[i];
          }
          std::cerr << " }";
        }

        std::cerr << "\n";
      }
        break;
    }
  }
}


void set_params(struct heif_encoder* encoder, const std::vector<std::string>& params)
{
  for (const std::string& p : params) {
    auto pos = p.find_first_of('=');
    if (pos == std::string::npos || pos == 0 || pos == p.size() - 1) {
      std::cerr << "Encoder parameter must be in the format 'name=value'\n";
      exit(5);
    }

    std::string name = p.substr(0, pos);
    std::string value = p.substr(pos + 1);

    struct heif_error error = heif_encoder_set_parameter(encoder, name.c_str(), value.c_str());
    if (error.code) {
      std::cerr << "Error: " << error.message << "\n";
      exit(5);
    }
  }
}


static void show_list_of_encoders(const heif_encoder_descriptor*const* encoder_descriptors,
                                  int count)
{
  std::cout << "Encoders (first is default):\n";
  for (int i = 0; i < count; i++) {
    std::cout << "- " << heif_encoder_descriptor_get_id_name(encoder_descriptors[i])
              << " = "
              << heif_encoder_descriptor_get_name(encoder_descriptors[i])
              << "\n";
  }
}


int main(int argc, char** argv)
{
  int quality = 50;
  bool lossless = false;
  std::string output_filename;
  int logging_level = 0;
  bool option_show_parameters = false;
  int thumbnail_bbox_size = 0;
  int output_bit_depth = 10;
  bool enc_av1f = false;
  bool crop_to_even_size = false;

  std::vector<std::string> raw_params;


  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "hq:Lo:vPp:t:b:AEe:", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'h':
        show_help(argv[0]);
        return 0;
      case 'q':
        quality = atoi(optarg);
        break;
      case 'L':
        lossless = true;
        break;
      case 'o':
        output_filename = optarg;
        break;
      case 'v':
        logging_level++;
        break;
      case 'P':
        option_show_parameters = true;
        break;
      case 'p':
        raw_params.push_back(optarg);
        break;
      case 't':
        thumbnail_bbox_size = atoi(optarg);
        break;
      case 'b':
        output_bit_depth = atoi(optarg);
        break;
      case 'A':
        enc_av1f = true;
        break;
      case 'E':
        crop_to_even_size = true;
        break;
      case 'e':
        encoderId = optarg;
        break;
      case OPTION_NCLX_MATRIX_COEFFICIENTS:
        nclx_matrix_coefficients = (uint16_t)strtoul(optarg, nullptr, 0);
        break;
      case OPTION_NCLX_COLOUR_PRIMARIES:
        nclx_colour_primaries = (uint16_t)strtoul(optarg, nullptr, 0);
        break;
      case OPTION_NCLX_TRANSFER_CHARACTERISTIC:
        nclx_transfer_characteristic = (uint16_t)strtoul(optarg, nullptr, 0);
        break;
      case OPTION_NCLX_FULL_RANGE_FLAG:
        nclx_full_range = atoi(optarg);
        break;
    }
  }

  if (quality < 0 || quality > 100) {
    std::cerr << "Invalid quality factor. Must be between 0 and 100.\n";
    return 5;
  }

  if (logging_level > 0) {
    logging_level += 2;

    if (logging_level > 4) {
      logging_level = 4;
    }
  }



  // ==============================================================================

  std::shared_ptr<heif_context> context(heif_context_alloc(),
                                        [](heif_context* c) { heif_context_free(c); });
  if (!context) {
    std::cerr << "Could not create context object\n";
    return 1;
  }


  struct heif_encoder* encoder = nullptr;

#define MAX_ENCODERS 5
  const heif_encoder_descriptor* encoder_descriptors[MAX_ENCODERS];
  int count = heif_context_get_encoder_descriptors(context.get(),
                                                   enc_av1f ? heif_compression_AV1 : heif_compression_HEVC,
                                                   nullptr,
                                                   encoder_descriptors, MAX_ENCODERS);

  if (list_encoders) {
    show_list_of_encoders(encoder_descriptors, count);
    return 0;
  }

  const heif_encoder_descriptor* active_encoder_descriptor = nullptr;
  if (count > 0) {
    int idx = 0;
    if (encoderId != nullptr) {
      for (int i = 0; i <= count; i++) {
        if (i==count) {
          std::cerr << "Unknown encoder ID. Choose one from the list below.\n";
          show_list_of_encoders(encoder_descriptors, count);
          return 5;
        }

        if (strcmp(encoderId, heif_encoder_descriptor_get_id_name(encoder_descriptors[i])) == 0) {
          idx = i;
          break;
        }
      }
    }

    heif_error error = heif_context_get_encoder(context.get(), encoder_descriptors[idx], &encoder);
    if (error.code) {
      std::cerr << error.message << "\n";
      return 5;
    }

    active_encoder_descriptor = encoder_descriptors[idx];
  }
  else {
    std::cerr << "No " << (enc_av1f ? "AV1" : "HEVC") << " encoder available.\n";
    return 5;
  }


  if (option_show_parameters) {
    list_encoder_parameters(encoder);
    return 0;
  }


  if (optind > argc - 1) {
    show_help(argv[0]);
    return 0;
  }


  struct heif_error error;

  for (; optind < argc; optind++) {
    std::string input_filename = argv[optind];

    if (output_filename.empty()) {
      std::string filename_without_suffix;
      std::string::size_type dot_position = input_filename.find_last_of('.');
      if (dot_position != std::string::npos) {
        filename_without_suffix = input_filename.substr(0, dot_position);
      }
      else {
        filename_without_suffix = input_filename;
      }

      output_filename = filename_without_suffix + (enc_av1f ? ".avif" : ".heic");
    }


    // ==============================================================================

    // get file type from file name

    std::string suffix;
    auto suffix_pos = input_filename.find_last_of('.');
    if (suffix_pos != std::string::npos) {
      suffix = input_filename.substr(suffix_pos + 1);
      std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
    }

    enum
    {
      PNG, JPEG, Y4M
    } filetype = JPEG;
    if (suffix == "png") {
      filetype = PNG;
    }
    else if (suffix == "y4m") {
      filetype = Y4M;
    }

    std::shared_ptr<heif_image> image;
    if (filetype == PNG) {
      image = loadPNG(input_filename.c_str(), output_bit_depth);
    }
    else if (filetype == Y4M) {
      image = loadY4M(input_filename.c_str());
    }
    else {
      image = loadJPEG(input_filename.c_str());
    }

    heif_color_profile_nclx nclx;
    error = heif_nclx_color_profile_set_matrix_coefficients(&nclx, nclx_matrix_coefficients);
    if (error.code) {
      std::cerr << "Invalid matrix coefficients specified.\n";
      exit(5);
    }
    error = heif_nclx_color_profile_set_transfer_characteristics(&nclx, nclx_transfer_characteristic);
    if (error.code) {
      std::cerr << "Invalid transfer characteristics specified.\n";
      exit(5);
    }
    error = heif_nclx_color_profile_set_color_primaries(&nclx, nclx_colour_primaries);
    if (error.code) {
      std::cerr << "Invalid color primaries specified.\n";
      exit(5);
    }
    nclx.full_range_flag = (uint8_t) nclx_full_range;

    //heif_image_set_nclx_color_profile(image.get(), &nclx);

    if (lossless) {
      if (heif_encoder_descriptor_supportes_lossless_compression(active_encoder_descriptor)) {
        heif_encoder_set_lossless(encoder, lossless);
      }
      else {
        std::cerr << "Warning: the selected encoder does not support lossless encoding. Encoding in lossy mode.\n";
      }
    }

    heif_encoder_set_lossy_quality(encoder, quality);
    heif_encoder_set_logging_level(encoder, logging_level);

    set_params(encoder, raw_params);
    struct heif_encoding_options* options = heif_encoding_options_alloc();
    options->save_alpha_channel = (uint8_t) master_alpha;
    options->save_two_colr_boxes_when_ICC_and_nclx_available = (uint8_t)two_colr_boxes;
    options->output_nclx_profile = &nclx;

    if (crop_to_even_size) {
      if (heif_image_get_primary_width(image.get()) == 1 ||
          heif_image_get_primary_height(image.get()) == 1) {
        std::cerr << "Image only has a size of 1 pixel width or height. Cannot crop to even size.\n";
        return 1;
      }

      std::cerr << "Warning: option --even-size/-E is deprecated as it is not needed anymore.\n";

      int right = heif_image_get_primary_width(image.get()) % 2;
      int bottom = heif_image_get_primary_height(image.get()) % 2;

      error = heif_image_crop(image.get(), 0, right, 0, bottom);
      if (error.code != 0) {
        heif_encoding_options_free(options);
        std::cerr << "Could not crop image: " << error.message << "\n";
        return 1;
      }
    }

    if (premultiplied_alpha) {
      heif_image_set_premultiplied_alpha(image.get(), premultiplied_alpha);
    }


    struct heif_image_handle* handle;
    error = heif_context_encode_image(context.get(),
                                      image.get(),
                                      encoder,
                                      options,
                                      &handle);
    if (error.code != 0) {
      heif_encoding_options_free(options);
      std::cerr << "Could not encode HEIF/AVIF file: " << error.message << "\n";
      return 1;
    }

    if (thumbnail_bbox_size > 0) {
      // encode thumbnail

      struct heif_image_handle* thumbnail_handle;

      options->save_alpha_channel = master_alpha && thumb_alpha;

      error = heif_context_encode_thumbnail(context.get(),
                                            image.get(),
                                            handle,
                                            encoder,
                                            options,
                                            thumbnail_bbox_size,
                                            &thumbnail_handle);
      if (error.code) {
        heif_encoding_options_free(options);
        std::cerr << "Could not generate thumbnail: " << error.message << "\n";
        return 5;
      }

      if (thumbnail_handle) {
        heif_image_handle_release(thumbnail_handle);
      }
    }

    heif_image_handle_release(handle);
    heif_encoding_options_free(options);
  }

  heif_encoder_release(encoder);

  error = heif_context_write_to_file(context.get(), output_filename.c_str());
  if (error.code) {
    std::cerr << error.message << "\n";
    return 5;
  }

  return 0;
}
