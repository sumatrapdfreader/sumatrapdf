/*
  libheif example application "heif-info".

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
#include <vector>

#if defined(HAVE_UNISTD_H)

#include <unistd.h>

#else
#define STDOUT_FILENO 1
#endif

#include <libheif/heif.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <getopt.h>
#include <assert.h>


/*
  image: 20005 (1920x1080), primary
    thumbnail: 20010 (320x240)
    alpha channel: 20012 (1920x1080)
    metadata: Exif

  image: 1920x1080 (20005), primary
    thumbnail: 320x240 (20010)
    alpha channel: 1920x1080 (20012)

info *file
info -w 20012 -o out.265 *file
info -d // dump
 */

static struct option long_options[] = {
    //{"write-raw", required_argument, 0, 'w' },
    //{"output",    required_argument, 0, 'o' },
    {(char* const) "dump-boxes", no_argument, 0, 'd'},
    {(char* const) "help",       no_argument, 0, 'h'},
    {0, 0,                                    0, 0}
};

const char* fourcc_to_string(uint32_t fourcc)
{
  static char fcc[5];
  fcc[0] = (char) ((fourcc >> 24) & 0xFF);
  fcc[1] = (char) ((fourcc >> 16) & 0xFF);
  fcc[2] = (char) ((fourcc >> 8) & 0xFF);
  fcc[3] = (char) ((fourcc >> 0) & 0xFF);
  fcc[4] = 0;
  return fcc;
}

void show_help(const char* argv0)
{
  fprintf(stderr, " heif-info  libheif version: %s\n", heif_get_version());
  fprintf(stderr, "------------------------------------\n");
  fprintf(stderr, "usage: heif-info [options] image.heic\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "options:\n");
  //fprintf(stderr,"  -w, --write-raw ID   write raw compressed data of image 'ID'\n");
  //fprintf(stderr,"  -o, --output NAME    output file name for image selected by -w\n");
  fprintf(stderr, "  -d, --dump-boxes     show a low-level dump of all MP4 file boxes\n");
  fprintf(stderr, "  -h, --help           show help\n");
}

int main(int argc, char** argv)
{
  bool dump_boxes = false;

  bool write_raw_image = false;
  heif_item_id raw_image_id;
  std::string output_filename = "output.265";

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "dh", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'd':
        dump_boxes = true;
        break;
      case 'h':
        show_help(argv[0]);
        return 0;
      case 'w':
        write_raw_image = true;
        raw_image_id = atoi(optarg);
        break;
      case 'o':
        output_filename = optarg;
        break;
    }
  }

  if (optind != argc - 1) {
    show_help(argv[0]);
    return 0;
  }


  (void) raw_image_id;
  (void) write_raw_image;

  const char* input_filename = argv[optind];

  // ==============================================================================
  //   show MIME type

  {
    const static int bufSize = 50;

    uint8_t buf[bufSize];
    FILE* fh = fopen(input_filename, "rb");
    if (fh) {
      std::cout << "MIME type: ";
      int n = (int) fread(buf, 1, bufSize, fh);
      const char* mime_type = heif_get_file_mime_type(buf, n);
      if (*mime_type == 0) {
        std::cout << "unknown\n";
      }
      else {
        std::cout << mime_type << "\n";
      }

      fclose(fh);

      char fourcc[5];
      fourcc[4]=0;
      heif_brand_to_fourcc( heif_read_main_brand(buf,bufSize), fourcc );
      std::cout << "main brand: " << fourcc << "\n";

      heif_brand2* brands=nullptr;
      int nBrands=0;
      struct heif_error err=heif_list_compatible_brands(buf, n, &brands, &nBrands);
      if (err.code) {
	std::cerr << "error reading brands: " << err.message << "\n";
      }
      else {
	std::cout << "compatible brands: ";
	for (int i=0;i<nBrands;i++) {
	  heif_brand_to_fourcc(brands[i], fourcc);
	  if (i>0) {
	    std::cout << ", ";
	  }
	  std::cout << fourcc;
	}
	
	std::cout << "\n";

	heif_free_list_of_compatible_brands(brands);
      }
    }

    std::cout << "\n";
  }

  // ==============================================================================

  std::shared_ptr<heif_context> ctx(heif_context_alloc(),
                                    [](heif_context* c) { heif_context_free(c); });
  if (!ctx) {
    fprintf(stderr, "Could not create context object\n");
    return 1;
  }

  struct heif_error err;
  err = heif_context_read_from_file(ctx.get(), input_filename, nullptr);

  if (dump_boxes) {
    heif_context_debug_dump_boxes_to_file(ctx.get(), STDOUT_FILENO); // dump to stdout
    return 0;
  }

  if (err.code != 0) {
    std::cerr << "Could not read HEIF/AVIF file: " << err.message << "\n";
    return 1;
  }


  // ==============================================================================


  int numImages = heif_context_get_number_of_top_level_images(ctx.get());
  std::vector<heif_item_id> IDs(numImages);
  heif_context_get_list_of_top_level_image_IDs(ctx.get(), IDs.data(), numImages);

  for (int i = 0; i < numImages; i++) {
    struct heif_image_handle* handle;
    struct heif_error err = heif_context_get_image_handle(ctx.get(), IDs[i], &handle);
    if (err.code) {
      std::cerr << err.message << "\n";
      return 10;
    }

    int width = heif_image_handle_get_width(handle);
    int height = heif_image_handle_get_height(handle);

    int primary = heif_image_handle_is_primary_image(handle);

    printf("image: %dx%d (id=%d)%s\n", width, height, IDs[i], primary ? ", primary" : "");


    // --- thumbnails

    int nThumbnails = heif_image_handle_get_number_of_thumbnails(handle);
    std::vector<heif_item_id> thumbnailIDs(nThumbnails);

    nThumbnails = heif_image_handle_get_list_of_thumbnail_IDs(handle, thumbnailIDs.data(), nThumbnails);

    for (int thumbnailIdx = 0; thumbnailIdx < nThumbnails; thumbnailIdx++) {
      heif_image_handle* thumbnail_handle;
      err = heif_image_handle_get_thumbnail(handle, thumbnailIDs[thumbnailIdx], &thumbnail_handle);
      if (err.code) {
        std::cerr << err.message << "\n";
        return 10;
      }

      int th_width = heif_image_handle_get_width(thumbnail_handle);
      int th_height = heif_image_handle_get_height(thumbnail_handle);

      printf("  thumbnail: %dx%d\n", th_width, th_height);

      heif_image_handle_release(thumbnail_handle);
    }


    // --- color profile

    uint32_t profileType = heif_image_handle_get_color_profile_type(handle);
    printf("  color profile: %s\n", profileType ? fourcc_to_string(profileType) : "no");


    // --- depth information

    bool has_depth = heif_image_handle_has_depth_image(handle);
    bool has_alpha = heif_image_handle_has_alpha_channel(handle);
    bool premultiplied_alpha = false;

    if (has_alpha) {
      premultiplied_alpha = heif_image_handle_is_premultiplied_alpha(handle);
    }

    printf("  alpha channel: %s %s\n", has_alpha ? "yes" : "no",
           premultiplied_alpha ? "(premultiplied)" : "");
    printf("  depth channel: %s\n", has_depth ? "yes" : "no");

    heif_item_id depth_id;
    int nDepthImages = heif_image_handle_get_list_of_depth_image_IDs(handle, &depth_id, 1);
    if (has_depth) { assert(nDepthImages == 1); }
    else { assert(nDepthImages == 0); }
    (void) nDepthImages;

    if (has_depth) {
      struct heif_image_handle* depth_handle;
      err = heif_image_handle_get_depth_image_handle(handle, depth_id, &depth_handle);
      if (err.code) {
        fprintf(stderr, "cannot get depth image: %s\n", err.message);
        return 1;
      }

      printf(" (%dx%d)\n",
             heif_image_handle_get_width(depth_handle),
             heif_image_handle_get_height(depth_handle));

      const struct heif_depth_representation_info* depth_info;
      if (heif_image_handle_get_depth_image_representation_info(handle, depth_id, &depth_info)) {

        printf("    z-near: ");
        if (depth_info->has_z_near) printf("%f\n", depth_info->z_near); else printf("undefined\n");
        printf("    z-far:  ");
        if (depth_info->has_z_far) printf("%f\n", depth_info->z_far); else printf("undefined\n");
        printf("    d-min:  ");
        if (depth_info->has_d_min) printf("%f\n", depth_info->d_min); else printf("undefined\n");
        printf("    d-max:  ");
        if (depth_info->has_d_max) printf("%f\n", depth_info->d_max); else printf("undefined\n");

        printf("    representation: ");
        switch (depth_info->depth_representation_type) {
          case heif_depth_representation_type_uniform_inverse_Z:
            printf("inverse Z\n");
            break;
          case heif_depth_representation_type_uniform_disparity:
            printf("uniform disparity\n");
            break;
          case heif_depth_representation_type_uniform_Z:
            printf("uniform Z\n");
            break;
          case heif_depth_representation_type_nonuniform_disparity:
            printf("non-uniform disparity\n");
            break;
          default:
            printf("unknown\n");
        }

        if (depth_info->has_d_min || depth_info->has_d_max) {
          printf("    disparity_reference_view: %d\n", depth_info->disparity_reference_view);
        }

        heif_depth_representation_info_free(depth_info);
      }

      heif_image_handle_release(depth_handle);
    }

    heif_image_handle_release(handle);
  }

#if 0
  std::cout << "num images: " << heif_context_get_number_of_top_level_images(ctx.get()) << "\n";

  struct heif_image_handle* handle;
  err = heif_context_get_primary_image_handle(ctx.get(), &handle);
  if (err.code != 0) {
    std::cerr << "Could not get primage image handle: " << err.message << "\n";
    return 1;
  }

  struct heif_image* image;
  err = heif_decode_image(handle, &image, heif_colorspace_undefined, heif_chroma_undefined, NULL);
  if (err.code != 0) {
    heif_image_handle_release(handle);
    std::cerr << "Could not decode primage image: " << err.message << "\n";
    return 1;
  }

  heif_image_release(image);
  heif_image_handle_release(handle);
#endif

  return 0;
}
