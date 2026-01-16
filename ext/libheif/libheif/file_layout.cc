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

#include "file_layout.h"
#include "sequences/seq_boxes.h"
#include <limits>


FileLayout::FileLayout()
{
  auto ftyp = std::make_shared<Box_ftyp>();
  ftyp->set_output_position(0);
  m_boxes.push_back(ftyp);

  // TODO: these variables are not used yet
  (void)m_writeMode;
  (void)m_file_size;
}


Error FileLayout::read(const std::shared_ptr<StreamReader>& stream, const heif_security_limits* limits)
{
  m_boxes.clear();

  m_stream_reader = stream;

  // --- read initial range, large enough to cover 'ftyp' box

  m_max_length = stream->request_range(0, INITIAL_FTYP_REQUEST);

  if (m_max_length < MAXIMUM_BOX_HEADER_SIZE) {
    return {heif_error_Invalid_input,
            heif_suberror_Unspecified,
            "File size too small."};
  }

  // --- read 'ftyp' box header

  BitstreamRange ftyp_hdr_range(m_stream_reader, m_max_length);
  BoxHeader ftyp_header;
  Error err;
  err = ftyp_header.parse_header(ftyp_hdr_range);
  if (err) {
    return err;
  }

  // --- check whether it is a valid 'ftyp' box header

  if (ftyp_header.get_short_type() != fourcc("ftyp")) {
    return {heif_error_Invalid_input,
            heif_suberror_No_ftyp_box,
            "File does not start with 'ftyp' box."};
  }

  uint64_t ftyp_size = ftyp_header.get_box_size();

  if (ftyp_size == 0) {
    return {heif_error_Invalid_input,
            heif_suberror_No_ftyp_box,
            "ftyp box shall not be the only box in the file"};
  }

  if (ftyp_size > m_max_length) {
    return {heif_error_Invalid_input,
            heif_suberror_No_ftyp_box,
            "ftyp box larger than initial read range"};
  }

  // --- read the 'ftyp' box

  BitstreamRange ftyp_range(m_stream_reader, 0, ftyp_size);
  std::shared_ptr<Box> ftyp_box;
  err = Box::read(ftyp_range, &ftyp_box, limits);

  m_boxes.push_back(ftyp_box);
  m_ftyp_box = std::dynamic_pointer_cast<Box_ftyp>(ftyp_box);


  // --- skip through box headers until we find the 'meta' box

  uint64_t next_box_start = ftyp_size;

  bool meta_found = false;
  bool mini_found = false;
  bool moov_found = false;

  for (;;) {
    // TODO: overflow
    uint64_t next_box_header_end = next_box_start + MAXIMUM_BOX_HEADER_SIZE;
    if (next_box_header_end > m_max_length) {
      m_max_length = stream->request_range(next_box_start, next_box_header_end);
    }

    if (next_box_header_end > m_max_length) {
      if (meta_found || mini_found || moov_found) {
        return Error::Ok;
      }
      else {
        return {heif_error_Invalid_input,
                heif_suberror_Unspecified,
                "Insufficient input data"};
      }
    }

    BitstreamRange box_range(m_stream_reader, next_box_start, m_max_length);
    BoxHeader box_header;
    err = box_header.parse_header(box_range);
    if (err) {
      return err;
    }

    if (box_header.get_short_type() == fourcc("meta")) {
      const uint64_t meta_box_start = next_box_start;
      if (box_header.get_box_size() == 0) {
        // TODO: get file-size from stream and compute box size
        return {heif_error_Invalid_input,
                heif_suberror_No_meta_box,
                "Cannot read meta box with unspecified size"};
      }

      // TODO: overflow
      uint64_t end_of_meta_box = meta_box_start + box_header.get_box_size();
      if (m_max_length < end_of_meta_box) {
        m_max_length = m_stream_reader->request_range(meta_box_start, end_of_meta_box);
      }

      if (m_max_length < end_of_meta_box) {
        return {heif_error_Invalid_input,
                heif_suberror_No_meta_box,
                "Cannot read full meta box"};
      }

      BitstreamRange meta_box_range(m_stream_reader, meta_box_start, end_of_meta_box);
      std::shared_ptr<Box> meta_box;
      err = Box::read(meta_box_range, &meta_box, limits);
      if (err) {
        return err;
      }

      m_boxes.push_back(meta_box);
      m_meta_box = std::dynamic_pointer_cast<Box_meta>(meta_box);
      meta_found = true;
    }

#if ENABLE_EXPERIMENTAL_MINI_FORMAT
    // TODO: this is basically the same as the meta box case above, with different error handling.
    if (box_header.get_short_type() == fourcc("mini")) {
      const uint64_t mini_box_start = next_box_start;
      if (box_header.get_box_size() == 0) {
        // TODO: get file-size from stream and compute box size
        return {heif_error_Invalid_input,
                heif_suberror_Invalid_mini_box,
                "Cannot read mini box with unspecified size"};
      }
      uint64_t end_of_mini_box = mini_box_start + box_header.get_box_size();
      if (m_max_length < end_of_mini_box) {
        m_max_length = m_stream_reader->request_range(mini_box_start, end_of_mini_box);
      }

      if (m_max_length < end_of_mini_box) {
        return {heif_error_Invalid_input,
                heif_suberror_Invalid_mini_box,
                "Cannot read full mini box"};
      }
      BitstreamRange mini_box_range(m_stream_reader, mini_box_start, end_of_mini_box);
      std::shared_ptr<Box> mini_box;
      err = Box::read(mini_box_range, &mini_box, heif_get_global_security_limits());
      if (err) {
        std::cout << "error reading mini box" << std::endl;
        return err;
      }

      m_boxes.push_back(mini_box);
      m_mini_box = std::dynamic_pointer_cast<Box_mini>(mini_box);
      if (m_mini_box == nullptr) {
        std::cout << "error casting mini box" << std::endl;
      }

      mini_found = true;
    }
#endif

    if (box_header.get_short_type() == fourcc("moov")) {
      const uint64_t moov_box_start = next_box_start;
      if (box_header.get_box_size() == 0) {
        // TODO: get file-size from stream and compute box size
        return {heif_error_Invalid_input,
                heif_suberror_No_moov_box,
                "Cannot read moov box with unspecified size"};
      }

      // TODO: overflow
      uint64_t end_of_moov_box = moov_box_start + box_header.get_box_size();
      if (m_max_length < end_of_moov_box) {
        m_max_length = m_stream_reader->request_range(moov_box_start, end_of_moov_box);
      }

      if (m_max_length < end_of_moov_box) {
        return {heif_error_Invalid_input,
                heif_suberror_No_moov_box,
                "Cannot read full moov box"};
      }

      BitstreamRange moov_box_range(m_stream_reader, moov_box_start, end_of_moov_box);
      std::shared_ptr<Box> moov_box;
      err = Box::read(moov_box_range, &moov_box, limits);
      if (err) {
        return err;
      }

      m_boxes.push_back(moov_box);
      m_moov_box = std::dynamic_pointer_cast<Box_moov>(moov_box);

      moov_found = true;
    }

    uint64_t boxSize = box_header.get_box_size();
    if (boxSize == Box::size_until_end_of_file) {
      if (meta_found || mini_found || moov_found) {
        return Error::Ok;
      }
      else {
        return {heif_error_Invalid_input,
                heif_suberror_Unspecified,
                "No meta box found"};
      }
    }

    if (std::numeric_limits<uint64_t>::max() - boxSize < next_box_start) {
      return {heif_error_Invalid_input,
              heif_suberror_Unspecified,
              "Box size too large, integer overflow"};
    }

    next_box_start = next_box_start + boxSize;
  }

  return Error::Ok;
}


void FileLayout::set_write_mode(WriteMode writeMode, const std::shared_ptr<StreamWriter>& writer)
{

}


Error FileLayout::write(std::shared_ptr<StreamWriter>& stream)
{
  return Error::Ok;
}
