/*
 * HEIF image base codec.
 * Copyright (c) 2024 Dirk Farin <dirk.farin@gmail.com>
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

#include "chunk.h"
#include "context.h"
#include "codecs/avc_enc.h"
#include "codecs/hevc_enc.h"
#include "codecs/avif_enc.h"
#include "codecs/vvc_enc.h"
#include "codecs/jpeg2000_enc.h"
#include "codecs/jpeg_enc.h"

#if WITH_UNCOMPRESSED_CODEC
#include "codecs/uncompressed/unc_enc.h"
#endif


Chunk::Chunk(HeifContext* ctx, uint32_t track_id, heif_compression_format format)
    : m_ctx(ctx),
      m_track_id(track_id),
      m_compression_format(format)
{
  switch (format) {
    case heif_compression_HEVC:
      m_encoder = std::make_shared<Encoder_HEVC>();
      break;
    case heif_compression_AV1:
      m_encoder = std::make_shared<Encoder_AVIF>();
      break;
    case heif_compression_VVC:
      m_encoder = std::make_shared<Encoder_VVC>();
      break;
    case heif_compression_AVC:
      m_encoder = std::make_shared<Encoder_AVC>();
      break;
    case heif_compression_JPEG2000:
      m_encoder = std::make_shared<Encoder_JPEG2000>();
      break;
    case heif_compression_HTJ2K:
      m_encoder = std::make_shared<Encoder_HTJ2K>();
      break;
    case heif_compression_JPEG:
      m_encoder = std::make_shared<Encoder_JPEG>();
      break;
#if WITH_UNCOMPRESSED_CODEC
    case heif_compression_uncompressed:
      m_encoder = std::make_shared<Encoder_uncompressed>();
      break;
#endif
    case heif_compression_undefined:
    default:
      m_encoder = nullptr;
      break;
  }
}


Chunk::Chunk(HeifContext* ctx, uint32_t track_id,
             uint32_t first_sample, uint32_t num_samples, uint64_t file_offset, const std::shared_ptr<const Box_stsz>& stsz)
{
  m_ctx = ctx;
  m_track_id = track_id;

  m_first_sample = first_sample;
  m_last_sample = first_sample + num_samples - 1;

  m_next_sample_to_be_decoded = first_sample;

  for (uint32_t i=0;i<num_samples;i++) {
    SampleFileRange range;
    range.offset = file_offset;
    if (stsz->has_fixed_sample_size()) {
      range.size = stsz->get_fixed_sample_size();
    }
    else {
      assert(first_sample + i < stsz->num_samples());
      range.size = stsz->get_sample_sizes()[first_sample + i];
    }

    m_sample_ranges.push_back(range);

    file_offset += range.size;
  }
}


DataExtent Chunk::get_data_extent_for_sample(uint32_t n) const
{
  assert(n>= m_first_sample);
  assert(n<= m_last_sample);

  DataExtent extent;
  extent.set_file_range(m_ctx->get_heif_file(),
                        m_sample_ranges[n - m_first_sample].offset,
                        m_sample_ranges[n - m_first_sample].size);
  return extent;
}
