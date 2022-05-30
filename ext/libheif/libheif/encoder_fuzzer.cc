/*
 * HEIF codec.
 * Copyright (c) 2018 struktur AG, Joachim Bauch <bauch@struktur.de>
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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include <memory>

#include "heif.h"

static void generate_plane(int width, int height, uint8_t* output, int stride)
{
  // TODO(fancycode): Fill with random data.
  if (width == stride) {
    memset(output, 0, width * height);
  }
  else {
    for (int y = 0; y < height; y++) {
      memset(output, 0, width);
      output += stride;
    }
  }
}

static size_t create_image(const uint8_t* data, size_t size, struct heif_image** image)
{
  if (size < 2) {
    return 0;
  }

  int width = data[0] + 16;
  int height = data[1] + 16;
  data += 2;
  size -= 2;
  // TODO(fancycode): Get colorspace/chroma from fuzzing input.
  heif_colorspace colorspace = heif_colorspace_YCbCr;
  heif_chroma chroma = heif_chroma_420;

  struct heif_error err = heif_image_create(width, height, colorspace, chroma, image);
  if (err.code != heif_error_Ok) {
    return 0;
  }

  int chroma_width = (width+1)/2;
  int chroma_height = (height+1)/2;

  err = heif_image_add_plane(*image, heif_channel_Y, width, height, 8);
  assert(err.code == heif_error_Ok);
  err = heif_image_add_plane(*image, heif_channel_Cb, chroma_width, chroma_height, 8);
  assert(err.code == heif_error_Ok);
  err = heif_image_add_plane(*image, heif_channel_Cr, chroma_width, chroma_height, 8);
  assert(err.code == heif_error_Ok);

  int stride;
  uint8_t* plane;

  plane = heif_image_get_plane(*image, heif_channel_Y, &stride);
  generate_plane(width, height, plane, stride);

  plane = heif_image_get_plane(*image, heif_channel_Cb, &stride);
  generate_plane(chroma_width, chroma_height, plane, stride);

  plane = heif_image_get_plane(*image, heif_channel_Cr, &stride);
  generate_plane(chroma_width, chroma_height, plane, stride);

  return 2;
}

class MemoryWriter
{
public:
  MemoryWriter() : data_(nullptr), size_(0), capacity_(0)
  {}

  ~MemoryWriter()
  {
    free(data_);
  }

  const uint8_t* data() const
  { return data_; }

  size_t size() const
  { return size_; }

  void write(const void* data, size_t size)
  {
    if (capacity_ - size_ < size) {
      size_t new_capacity = capacity_ + size;
      uint8_t* new_data = static_cast<uint8_t*>(malloc(new_capacity));
      assert(new_data);
      if (data_) {
        memcpy(new_data, data_, size_);
        free(data_);
      }
      data_ = new_data;
      capacity_ = new_capacity;
    }
    memcpy(&data_[size_], data, size);
    size_ += size;
  }

public:
  uint8_t* data_;
  size_t size_;
  size_t capacity_;
};

static struct heif_error writer_write(struct heif_context* ctx, const void* data, size_t size, void* userdata)
{
  MemoryWriter* writer = static_cast<MemoryWriter*>(userdata);
  writer->write(data, size);
  struct heif_error err{heif_error_Ok, heif_suberror_Unspecified, nullptr};
  return err;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
  struct heif_error err;
  std::shared_ptr<heif_context> context(heif_context_alloc(),
                                        [](heif_context* c) { heif_context_free(c); });
  assert(context);

  if (size < 2) {
    return 0;
  }

  int quality = (data[0] & 0x7F) % 101;
  bool lossless = (data[1] & 0x80);
  bool use_avif = (data[1] & 0x40);
  data += 2;
  size -= 2;

  static const size_t kMaxEncoders = 5;
  const heif_encoder_descriptor* encoder_descriptors[kMaxEncoders];
  int count = heif_context_get_encoder_descriptors(context.get(),
                                                   use_avif ? heif_compression_AV1 : heif_compression_HEVC,
                                                   nullptr,
                                                   encoder_descriptors, kMaxEncoders);
  assert(count >= 0);
  if (count == 0) {
    return 0;
  }

  heif_encoder* encoder;
  err = heif_context_get_encoder(context.get(), encoder_descriptors[0], &encoder);
  if (err.code != heif_error_Ok) {
    return 0;
  }

  heif_encoder_set_lossy_quality(encoder, quality);
  heif_encoder_set_lossless(encoder, lossless);

  struct heif_image* image = nullptr;
  size_t read = create_image(data, size, &image);
  assert(read <= size);
  if (!read) {
    heif_image_release(image);
    heif_encoder_release(encoder);
    return 0;
  }

  data += read;
  size -= read;

  struct heif_image_handle* img;
  err = heif_context_encode_image(context.get(), image, encoder, nullptr, &img);
  heif_image_release(image);
  heif_encoder_release(encoder);
  if (err.code != heif_error_Ok) {
    return 0;
  }

  heif_image_handle_release(img);

  MemoryWriter writer;
  struct heif_writer w;
  w.writer_api_version = 1;
  w.write = writer_write;
  heif_context_write(context.get(), &w, &writer);
  assert(writer.size() > 0);
  return 0;
}
