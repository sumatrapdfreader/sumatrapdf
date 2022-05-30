/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
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

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <cinttypes>
#include <cstddef>

#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <istream>
#include <string>

#include "error.h"


namespace heif {

  class StreamReader
  {
  public:
    virtual ~StreamReader() = default;

    virtual int64_t get_position() const = 0;

    enum grow_status
    {
      size_reached,   // requested size has been reached
      timeout,        // size has not been reached yet, but it may still grow further
      size_beyond_eof // size has not been reached and never will. The file has grown to its full size
    };

    // a StreamReader can maintain a timeout for waiting for new data
    virtual grow_status wait_for_file_size(int64_t target_size) = 0;

    // returns 'false' when we read out of the available file size
    virtual bool read(void* data, size_t size) = 0;

    virtual bool seek(int64_t position) = 0;

    bool seek_cur(int64_t position_offset)
    {
      return seek(get_position() + position_offset);
    }
  };


  class StreamReader_istream : public StreamReader
  {
  public:
    StreamReader_istream(std::unique_ptr<std::istream>&& istr);

    int64_t get_position() const override;

    grow_status wait_for_file_size(int64_t target_size) override;

    bool read(void* data, size_t size) override;

    bool seek(int64_t position) override;

  private:
    std::unique_ptr<std::istream> m_istr;
    int64_t m_length;
  };


  class StreamReader_memory : public StreamReader
  {
  public:
    StreamReader_memory(const uint8_t* data, int64_t size, bool copy);

    ~StreamReader_memory() override;

    int64_t get_position() const override;

    grow_status wait_for_file_size(int64_t target_size) override;

    bool read(void* data, size_t size) override;

    bool seek(int64_t position) override;

  private:
    const uint8_t* m_data;
    int64_t m_length;
    int64_t m_position;

    // if we made a copy of the data, we store a pointer to the owned memory area here
    uint8_t* m_owned_data = nullptr;
  };


  class StreamReader_CApi : public StreamReader
  {
  public:
    StreamReader_CApi(const heif_reader* func_table, void* userdata);

    int64_t get_position() const override
    { return m_func_table->get_position(m_userdata); }

    StreamReader::grow_status wait_for_file_size(int64_t target_size) override;

    bool read(void* data, size_t size) override
    { return !m_func_table->read(data, size, m_userdata); }

    bool seek(int64_t position) override
    { return !m_func_table->seek(position, m_userdata); }

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
                   uint64_t length,
                   BitstreamRange* parent = nullptr);

    // This function tries to make sure that the full data of this range is
    // available. You should call this before starting reading the range.
    // If you don't, you have to make sure that you do not read past the available data.
    StreamReader::grow_status wait_until_range_is_available();

    uint8_t read8();

    uint16_t read16();

    uint32_t read32();

    std::string read_string();

    bool read(uint8_t* data, int64_t n);

    bool prepare_read(int64_t nBytes);

    StreamReader::grow_status wait_for_available_bytes(int64_t nBytes);

    void skip_to_end_of_file()
    {
      // we do not actually move the file position here (because the stream may still be incomplete),
      // but we set all m_remaining to zero
      m_remaining = 0;

      if (m_parent_range) {
        m_parent_range->skip_to_end_of_file();
      }
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

    std::shared_ptr<StreamReader> get_istream()
    { return m_istr; }

    int get_nesting_level() const
    { return m_nesting_level; }

    int64_t get_remaining_bytes() const
    { return m_remaining; }

  private:
    std::shared_ptr<StreamReader> m_istr;
    BitstreamRange* m_parent_range = nullptr;
    int m_nesting_level = 0;

    int64_t m_remaining;
    bool m_error = false;

    // Note: 'nBytes' may not be larger than the number of remaining bytes
    void skip_without_advancing_file_pos(int64_t nBytes);
  };


  class BitReader
  {
  public:
    BitReader(const uint8_t* buffer, int len);

    int get_bits(int n);

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
    const uint8_t* data;
    int data_length;
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

    void write32(uint32_t);

    void write64(uint64_t);

    void write(int size, uint64_t value);

    void write(const std::string&);

    void write(const std::vector<uint8_t>&);

    void write(const StreamWriter&);

    void skip(int n);

    void insert(int nBytes);

    size_t data_size() const
    { return m_data.size(); }

    size_t get_position() const
    { return m_position; }

    void set_position(size_t pos)
    { m_position = pos; }

    void set_position_to_end()
    { m_position = m_data.size(); }

    const std::vector<uint8_t> get_data() const
    { return m_data; }

  private:
    std::vector<uint8_t> m_data;
    size_t m_position = 0;
  };
}

#endif
