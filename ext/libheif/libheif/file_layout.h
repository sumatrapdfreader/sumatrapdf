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

#ifndef LIBHEIF_FILELAYOUT_H
#define LIBHEIF_FILELAYOUT_H

#include "error.h"
#include "bitstream.h"
#include "box.h"
#if ENABLE_EXPERIMENTAL_MINI_FORMAT
#include "mini.h"
#endif

#include <memory>
#include <vector>

class Box_moov;


class FileLayout
{
public:
  enum class WriteMode {
    Streaming, // 'mdat' data will be written to output immediately
    Floating,  // 'mdat' data will be held in memory until written
    TmpFile    // 'mdat' data will be written to temporary file and copied into final file
  };

  // Generate a file in WriteMode::Floating
  FileLayout();

  Error read(const std::shared_ptr<StreamReader>& stream, const heif_security_limits* limits);

  // For WriteMode::Streaming, writer cannot be null.
  void set_write_mode(WriteMode writeMode, const std::shared_ptr<StreamWriter>& writer = nullptr);

  // For WriteMode::Streaming, stream must be null.
  Error write(std::shared_ptr<StreamWriter>& stream);


  // --- access to boxes

  std::shared_ptr<Box_ftyp> get_ftyp_box() { return m_ftyp_box; }

  std::shared_ptr<Box_meta> get_meta_box() { return m_meta_box; }

#if ENABLE_EXPERIMENTAL_MINI_FORMAT
  std::shared_ptr<Box_mini> get_mini_box() { return m_mini_box; }
#endif

  std::shared_ptr<Box_moov> get_moov_box() { return m_moov_box; }

private:
  WriteMode m_writeMode = WriteMode::Floating;

  const static uint64_t INVALID_FILE_SIZE = 0xFFFFFFFFFFFFFFFF;

  uint64_t m_file_size = INVALID_FILE_SIZE;

  // the first one is always 'ftyp'
  std::vector<std::shared_ptr<Box>> m_boxes; // TODO: do we need this ?

  std::shared_ptr<Box_ftyp> m_ftyp_box;
  std::shared_ptr<Box_meta> m_meta_box;
#if ENABLE_EXPERIMENTAL_MINI_FORMAT
  std::shared_ptr<Box_mini> m_mini_box;
#endif
  std::shared_ptr<Box_moov> m_moov_box;


  uint64_t m_max_length = 0; // Length seen so far. It can grow over time.

  std::shared_ptr<StreamReader> m_stream_reader;
  std::shared_ptr<StreamWriter> m_stream_writer;

  static const uint64_t INITIAL_FTYP_REQUEST = 1024; // should be enough to read ftyp and next box header
  static const uint16_t MAXIMUM_BOX_HEADER_SIZE = 32;
};

#endif //LIBHEIF_FILELAYOUT_H
