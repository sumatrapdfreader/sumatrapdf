/*
 * HEIF codec.
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

#ifndef HEIF_UNC_DEC_H
#define HEIF_UNC_DEC_H

#include "libheif/heif.h"
#include "box.h"
#include "error.h"

#include <memory>
#include <vector>
#include "codecs/decoder.h"
#include <utility>

class Box_uncC;
class Box_cmpd;
class Box_cpat;
class Box_cmpC;
class Box_icef;
class Box_splz;
class Box_sbpm;
class Box_snuc;
class Box_cloc;


class Decoder_uncompressed : public Decoder
{
public:
  explicit Decoder_uncompressed(std::shared_ptr<Box_uncC> uncC,
                                std::shared_ptr<Box_cmpd> cmpd,
                                std::shared_ptr<const Box_ispe> ispe);

  void set_cpat(std::shared_ptr<const Box_cpat> cpat) { m_cpat = std::move(cpat); }
  void set_cmpC(std::shared_ptr<const Box_cmpC> cmpC) { m_cmpC = std::move(cmpC); }
  void set_icef(std::shared_ptr<const Box_icef> icef) { m_icef = std::move(icef); }
  void set_cloc(std::shared_ptr<const Box_cloc> cloc) { m_cloc = std::move(cloc); }
  void set_splz(std::vector<std::shared_ptr<const Box_splz>> splz) { m_splz = std::move(splz); }
  void set_sbpm(std::vector<std::shared_ptr<const Box_sbpm>> sbpm) { m_sbpm = std::move(sbpm); }
  void set_snuc(std::vector<std::shared_ptr<const Box_snuc>> snuc) { m_snuc = std::move(snuc); }

  // Overloads accepting non-const shared_ptrs (from get_child_boxes)
  void set_splz(std::vector<std::shared_ptr<Box_splz>> splz) { m_splz.assign(splz.begin(), splz.end()); }
  void set_sbpm(std::vector<std::shared_ptr<Box_sbpm>> sbpm) { m_sbpm.assign(sbpm.begin(), sbpm.end()); }
  void set_snuc(std::vector<std::shared_ptr<Box_snuc>> snuc) { m_snuc.assign(snuc.begin(), snuc.end()); }

  heif_compression_format get_compression_format() const override { return heif_compression_uncompressed; }

  int get_luma_bits_per_pixel() const override;

  int get_chroma_bits_per_pixel() const override;

  Error get_coded_image_colorspace(heif_colorspace*, heif_chroma*) const override;

  bool has_alpha_component() const;

  Result<std::vector<uint8_t>> read_bitstream_configuration_data() const override;

  Result<std::shared_ptr<HeifPixelImage>>
  decode_single_frame_from_compressed_data(const struct heif_decoding_options& options,
                                           const struct heif_security_limits* limits) override;


  // Push data for one frame into decoder.
  Error
  decode_sequence_frame_from_compressed_data(bool upload_configuration_NALs,
                                             const heif_decoding_options& options,
                                             uintptr_t user_data,
                                             const heif_security_limits* limits) override;

  Error flush_decoder() override { return {}; }

  // Get a decoded frame from the decoder.
  // It may return NULL when there is buffering in the codec.
  Result<std::shared_ptr<HeifPixelImage> > get_decoded_frame(const heif_decoding_options& options,
                                                             uintptr_t* out_user_data,
                                                             const heif_security_limits* limits) override;

private:
  std::shared_ptr<const Box_uncC> m_uncC;
  std::shared_ptr<const Box_cmpd> m_cmpd;
  std::shared_ptr<const Box_ispe> m_ispe;
  std::shared_ptr<const Box_cpat> m_cpat;
  std::shared_ptr<const Box_cmpC> m_cmpC;
  std::shared_ptr<const Box_icef> m_icef;
  std::vector<std::shared_ptr<const Box_splz>> m_splz;
  std::vector<std::shared_ptr<const Box_sbpm>> m_sbpm;
  std::vector<std::shared_ptr<const Box_snuc>> m_snuc;
  std::shared_ptr<const Box_cloc> m_cloc;

  std::shared_ptr<HeifPixelImage> m_decoded_image;
  uintptr_t m_decoded_image_user_data;
};

#endif
