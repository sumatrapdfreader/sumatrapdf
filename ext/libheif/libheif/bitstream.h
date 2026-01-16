/*
 * HEIF codec.
 * Copyright (c) 2017 Dirk Farin <dirk.farin@gmail.com>
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

#ifndef LIBHEIF_BITSTREAM_H
#define LIBHEIF_BITSTREAM_H

#include <cinttypes>
#include <cstddef>

#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <istream>
#include <string>
#include <cassert>

#include "error.h"
#include <algorithm>


class StreamReader
{
public:
  virtual ~StreamReader() = default;

  virtual uint64_t get_position() const = 0;

  enum class grow_status : uint8_t
  {
    size_reached,   // requested size has been reached
    timeout,        // size has not been reached yet, but it may still grow further
    size_beyond_eof // size has not been reached and never will. The file has grown to its full size
  };

  // a StreamReader can maintain a timeout for waiting for new data
  virtual grow_status wait_for_file_size(uint64_t target_size) = 0;

  // returns 'false' when we read out of the available file size
  virtual bool read(void* data, size_t size) = 0;

  virtual bool seek(uint64_t position) = 0;

  bool seek_cur(uint64_t position_offset)
  {
    return seek(get_position() + position_offset);
  }

  // Informs the reader implementation that we will process data in the given range.
  // The reader can use this information to retrieve a larger chunk of data instead of individual read() calls.
  // Returns the file size that was made available, but you still have to check each read() call.
  // Returning a value shorter than the requested range end indicates to libheif that the data is not available.
  // Returns 0 on error.
  virtual uint64_t request_range(uint64_t start, uint64_t end_pos) {
    return std::numeric_limits<uint64_t>::max();
  }

  virtual void release_range(uint64_t start, uint64_t end_pos) { }

  virtual void preload_range_hint(uint64_t start, uint64_t end_pos) { }

  Error get_error() const {
    return m_last_error;
  }

  void clear_last_error() { m_last_error = {}; }

protected:
  Error m_last_error;
};

#include <iostream>

class StreamReader_istream : public StreamReader
{
public:
  StreamReader_istream(std::unique_ptr<std::istream>&& istr);

  uint64_t get_position() const override;

  grow_status wait_for_file_size(uint64_t target_size) override;

  bool read(void* data, size_t size) override;

  bool seek(uint64_t position) override;

  uint64_t request_range(uint64_t start, uint64_t end_pos) override {
    // std::cout << "[istream] request_range " << start << " - " << end_pos << "\n";
    return std::min(end_pos, m_length);
  }

  void release_range(uint64_t start, uint64_t end_pos) override {
    // std::cout << "[istream] release_range " << start << " - " << end_pos << "\n";
  }

  void preload_range_hint(uint64_t start, uint64_t end_pos) override {
    // std::cout << "[istream] preload_range_hint " << start << " - " << end_pos << "\n";
  }

private:
  std::unique_ptr<std::istream> m_istr;
  uint64_t m_length;
};


class StreamReader_memory : public StreamReader
{
public:
  StreamReader_memory(const uint8_t* data, size_t size, bool copy);

  ~StreamReader_memory() override;

  uint64_t get_position() const override;

  grow_status wait_for_file_size(uint64_t target_size) override;

  bool read(void* data, size_t size) override;

  bool seek(uint64_t position) override;

  // end_pos is last byte to read + 1. I.e. like a file size.
  uint64_t request_range(uint64_t start, uint64_t end_pos) override {
    return m_length;
  }

private:
  const uint8_t* m_data;
  uint64_t m_length;
  uint64_t m_position;

  // if we made a copy of the data, we store a pointer to the owned memory area here
  uint8_t* m_owned_data = nullptr;
};


class StreamReader_CApi : public StreamReader
{
public:
  StreamReader_CApi(const heif_reader* func_table, void* userdata);

  uint64_t get_position() const override { return m_func_table->get_position(m_userdata); }

  StreamReader::grow_status wait_for_file_size(uint64_t target_size) override;

  bool read(void* data, size_t size) override { return !m_func_table->read(data, size, m_userdata); }

  bool seek(uint64_t position) override { return !m_func_table->seek(position, m_userdata); }

  uint64_t request_range(uint64_t start, uint64_t end_pos) override {
    if (m_func_table->reader_api_version >= 2) {
      heif_reader_range_request_result result = m_func_table->request_range(start, end_pos, m_userdata);

      // convert error message string and release input string memory

      std::string error_msg;
      if (result.reader_error_msg) {
        error_msg = std::string{result.reader_error_msg};

        if (m_func_table->release_error_msg) {
          m_func_table->release_error_msg(result.reader_error_msg);
        }
      }

      switch (result.status) {
        case heif_reader_grow_status_size_reached:
          return end_pos;
        case heif_reader_grow_status_timeout:
          return 0; // invalid return value from callback
        case heif_reader_grow_status_size_beyond_eof:
          m_last_error = {heif_error_Invalid_input, heif_suberror_End_of_data, "Read beyond file size"};
          return result.range_end;
        case heif_reader_grow_status_error: {
          if (result.reader_error_msg) {
            std::stringstream sstr;
            sstr << "Input error (" << result.reader_error_code << ") : " << error_msg;
            m_last_error = {heif_error_Invalid_input, heif_suberror_Unspecified, sstr.str()};
          }
          else {
            std::stringstream sstr;
            sstr << "Input error (" << result.reader_error_code << ")";
            m_last_error = {heif_error_Invalid_input, heif_suberror_Unspecified, sstr.str()};
          }

          return 0; // error occurred
        }
        default:
          m_last_error = {heif_error_Invalid_input, heif_suberror_Unspecified, "Invalid input reader return value"};
          return 0;
      }
    }
    else {
      auto result = m_func_table->wait_for_file_size(end_pos, m_userdata);
      if (result == heif_reader_grow_status_size_reached) {
        return end_pos;
      }
      else {
        uint64_t pos = m_func_table->get_position(m_userdata);
        return bisect_filesize(pos,end_pos);
      }
    }
  }

  uint64_t bisect_filesize(uint64_t mini, uint64_t maxi) {
    // mini - <= filesize
    // maxi - >  filesize

    if (maxi == mini + 1) {
      return mini;
    }

    uint64_t pos = (mini + maxi) / 2;
    auto result = m_func_table->wait_for_file_size(pos, m_userdata);
    if (result == heif_reader_grow_status_size_reached) {
      return bisect_filesize(pos, maxi);
    }
    else {
      return bisect_filesize(mini, pos);
    }
  }

  void release_range(uint64_t start, uint64_t end_pos) override {
    if (m_func_table->reader_api_version >= 2) {
      m_func_table->release_file_range(start, end_pos, m_userdata);
    }
  }

  void preload_range_hint(uint64_t start, uint64_t end_pos) override {
    if (m_func_table->reader_api_version >= 2) {
      m_func_table->preload_range_hint(start, end_pos, m_userdata);
    }
  }

private:
  const heif_reader* m_func_table;
  void* m_userdata;
};


// This class simplifies safely reading part of a file (e.g. a box).
// It makes sure that we do not read past the boundaries of a box.
class BitstreamRange
{
public:
  BitstreamRange(std::shared_ptr<StreamReader> istr,
                 size_t length,
                 BitstreamRange* parent = nullptr);

  BitstreamRange(std::shared_ptr<StreamReader> istr,
                 size_t start,
                 size_t end); // one past end

  // This function tries to make sure that the full data of this range is
  // available. You should call this before starting reading the range.
  // If you don't, you have to make sure that you do not read past the available data.
  StreamReader::grow_status wait_until_range_is_available();

  uint8_t read8();

  uint16_t read16();

  int16_t read16s();

  /**
   * Read 24 bit unsigned integer from the bitstream.
   *
   * The data is assumed to be in big endian format and is returned as a 32 bit value.
   */
  uint32_t read24();

  uint32_t read32();

  int32_t read32s();

  uint64_t read64();

  uint64_t read_uint(int len);

  /**
   * Read 32 bit floating point value from the bitstream.
   *
   * The file data is assumed to be in big endian format.
   */
  float read_float32();

  int64_t read64s();

  std::string read_string();

  // A string stored with a fixed number of bytes. The first byte contains the string length and the extra bytes
  // are filled with a padding 0.
  std::string read_fixed_string(int len);

  std::string read_string_until_eof();

  bool read(uint8_t* data, size_t n);

  bool prepare_read(size_t nBytes);

  StreamReader::grow_status wait_for_available_bytes(size_t nBytes);

  void skip_to_end_of_file()
  {
    // we do not actually move the file position here (because the stream may still be incomplete),
    // but we set all m_remaining to zero
    m_remaining = 0;

    if (m_parent_range) {
      m_parent_range->skip_to_end_of_file();
    }
  }

  void skip(uint64_t n)
  {
    size_t actual_skip = std::min(static_cast<size_t>(n), m_remaining);

    if (m_parent_range) {
      // also advance position in parent range
      m_parent_range->skip_without_advancing_file_pos(actual_skip);
    }

    assert(actual_skip <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));

    m_istr->seek_cur(static_cast<int64_t>(actual_skip));
    m_remaining -= actual_skip;
  }

  void skip_to_end_of_box()
  {
    if (m_remaining > 0) {
      if (m_parent_range) {
        // also advance position in parent range
        m_parent_range->skip_without_advancing_file_pos(m_remaining);
      }

      m_istr->seek_cur(m_remaining);
      m_remaining = 0;
    }
  }

  void set_eof_while_reading()
  {
    m_remaining = 0;

    if (m_parent_range) {
      m_parent_range->set_eof_while_reading();
    }

    m_error = true;
  }

  bool eof() const
  {
    return m_remaining == 0;
  }

  bool error() const
  {
    return m_error;
  }

  Error get_error() const
  {
    if (m_error) {
      return Error(heif_error_Invalid_input,
                   heif_suberror_End_of_data);
    }
    else {
      return Error::Ok;
    }
  }

  std::shared_ptr<StreamReader> get_istream() { return m_istr; }

  int get_nesting_level() const { return m_nesting_level; }

  size_t get_remaining_bytes() const { return m_remaining; }

private:
  std::shared_ptr<StreamReader> m_istr;
  BitstreamRange* m_parent_range = nullptr;
  int m_nesting_level = 0;

  size_t m_remaining;
  bool m_error = false;

  // Note: 'nBytes' may not be larger than the number of remaining bytes
  void skip_without_advancing_file_pos(size_t nBytes);
};


class BitReader
{
public:
  BitReader(const uint8_t* buffer, int len);

  void reset();

  uint32_t get_bits(int n);

  uint8_t get_bits8(int n);

  uint16_t get_bits16(int n);

  uint32_t get_bits32(int n);

  int32_t get_bits32s();

  /**
   * Get a one-bit flag value.
   *
   * @returns true if the next bit value is 1, otherwise false
   */
  bool get_flag();

  std::vector<uint8_t> read_bytes(uint32_t n);

  int get_bits_fast(int n);

  int peek_bits(int n);

  void skip_bytes(int nBytes);

  void skip_bits(int n);

  void skip_bits_fast(int n);

  void skip_to_byte_boundary();

  bool get_uvlc(int* value);

  bool get_svlc(int* value);

  int get_current_byte_index() const
  {
    return data_length - bytes_remaining - nextbits_cnt / 8;
  }

  int64_t get_bits_remaining() const
  {
    return ((int64_t) bytes_remaining) * 8 + nextbits_cnt;
  }

private:
  const uint8_t* const data_start;
  const uint8_t* data;
  const int data_length;
  int bytes_remaining;

  uint64_t nextbits; // left-aligned bits
  int nextbits_cnt;

  void refill(); // refill to at least 56+1 bits
};


class StreamWriter
{
public:
  void write8(uint8_t);

  void write16(uint16_t);

  void write16s(int16_t);

  void write24(uint32_t);

  void write32(uint32_t);

  void write32s(int32_t);

  void write64(uint64_t);

  void write_float32(float);

  void write64s(int64_t);

  void write(int size, uint64_t value);

  void write(const std::string&, bool end_with_null = true);

  void write_fixed_string(std::string s, size_t len);

  void write(const std::vector<uint8_t>&);

  void write(const StreamWriter&);

  void skip(int n);

  void insert(int nBytes);

  size_t data_size() const { return m_data.size(); }

  size_t get_position() const { return m_position; }

  void set_position(size_t pos) { m_position = pos; }

  void set_position_to_end() { m_position = m_data.size(); }

  const std::vector<uint8_t> get_data() const { return m_data; }

private:
  std::vector<uint8_t> m_data;
  size_t m_position = 0;
};

#endif
