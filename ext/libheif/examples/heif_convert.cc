/*
  libheif example application "convert".

  MIT License

  Copyright (c) 2017 struktur AG, Joachim Bauch <bauch@struktur.de>

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

#include <cstring>
#include <getopt.h>

#if defined(HAVE_UNISTD_H)

#include <unistd.h>

#endif

#include <fstream>
#include <iostream>
#include <sstream>
#include <cassert>
#include <algorithm>
#include <vector>
#include <cctype>

#include <libheif/heif.h>

#include "encoder.h"

#if HAVE_LIBJPEG

#include "encoder_jpeg.h"

#endif
#if HAVE_LIBPNG

#include "encoder_png.h"

#endif

#include "encoder_y4m.h"

#if defined(__MINGW32__)  || defined(__MINGW64__) || defined(_MSC_VER)
#include "getopt.h"
#endif

#define UNUSED(x) (void)x

static void show_help(const char* argv0)
{
  std::cerr << " heif-convert  libheif version: " << heif_get_version() << "\n"
            << "-------------------------------------------\n"
               "Usage: heif-convert [options]  <input-image> <output-image>\n"
               "\n"
               "The program determines the output file format from the output filename suffix.\n"
               "These suffices are recognized: jpg, jpeg, png, y4m."
               "\n"
               "Options:\n"
               "  -h, --help      show help\n"
               "  -q, --quality   quality (for JPEG output)\n"
               "      --with-aux  also write auxiliary images (e.g. depth images)\n"
               "      --no-colons replace ':' characters in auxiliary image filenames with '_'\n"
               "      --quiet     do not output status messages to console\n";
}


class ContextReleaser
{
public:
  ContextReleaser(struct heif_context* ctx) : ctx_(ctx)
  {}

  ~ContextReleaser()
  {
    heif_context_free(ctx_);
  }

private:
  struct heif_context* ctx_;
};


int option_quiet = 0;
int option_aux = 0;
int option_no_colons = 0;

static struct option long_options[] = {
    {(char* const) "quality",   required_argument, 0,                 'q'},
    {(char* const) "strict",    no_argument,       0,                 's'},
    {(char* const) "quiet",     no_argument,       &option_quiet,     1},
    {(char* const) "with-aux",  no_argument,       &option_aux,       1},
    {(char* const) "no-colons", no_argument,       &option_no_colons, 1},
    {(char* const) "help",      no_argument,       0,                 'h'}
};

int main(int argc, char** argv)
{
  int quality = -1;  // Use default quality.
  bool strict_decoding = false;

  UNUSED(quality);  // The quality will only be used by encoders that support it.
  //while ((opt = getopt(argc, argv, "q:s")) != -1) {
  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "q:s", long_options, &option_index);
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'q':
        quality = atoi(optarg);
        break;
      case 's':
        strict_decoding = true;
        break;
      case '?':
        std::cerr << "\n";
        // falltrough
      case 'h':
        show_help(argv[0]);
        return 0;
    }
  }

  if (optind + 2 > argc) {
    // Need input and output filenames as additional arguments.
    show_help(argv[0]);
    return 5;
  }

  std::string input_filename(argv[optind++]);
  std::string output_filename(argv[optind++]);

  std::unique_ptr<Encoder> encoder;

  size_t dot_pos = output_filename.rfind('.');
  if (dot_pos != std::string::npos) {
    std::string suffix_lowercase = output_filename.substr(dot_pos + 1);

    std::transform(suffix_lowercase.begin(), suffix_lowercase.end(),
                   suffix_lowercase.begin(), ::tolower);

    if (suffix_lowercase == "jpg" || suffix_lowercase == "jpeg") {
#if HAVE_LIBJPEG
      static const int kDefaultJpegQuality = 90;
      if (quality == -1) {
        quality = kDefaultJpegQuality;
      }
      encoder.reset(new JpegEncoder(quality));
#else
      fprintf(stderr, "JPEG support has not been compiled in.\n");
      return 1;
#endif  // HAVE_LIBJPEG
    }

    if (suffix_lowercase == "png") {
#if HAVE_LIBPNG
      encoder.reset(new PngEncoder());
#else
      fprintf(stderr, "PNG support has not been compiled in.\n");
      return 1;
#endif  // HAVE_LIBPNG
    }

    if (suffix_lowercase == "y4m") {
      encoder.reset(new Y4MEncoder());
    }
  }

  if (!encoder) {
    fprintf(stderr, "Unknown file type in %s\n", output_filename.c_str());
    return 1;
  }


  // --- check whether input is a supported HEIF file

  // TODO: when we are reading from named pipes, we probably should not consume any bytes
  // just for file-type checking.
  // TODO: check, whether reading from named pipes works at all.

  std::ifstream istr(input_filename.c_str(), std::ios_base::binary);
  uint8_t magic[12];
  istr.read((char*) magic, 12);
  enum heif_filetype_result filetype_check = heif_check_filetype(magic, 12);
  if (filetype_check == heif_filetype_no) {
    fprintf(stderr, "Input file is not an HEIF/AVIF file\n");
    return 1;
  }

  if (filetype_check == heif_filetype_yes_unsupported) {
    fprintf(stderr, "Input file is an unsupported HEIF/AVIF file type\n");
    return 1;
  }



  // --- read the HEIF file

  struct heif_context* ctx = heif_context_alloc();
  if (!ctx) {
    fprintf(stderr, "Could not create context object\n");
    return 1;
  }

  ContextReleaser cr(ctx);
  struct heif_error err;
  err = heif_context_read_from_file(ctx, input_filename.c_str(), nullptr);
  if (err.code != 0) {
    std::cerr << "Could not read HEIF/AVIF file: " << err.message << "\n";
    return 1;
  }

  int num_images = heif_context_get_number_of_top_level_images(ctx);
  if (num_images == 0) {
    fprintf(stderr, "File doesn't contain any images\n");
    return 1;
  }

  if (!option_quiet) {
    std::cout << "File contains " << num_images << " image" << (num_images>1 ? "s" : "") << "\n";
  }

  std::vector<heif_item_id> image_IDs(num_images);
  num_images = heif_context_get_list_of_top_level_image_IDs(ctx, image_IDs.data(), num_images);


  std::string filename;
  size_t image_index = 1;  // Image filenames are "1" based.

  for (int idx = 0; idx < num_images; ++idx) {

    if (num_images > 1) {
      std::ostringstream s;
      s << output_filename.substr(0, output_filename.find_last_of('.'));
      s << "-" << image_index;
      s << output_filename.substr(output_filename.find_last_of('.'));
      filename.assign(s.str());
    }
    else {
      filename.assign(output_filename);
    }

    struct heif_image_handle* handle;
    err = heif_context_get_image_handle(ctx, image_IDs[idx], &handle);
    if (err.code) {
      std::cerr << "Could not read HEIF/AVIF image " << idx << ": "
                << err.message << "\n";
      return 1;
    }

    int has_alpha = heif_image_handle_has_alpha_channel(handle);
    struct heif_decoding_options* decode_options = heif_decoding_options_alloc();
    encoder->UpdateDecodingOptions(handle, decode_options);

    decode_options->strict_decoding = strict_decoding;

    int bit_depth = heif_image_handle_get_luma_bits_per_pixel(handle);
    if (bit_depth < 0) {
      heif_decoding_options_free(decode_options);
      heif_image_handle_release(handle);
      std::cerr << "Input image has undefined bit-depth\n";
      return 1;
    }

    struct heif_image* image;
    err = heif_decode_image(handle,
                            &image,
                            encoder->colorspace(has_alpha),
                            encoder->chroma(has_alpha, bit_depth),
                            decode_options);
    heif_decoding_options_free(decode_options);
    if (err.code) {
      heif_image_handle_release(handle);
      std::cerr << "Could not decode image: " << idx << ": "
                << err.message << "\n";
      return 1;
    }

    // show decoding warnings

    for (int i = 0;; i++) {
      int n = heif_image_get_decoding_warnings(image, i, &err, 1);
      if (n == 0) {
        break;
      }

      std::cerr << "Warning: " << err.message << "\n";
    }

    if (image) {
      bool written = encoder->Encode(handle, image, filename);
      if (!written) {
        fprintf(stderr, "could not write image\n");
      }
      else {
        if (!option_quiet) {
          std::cout << "Written to " << filename << "\n";
        }
      }
      heif_image_release(image);


      if (option_aux) {
        int has_depth = heif_image_handle_has_depth_image(handle);
        if (has_depth) {
          heif_item_id depth_id;
          int nDepthImages = heif_image_handle_get_list_of_depth_image_IDs(handle, &depth_id, 1);
          assert(nDepthImages == 1);
          (void) nDepthImages;

          struct heif_image_handle* depth_handle;
          err = heif_image_handle_get_depth_image_handle(handle, depth_id, &depth_handle);
          if (err.code) {
            heif_image_handle_release(handle);
            std::cerr << "Could not read depth channel\n";
            return 1;
          }

          int depth_bit_depth = heif_image_handle_get_luma_bits_per_pixel(depth_handle);

          struct heif_image* depth_image;
          err = heif_decode_image(depth_handle,
                                  &depth_image,
                                  encoder->colorspace(false),
                                  encoder->chroma(false, depth_bit_depth),
                                  nullptr);
          if (err.code) {
            heif_image_handle_release(depth_handle);
            heif_image_handle_release(handle);
            std::cerr << "Could not decode depth image: " << err.message << "\n";
            return 1;
          }

          std::ostringstream s;
          s << output_filename.substr(0, output_filename.find('.'));
          s << "-depth";
          s << output_filename.substr(output_filename.find('.'));

          written = encoder->Encode(depth_handle, depth_image, s.str());
          if (!written) {
            fprintf(stderr, "could not write depth image\n");
          }
          else {
            if (!option_quiet) {
              std::cout << "Depth image written to " << s.str() << "\n";
            }
          }

          heif_image_release(depth_image);
          heif_image_handle_release(depth_handle);
        }
      }


      // --- aux images

      if (option_aux) {
        int nAuxImages = heif_image_handle_get_number_of_auxiliary_images(handle, LIBHEIF_AUX_IMAGE_FILTER_OMIT_ALPHA | LIBHEIF_AUX_IMAGE_FILTER_OMIT_DEPTH);
        if (nAuxImages > 0) {

          std::vector<heif_item_id> auxIDs(nAuxImages);
          heif_image_handle_get_list_of_auxiliary_image_IDs(handle,
                                                            LIBHEIF_AUX_IMAGE_FILTER_OMIT_ALPHA | LIBHEIF_AUX_IMAGE_FILTER_OMIT_DEPTH,
                                                            auxIDs.data(), nAuxImages);

          for (heif_item_id auxId: auxIDs) {

            struct heif_image_handle* aux_handle;
            err = heif_image_handle_get_auxiliary_image_handle(handle, auxId, &aux_handle);
            if (err.code) {
              heif_image_handle_release(handle);
              std::cerr << "Could not read auxiliary image\n";
              return 1;
            }

            int aux_bit_depth = heif_image_handle_get_luma_bits_per_pixel(aux_handle);

            struct heif_image* aux_image;
            err = heif_decode_image(aux_handle,
                                    &aux_image,
                                    encoder->colorspace(false),
                                    encoder->chroma(false, aux_bit_depth),
                                    nullptr);
            if (err.code) {
              heif_image_handle_release(aux_handle);
              heif_image_handle_release(handle);
              std::cerr << "Could not decode auxiliary image: " << err.message << "\n";
              return 1;
            }

            const char* auxTypeC = nullptr;
            err = heif_image_handle_get_auxiliary_type(aux_handle, &auxTypeC);
            if (err.code) {
              heif_image_handle_release(aux_handle);
              heif_image_handle_release(handle);
              std::cerr << "Could not get type of auxiliary image: " << err.message << "\n";
              return 1;
            }

            std::string auxType = std::string(auxTypeC);

            heif_image_handle_free_auxiliary_types(aux_handle, &auxTypeC);

            std::ostringstream s;
            s << output_filename.substr(0, output_filename.find('.'));
            s << "-" + auxType;
            s << output_filename.substr(output_filename.find('.'));

            std::string auxFilename = s.str();

            if (option_no_colons) {
              std::replace(auxFilename.begin(), auxFilename.end(), ':', '_');
            }

            written = encoder->Encode(aux_handle, aux_image, auxFilename);
            if (!written) {
              fprintf(stderr, "could not write auxiliary image\n");
            }
            else {
              if (!option_quiet) {
                std::cout << "Auxiliary image written to " << auxFilename << "\n";
              }
            }

            heif_image_release(aux_image);
            heif_image_handle_release(aux_handle);
          }
        }
      }

      heif_image_handle_release(handle);
    }

    image_index++;
  }

  return 0;
}
