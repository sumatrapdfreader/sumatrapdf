/*
  libheif example application "heif-test".

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

#if defined(HAVE_UNISTD_H)

#include <unistd.h>

#else
#define STDOUT_FILENO 1
#endif

#include <libheif/heif_cxx.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <getopt.h>
#include <assert.h>


static struct option long_options[] = {
    //{"write-raw", required_argument, 0, 'w' },
    //{"output",    required_argument, 0, 'o' },
    {(char* const) "decode-img", required_argument, 0, 'd'},
    {(char* const) "metadata",   required_argument, 0, 'm'},
    {0, 0,                                          0, 0}
};

void show_help(const char* argv0)
{
  fprintf(stderr, " heif-test  libheif version: %s\n", heif_get_version());
  fprintf(stderr, "------------------------------------\n");
  fprintf(stderr, "usage: heif-test [options] image.heic\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "options:\n");
  fprintf(stderr, "  -d, --decode-img ID  decode image and output raw pixel data of all planes\n");
  fprintf(stderr, "  -m, --metadata ID    output metadata\n");
  fprintf(stderr, "  -h, --help           show help\n");
}


std::pair<heif_item_id, heif_item_id> parse_id_pair(const std::string& s)
{
  std::string::size_type p = s.find_first_of(':');
  if (p == std::string::npos) {
    fprintf(stderr, "id pair has to be in this format: 'ID:ID'\n");
    exit(1);
  }

  std::pair<heif_item_id, heif_item_id> pair;
  pair.first = atoi(s.substr(0, p).c_str());
  pair.second = atoi(s.substr(p + 1).c_str());
  return pair;
}


int main(int argc, char** argv)
{
  std::vector<heif_item_id> image_IDs;
  std::vector<std::pair<heif_item_id, heif_item_id>> metadata_IDs; // first: image, second: metadata

  while (true) {
    int option_index = 0;
    int c = getopt_long(argc, argv, "d:m:h", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'd':
        image_IDs.push_back(atoi(optarg));
        break;
      case 'm':
        metadata_IDs.push_back(parse_id_pair(optarg));
        break;
      case 'h':
        show_help(argv[0]);
        return 0;
    }
  }

  if (optind != argc - 1) {
    show_help(argv[0]);
    return 0;
  }


  const char* input_filename = argv[optind];

  // ==============================================================================

  try {
    heif::Context ctx;
    ctx.read_from_file(input_filename);


    // --- dump images

    for (auto id : image_IDs) {
      heif::ImageHandle handle = ctx.get_image_handle(id);

      heif::Image img = handle.decode_image(heif_colorspace_undefined, heif_chroma_undefined);

      std::vector<heif_channel> channel_candidates{
          heif_channel_Y,
          heif_channel_Cb,
          heif_channel_Cr,
          heif_channel_R,
          heif_channel_G,
          heif_channel_B,
          heif_channel_Alpha,
          heif_channel_interleaved
      };

      for (heif_channel channel : channel_candidates) {
        if (img.has_channel(channel)) {
          int width = img.get_width(channel);
          int height = img.get_height(channel);
          int bytes = (img.get_bits_per_pixel(channel) + 7) / 8;

          int stride;
          const uint8_t* p = img.get_plane(channel, &stride);
          for (int y = 0; y < height; y++) {
            fwrite(p + y * stride, width, bytes, stdout);
          }
        }
      }
    }


    // --- dump metadata

    for (auto idpair : metadata_IDs) {
      heif::ImageHandle handle = ctx.get_image_handle(idpair.first);
      std::vector<uint8_t> data = handle.get_metadata(idpair.second);
      fwrite(data.data(), data.size(), 1, stdout);
    }
  }
  catch (const heif::Error& err) {
    std::cerr << err.get_message() << "\n";
  }

  return 0;
}
