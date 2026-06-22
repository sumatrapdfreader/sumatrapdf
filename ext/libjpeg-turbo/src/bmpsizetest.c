/*
 * bmpsizetest.c
 *
 * Copyright (C) 2026, D. R. Commander.
 * For conditions of distribution and use, see the accompanying README.ijg
 * file.
 */

/* This program verifies that the low-level image I/O modules used by cjpeg,
   djpeg, tj3LoadImage*(), and tj3SaveImage*() limit image dimensions
   appropriately and do not overflow 32-bit integers. */

#define JPEG_INTERNALS
#include "cdjpeg.h"
#include <setjmp.h>

#define JMESSAGE(code, string)  string,

static const char * const cdjpeg_message_table[] = {
#include "cderror.h"
  NULL
};

static char error_str[JMSG_LENGTH_MAX] = { 0 };

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

static void my_error_exit(j_common_ptr cinfo)
{
  struct my_error_mgr *myerr = (struct my_error_mgr *)cinfo->err;

  (*cinfo->err->output_message) (cinfo);
  longjmp(myerr->setjmp_buffer, 1);
}

static void my_output_message(j_common_ptr cinfo)
{
  (*cinfo->err->format_message) (cinfo, error_str);
}


#define PAD(v, p)  (((v) + (p) - 1) & (~((p) - 1)))

#define THROW(m, e) { \
  SNPRINTF(error_str, JMSG_LENGTH_MAX, "%s: %s\n", m, e); \
  retval = -1; \
  goto bailout; \
}

#define PUT16(array, index, value) { \
  array[index] = (unsigned int)(value) & 0xFF; \
  array[index + 1] = (unsigned int)(value) >> 8; \
}

#define PUT32(array, index, value) { \
  array[index] = (unsigned int)(value) & 0xFF; \
  array[index + 1] = (unsigned int)(value) >> 8; \
  array[index + 2] = (unsigned int)(value) >> 16; \
  array[index + 3] = (unsigned int)(value) >> 24; \
}


static int test_read_BMP(int width, int height, JDIMENSION max_pixels,
                         boolean use_inversion_array)
{
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  cjpeg_source_ptr src_mgr;
  unsigned char bh[54];
  unsigned int pitch;
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_compress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);

  bh[0] = 'B';
  bh[1] = 'M';
  pitch = PAD((unsigned int)width * 3, 4);
  PUT32(bh, 2, 54 + pitch * height);       /* bfSize */
  bh[6] = bh[7] = 0;                       /* bfReserved1 */
  bh[8] = bh[9] = 0;                       /* bfReserved2 */
  PUT32(bh, 10, 54);                       /* bfOffBits */
  PUT32(bh, 14, 40);                       /* biSize */
  PUT32(bh, 18, width);                    /* biWidth */
  PUT32(bh, 22, height);                   /* biHeight */
  PUT16(bh, 26, 1);                        /* biPlanes */
  PUT16(bh, 28, 24);                       /* biBitCount */
  bh[30] = bh[31] = bh[32] = bh[33] = 0;   /* biCompression */
  bh[34] = bh[35] = bh[36] = bh[37] = 0;   /* biSizeImage */
  bh[38] = bh[39] = bh[40] = bh[41] = 0;   /* biXPelsPerMeter */
  bh[42] = bh[43] = bh[44] = bh[45] = 0;   /* biYPelsPerMeter */
  bh[46] = bh[47] = bh[48] = bh[49] = 0;   /* biClrUsed */
  bh[50] = bh[51] = bh[52] = bh[53] = 0;   /* biClrImportant */

  src_mgr = jinit_read_bmp(&cinfo, use_inversion_array);
  if (!src_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&bh, 54, "r")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  src_mgr->input_file = file;
  src_mgr->max_pixels = max_pixels;
  (*src_mgr->start_input) (&cinfo, src_mgr);

bailout:
  jpeg_destroy_compress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_write_BMP(int width, int height, boolean use_inversion_array)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  djpeg_dest_ptr dest_mgr;
  unsigned char bh[54];
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_decompress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.out_color_space = JCS_RGB;
  cinfo.quantize_colors = FALSE;
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.scale_num = cinfo.scale_denom = 1;
  cinfo.global_state = DSTATE_READY;

  dest_mgr = jinit_write_bmp(&cinfo, FALSE, use_inversion_array);
  if (!dest_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&bh, 54, "w")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  dest_mgr->output_file = file;
  (*dest_mgr->start_output) (&cinfo, dest_mgr);

bailout:
  jpeg_destroy_decompress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_read_GIF(int width, int height, JDIMENSION max_pixels,
                         boolean interlaced)
{
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  cjpeg_source_ptr src_mgr;
  unsigned char gh[30];
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_compress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);

  gh[0] = 'G';
  gh[1] = 'I';
  gh[2] = 'F';
  gh[3] = '8';
  gh[4] = '7';
  gh[5] = 'a';
  PUT16(gh, 6, (unsigned int)width / 2);   /* Canvas width */
  PUT16(gh, 8, (unsigned int)height / 2);  /* Canvas height */
  gh[10] = 0x80;                        /* Bit 8 = Global color table flag
                                           Bits 7-5 = Color resolution
                                                      [0 = 2 ^ (0 + 1) colors]
                                           Bits 3-1 = Global color table size
                                                      [0 = 2 ^ (0 + 1) colors]
                                         */
  gh[11] = 0;                           /* Background color index */
  gh[12] = 0;                           /* Pixel aspect ratio */
  gh[13] = gh[14] = gh[15] = 0;         /* Global color table entry 0 */
  gh[16] = gh[17] = gh[18] = 255;       /* Global color table entry 1 */
  gh[19] = ',';                         /* Image separator */
  PUT16(gh, 24, (unsigned int)width);   /* Image width */
  PUT16(gh, 26, (unsigned int)height);  /* Image height */
  gh[28] = interlaced ? 0x40 : 0;       /* Bit 7 = Interlace flag */
  gh[29] = 2;                           /* LZW minimum code size */

  src_mgr = jinit_read_gif(&cinfo);
  if (!src_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&gh, 30, "r")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval += -1;
    goto next;
  }

  src_mgr->input_file = file;
  src_mgr->max_pixels = max_pixels;
  (*src_mgr->start_input) (&cinfo, src_mgr);

next:
  if (setjmp(myerr.setjmp_buffer)) {
    retval += -1;
    goto bailout;
  }

  PUT16(gh, 6, (unsigned int)width);
  PUT16(gh, 8, (unsigned int)height);
  PUT16(gh, 24, (unsigned int)width / 2);
  PUT16(gh, 26, (unsigned int)height / 2);
  fseek(file, 0, SEEK_SET);
  (*src_mgr->start_input) (&cinfo, src_mgr);

bailout:
  jpeg_destroy_compress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_write_GIF(int width, int height, int unused)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  djpeg_dest_ptr dest_mgr;
  unsigned char gh[30];
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_decompress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.out_color_space = JCS_RGB;
  cinfo.quantize_colors = FALSE;
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.scale_num = cinfo.scale_denom = 1;
  cinfo.global_state = DSTATE_READY;

  dest_mgr = jinit_write_gif(&cinfo, FALSE);
  if (!dest_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&gh, 30, "w")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  dest_mgr->output_file = file;
  (*dest_mgr->start_output) (&cinfo, dest_mgr);

bailout:
  jpeg_destroy_decompress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_read_PPM(int width, int height, JDIMENSION max_pixels,
                         int data_precision)
{
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  cjpeg_source_ptr src_mgr;
  unsigned char ph[80] = { 0 };
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_compress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);
  cinfo.data_precision = data_precision;

  SNPRINTF((char *)ph, 80, "P6\n%d %d\n%d\n", width, height,
           (1 << data_precision) - 1);

  if (data_precision <= 8)
    src_mgr = jinit_read_ppm(&cinfo);
  else if (data_precision <= 12)
    src_mgr = j12init_read_ppm(&cinfo);
#ifdef C_LOSSLESS_SUPPORTED
  else
    src_mgr = j16init_read_ppm(&cinfo);
#endif
  if (!src_mgr)
    return -1;
  if ((file = fmemopen((void *)&ph, 80, "r")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  src_mgr->input_file = file;
  src_mgr->max_pixels = max_pixels;
  (*src_mgr->start_input) (&cinfo, src_mgr);

bailout:
  jpeg_destroy_compress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_write_PPM(int width, int height, int data_precision)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  djpeg_dest_ptr dest_mgr;
  unsigned char ph[80];
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_decompress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.out_color_space = JCS_RGB;
  cinfo.quantize_colors = FALSE;
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.scale_num = cinfo.scale_denom = 1;
  cinfo.data_precision = data_precision;
  cinfo.global_state = DSTATE_READY;

  if (data_precision <= 8)
    dest_mgr = jinit_write_ppm(&cinfo);
  else if (data_precision <= 12)
    dest_mgr = j12init_write_ppm(&cinfo);
#ifdef D_LOSSLESS_SUPPORTED
  else
    dest_mgr = j16init_write_ppm(&cinfo);
#endif
  if (!dest_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&ph, 80, "w")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  dest_mgr->output_file = file;
  (*dest_mgr->start_output) (&cinfo, dest_mgr);

bailout:
  jpeg_destroy_decompress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_read_TGA(int width, int height, JDIMENSION max_pixels,
                         boolean top_down)
{
  struct jpeg_compress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  cjpeg_source_ptr src_mgr = NULL;
  unsigned char th[18];
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_compress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.in_color_space = JCS_RGB;
  jpeg_set_defaults(&cinfo);

  th[0] = 0;                            /* ID length */
  th[1] = 0;                            /* Color map type */
  th[2] = 2;                            /* Image type
                                           (2 = uncompressed true color) */
  th[3] = th[4] = 0;                    /* Color map first entry index */
  th[5] = th[6] = 0;                    /* Color map length */
  th[7] = 0;                            /* Color map entry size (bits) */
  th[8] = th[9] = 0;                    /* X origin */
  th[10] = th[11] = 0;                  /* Y origin */
  PUT16(th, 12, width);                 /* Image width */
  PUT16(th, 14, height);                /* Image height */
  th[16] = 24;                          /* Pixel depth (bits) */
  th[17] = top_down ? 0x20 : 0;         /* Image descriptor
                                           (bit 5 = top-down) */

  src_mgr = jinit_read_targa(&cinfo);
  if (!src_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&th, 18, "r")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  src_mgr->input_file = file;
  src_mgr->max_pixels = max_pixels;
  (*src_mgr->start_input) (&cinfo, src_mgr);

bailout:
  jpeg_destroy_compress(&cinfo);
  if (file) fclose(file);
  return retval;
}


static int test_write_TGA(int width, int height, int unused)
{
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr myerr;
  struct jpeg_error_mgr *jerr = &myerr.pub;
  djpeg_dest_ptr dest_mgr;
  unsigned char th[18];
  int retval = 0;
  FILE *file = NULL;

  cinfo.err = jpeg_std_error(jerr);
  jerr->error_exit = my_error_exit;
  jerr->output_message = my_output_message;

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  jpeg_create_decompress(&cinfo);
  jerr->addon_message_table = cdjpeg_message_table;
  jerr->first_addon_message = JMSG_FIRSTADDONCODE;
  jerr->last_addon_message = JMSG_LASTADDONCODE;

  cinfo.out_color_space = JCS_RGB;
  cinfo.quantize_colors = FALSE;
  cinfo.image_width = width;
  cinfo.image_height = height;
  cinfo.scale_num = cinfo.scale_denom = 1;
  cinfo.global_state = DSTATE_READY;

  dest_mgr = jinit_write_targa(&cinfo);
  if (!dest_mgr) {
    retval = -1;
    goto bailout;
  }
  if ((file = fmemopen((void *)&th, 18, "w")) == NULL)
    THROW("Can't open file", strerror(errno));

  if (setjmp(myerr.setjmp_buffer)) {
    retval = -1;
    goto bailout;
  }

  dest_mgr->output_file = file;
  (*dest_mgr->start_output) (&cinfo, dest_mgr);

bailout:
  jpeg_destroy_decompress(&cinfo);
  if (file) fclose(file);
  return retval;
}


#define TESTREAD(EXT, w, h, max_pixels, opt_desc, opt, retval, \
  expected_error_str) { \
  error_str[0] = 0; \
  if (test_read_##EXT(w, h, max_pixels, opt) != retval) { \
    fprintf(stderr, "%ld x %ld " opt_desc " " #EXT " error not caught\n", \
            w, h); \
    return -1; \
  } \
  if (strncmp(error_str, expected_error_str, JMSG_LENGTH_MAX)) { \
    fprintf(stderr, "%ld x %ld " opt_desc " " #EXT \
            " error string incorrect\n" \
            "  Expected: %s\n  Actual: %s\n", w, h, expected_error_str, \
            error_str); \
    return -1; \
  } \
}

#define TESTWRITE(EXT, w, h, opt_desc, opt, retval, expected_error_str) { \
  error_str[0] = 0; \
  if (test_write_##EXT(w, h, opt) != retval) { \
    fprintf(stderr, "%ld x %ld " opt_desc " " #EXT " error not caught\n", \
            w, h); \
    return -1; \
  } \
  if (strncmp(error_str, expected_error_str, JMSG_LENGTH_MAX)) { \
    fprintf(stderr, "%ld x %ld " opt_desc " " #EXT \
            " error string incorrect\n" \
            "  Expected: %s\n  Actual: %s\n", w, h, expected_error_str, \
            error_str); \
    return -1; \
  } \
}


int main(void)
{
  fprintf(stderr, "Testing BMP reader ...\n");

  TESTREAD(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "bottom-up",
           FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(BMP, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "bottom-up",
           FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "bottom-up", FALSE,
           0, "");
  TESTREAD(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "top-down",
           TRUE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(BMP, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "top-down",
           TRUE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "top-down", TRUE, 0,
           "");

  TESTREAD(BMP, JPEG_MAX_DIMENSION, 15268L, 1000000000, "bottom-up", FALSE, -1,
           "Maximum supported image dimension is 1000000000 pixels");
  TESTREAD(BMP, JPEG_MAX_DIMENSION, 15267L, 1000000000, "bottom-up", FALSE, 0,
           "");
  TESTREAD(BMP, 1527L, JPEG_MAX_DIMENSION, 100000000, "bottom-up", FALSE, -1,
           "Maximum supported image dimension is 100000000 pixels");
  TESTREAD(BMP, 1526L, JPEG_MAX_DIMENSION, 100000000, "bottom-up", FALSE, 0,
           "");
  TESTREAD(BMP, 1000L, 1001L, 1000000, "top-down", TRUE, -1,
           "Maximum supported image dimension is 1000000 pixels");
  TESTREAD(BMP, 1000L, 1000L, 1000000, "top-down", TRUE, 0, "");

  fprintf(stderr, "\n");

  fprintf(stderr, "Testing BMP writer ...\n");

  TESTWRITE(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "bottom-up",
            FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(BMP, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "bottom-up",
            FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, "bottom-up", FALSE, 0,
            "");
  TESTWRITE(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "top-down", TRUE,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(BMP, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "top-down", TRUE,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(BMP, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, "top-down", TRUE, 0,
            "");

  fprintf(stderr, "\n");

  /***************************************************************************/

  fprintf(stderr, "Testing GIF reader ...\n");

  TESTREAD(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0,
           "non-interlaced", FALSE, -2,
           "Maximum supported image dimension is 65500 pixels");
  TESTREAD(GIF, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0,
           "non-interlaced", FALSE, -2,
           "Maximum supported image dimension is 65500 pixels");
  TESTREAD(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "non-interlaced",
           FALSE, 0, "");
  TESTREAD(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "interlaced",
           TRUE, -2, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(GIF, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "interlaced",
           TRUE, -2, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "interlaced", TRUE,
           0, "");

  TESTREAD(GIF, JPEG_MAX_DIMENSION, 15268L, 1000000000, "non-interlaced",
           FALSE, -2,
           "Maximum supported image dimension is 1000000000 pixels");
  TESTREAD(GIF, JPEG_MAX_DIMENSION, 15267L, 1000000000, "non-interlaced",
           FALSE, 0, "");
  TESTREAD(GIF, 1527L, JPEG_MAX_DIMENSION, 100000000, "non-interlaced", FALSE,
           -2, "Maximum supported image dimension is 100000000 pixels");
  TESTREAD(GIF, 1526L, JPEG_MAX_DIMENSION, 100000000, "non-interlaced", FALSE,
           0, "");
  TESTREAD(GIF, 1000L, 1001L, 1000000, "interlaced", TRUE, -2,
           "Maximum supported image dimension is 1000000 pixels");
  TESTREAD(GIF, 1000L, 1000L, 1000000, "interlaced", TRUE, 0, "");

  fprintf(stderr, "\n");

  fprintf(stderr, "Testing GIF writer ...\n");

  TESTWRITE(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "non-interlaced",
            FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(GIF, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "non-interlaced",
            FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, "non-interlaced",
            FALSE, 0, "");
  TESTWRITE(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "interlaced",
            TRUE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(GIF, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "interlaced",
            TRUE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(GIF, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, "interlaced", TRUE, 0,
            "");

  fprintf(stderr, "\n");

  /***************************************************************************/

  fprintf(stderr, "Testing PPM reader ...\n");

  TESTREAD(PPM, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "8-bit", 8, -1,
           "Maximum supported image dimension is 65500 pixels");
  TESTREAD(PPM, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "12-bit", 12,
           -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(PPM, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "16-bit", 16,
           -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(PPM, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "16-bit", 16, 0,
           "");
  TESTREAD(PPM, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "17-bit", 17,
           -1, "Unsupported JPEG data precision 17");

  TESTREAD(PPM, JPEG_MAX_DIMENSION, 15268L, 1000000000, "8-bit", 8, -1,
           "Maximum supported image dimension is 1000000000 pixels");
  TESTREAD(PPM, JPEG_MAX_DIMENSION, 15267L, 1000000000, "8-bit", 8, 0, "");
  TESTREAD(PPM, 1527L, JPEG_MAX_DIMENSION, 100000000, "12-bit", 12, -1,
           "Maximum supported image dimension is 100000000 pixels");
  TESTREAD(PPM, 1526L, JPEG_MAX_DIMENSION, 100000000, "12-bit", 12, 0, "");
  TESTREAD(PPM, 1000L, 1001L, 1000000, "16-bit", 16, -1,
           "Maximum supported image dimension is 1000000 pixels");
  TESTREAD(PPM, 1000L, 1000L, 1000000, "16-bit", 16, 0, "");

  fprintf(stderr, "\n");

  fprintf(stderr, "Testing PPM writer ...\n");

  TESTWRITE(PPM, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "8-bit", 8,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(PPM, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "12-bit", 12,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(PPM, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "16-bit", 16,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(PPM, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, "16-bit", 16, 0, "");
  TESTWRITE(PPM, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "17-bit", 17,
            -1, "Unsupported JPEG data precision 17");

  fprintf(stderr, "\n");

  /***************************************************************************/

  fprintf(stderr, "Testing Targa reader ...\n");

  TESTREAD(TGA, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "bottom-up",
           FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(TGA, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "bottom-up",
           FALSE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(TGA, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "bottom-up", FALSE,
           0, "");
  TESTREAD(TGA, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, 0, "top-down",
           TRUE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(TGA, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, 0, "top-down",
           TRUE, -1, "Maximum supported image dimension is 65500 pixels");
  TESTREAD(TGA, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, 0, "top-down", TRUE,
           0, "");

  TESTREAD(TGA, JPEG_MAX_DIMENSION, 15268L, 1000000000, "bottom-up", FALSE, -1,
           "Maximum supported image dimension is 1000000000 pixels");
  TESTREAD(TGA, JPEG_MAX_DIMENSION, 15267L, 1000000000, "bottom-up", FALSE, 0,
           "");
  TESTREAD(TGA, 1527L, JPEG_MAX_DIMENSION, 100000000, "bottom-up", FALSE, -1,
           "Maximum supported image dimension is 100000000 pixels");
  TESTREAD(TGA, 1526L, JPEG_MAX_DIMENSION, 100000000, "bottom-up", FALSE, 0,
           "");
  TESTREAD(TGA, 1000L, 1001L, 1000000, "top-down", TRUE, -1,
           "Maximum supported image dimension is 1000000 pixels");
  TESTREAD(TGA, 1000L, 1000L, 1000000, "top-down", TRUE, 0, "");

  fprintf(stderr, "\n");

  fprintf(stderr, "Testing Targa writer ...\n");

  TESTWRITE(TGA, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION + 1, "top-down", 0,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(TGA, JPEG_MAX_DIMENSION + 1, JPEG_MAX_DIMENSION, "top-down", 0,
            -1, "Maximum supported image dimension is 65500 pixels");
  TESTWRITE(TGA, JPEG_MAX_DIMENSION, JPEG_MAX_DIMENSION, "top-down", 0, 0, "");

  fprintf(stderr, "\n");

  /***************************************************************************/

  fprintf(stderr, "GREAT SUCCESS!\n");
  return 0;
}
