/*
  libheif thumbnailer for Gnome desktop.

  MIT License

  Copyright (c) 2018 struktur AG, Dirk Farin <farin@struktur.de>

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
#  include "config.h"
#endif


#if defined(HAVE_UNISTD_H)
#  include <unistd.h>
#endif

#include <string>
#include <iostream>
#include <cassert>
#include <libheif/heif.h>
#include "encoder.h"

#if HAVE_LIBPNG
#  include "encoder_png.h"
#endif

#if defined(_MSC_VER)
#include "getopt.h"
#endif


static int usage(const char* command)
{
  fprintf(stderr, "usage: %s [-s size] [-p] <filename> <output>\n", command);
  fprintf(stderr, " -p   Render thumbnail from primary image, even if thumbnail is stored in image.\n");
  return 1;
}


int main(int argc, char** argv)
{
  int opt;
  int size = 512; // default thumbnail size
  bool thumbnail_from_primary_image_only = false;

  while ((opt = getopt(argc, argv, "s:hp")) != -1) {
    switch (opt) {
      case 's':
        size = atoi(optarg);
        break;
      case 'p':
        thumbnail_from_primary_image_only = true;
        break;
      case 'h':
      default:
        return usage(argv[0]);
    }
  }

  if (optind + 2 > argc) {
    // Need input and output filenames as additional arguments.
    return usage(argv[0]);
  }

  std::string input_filename(argv[optind++]);
  std::string output_filename(argv[optind++]);


  // --- read heif file

  std::shared_ptr<heif_context> context(heif_context_alloc(),
                                        [](heif_context* c) { heif_context_free(c); });

  struct heif_error err;
  err = heif_context_read_from_file(context.get(), input_filename.c_str(), nullptr);
  if (err.code != 0) {
    std::cerr << "Could not read HEIF file: " << err.message << "\n";
    return 1;
  }



  // --- get primary image

  struct heif_image_handle* image_handle = NULL;
  err = heif_context_get_primary_image_handle(context.get(), &image_handle);
  if (err.code) {
    std::cerr << "Could not read HEIF image : " << err.message << "\n";
    return 1;
  }


  // --- if image has a thumbnail, use that instead

  if (!thumbnail_from_primary_image_only) {
    heif_item_id thumbnail_ID;
    int nThumbnails = heif_image_handle_get_list_of_thumbnail_IDs(image_handle, &thumbnail_ID, 1);
    if (nThumbnails > 0) {
      struct heif_image_handle* thumbnail_handle;
      err = heif_image_handle_get_thumbnail(image_handle, thumbnail_ID, &thumbnail_handle);
      if (err.code) {
        std::cerr << "Could not read HEIF image : " << err.message << "\n";
        return 1;
      }

      // replace image handle with thumbnail handle

      heif_image_handle_release(image_handle);
      image_handle = thumbnail_handle;
    }
  }



  // --- decode the image (or its thumbnail)

  std::unique_ptr<Encoder> encoder(new PngEncoder());

  struct heif_decoding_options* decode_options = heif_decoding_options_alloc();
  encoder->UpdateDecodingOptions(image_handle, decode_options);
  decode_options->convert_hdr_to_8bit = true;

  int bit_depth = 8;

  struct heif_image* image = NULL;
  err = heif_decode_image(image_handle,
                          &image,
                          encoder->colorspace(false),
                          encoder->chroma(false, bit_depth),
                          decode_options);
  if (err.code) {
    std::cerr << "Could not decode HEIF image : " << err.message << "\n";
    return 1;
  }

  assert(image);

  // --- compute output thumbnail size

  int input_width = heif_image_handle_get_width(image_handle);
  int input_height = heif_image_handle_get_height(image_handle);

  if (input_width > size || input_height > size) {
    int thumbnail_width;
    int thumbnail_height;

    if (input_width > input_height) {
      thumbnail_height = input_height * size / input_width;
      thumbnail_width = size;
    }
    else if (input_height > 0) {
      thumbnail_width = input_width * size / input_height;
      thumbnail_height = size;
    }
    else {
      thumbnail_width = thumbnail_height = 0;
    }


    // --- output thumbnail smaller than HEIF thumbnail -> scale down

    struct heif_image* scaled_image = NULL;
    err = heif_image_scale_image(image, &scaled_image,
                                 thumbnail_width, thumbnail_height,
                                 NULL);
    if (err.code) {
      std::cerr << "Could not scale image : " << err.message << "\n";
      return 1;
    }

    heif_image_release(image);
    image = scaled_image;
  }



  // --- write thumbnail image to PNG

  bool written = encoder->Encode(image_handle, image, output_filename.c_str());
  if (!written) {
    fprintf(stderr, "could not write image\n");
    return 1;
  }

  heif_image_release(image);

  heif_image_handle_release(image_handle);

  return 0;
}
