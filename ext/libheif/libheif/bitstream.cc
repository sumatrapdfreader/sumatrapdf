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

#include "bitstream.h"

#include <utility>
#include <cstring>
#include <cassert>

#define MAX_UVLC_LEADING_ZEROS 20

using namespace heif;


StreamReader_istream::StreamReader_istream(std::unique_ptr<std::istream>&& istr)
    : m_istr(std::move(istr))
{
  m_istr->seekg(0, std::ios_base::end);
  m_length = m_istr->tellg();
  m_istr->seekg(0, std::ios_base::beg);
}

int64_t StreamReader_istream::get_position() const
{
  return m_istr->tellg();
}

StreamReader::grow_status StreamReader_istream::wait_for_file_size(int64_t target_size)
{
  return (target_size > m_length) ? size_beyond_eof : size_reached;
}

bool StreamReader_istream::read(void* data, size_t size)
{
  int64_t end_pos = get_position() + size;
  if (end_pos > m_length) {
    return false;
  }

  m_istr->read((char*) data, size);
  return true;
}

bool StreamReader_istream::seek(int64_t position)
{
  if (position > m_length)
    return false;

  m_istr->seekg(position, std::ios_base::beg);
  return true;
}


StreamReader_memory::StreamReader_memory(const uint8_t* data, int64_t size, bool copy)
    : m_length(size),
      m_position(0)
{
  if (copy) {
    m_owned_data = new uint8_t[m_length];
    memcpy(m_owned_data, data, m_length);

    m_data = m_owned_data;
  }
  else {
    m_data = data;
  }
}

StreamReader_memory::~StreamReader_memory()
{
  if (m_owned_data) {
    delete[] m_owned_data;
  }
}

int64_t StreamReader_memory::get_position() const
{
  return m_position;
}

StreamReader::grow_status StreamReader_memory::wait_for_file_size(int64_t target_size)
{
  return (target_size > m_length) ? size_beyond_eof : size_reached;
}

bool StreamReader_memory::read(void* data, size_t size)
{
  int64_t end_pos = m_position + size;
  if (end_pos > m_length) {
    return false;
  }

  memcpy(data, &m_data[m_position], size);
  m_position += size;

  return true;
}

bool StreamReader_memory::seek(int64_t position)
{
  if (position > m_length || position < 0)
    return false;

  m_position = position;
  return true;
}


StreamReader_CApi::StreamReader_CApi(const heif_reader* func_table, void* userdata)
    : m_func_table(func_table), m_userdata(userdata)
{
}

StreamReader::grow_status StreamReader_CApi::wait_for_file_size(int64_t target_size)
{
  heif_reader_grow_status status = m_func_table->wait_for_file_size(target_size, m_userdata);
  switch (status) {
    case heif_reader_grow_status_size_reached:
      return size_reached;
    case heif_reader_grow_status_timeout:
      return timeout;
    case heif_reader_grow_status_size_beyond_eof:
      return size_beyond_eof;
    default:
      assert(0);
      return size_beyond_eof;
  }
}


BitstreamRange::BitstreamRange(std::shared_ptr<StreamReader> istr,
                               uint64_t length,
                               BitstreamRange* parent)
    : m_istr(std::move(istr)), m_parent_range(parent), m_remaining(length)
{
  if (parent) {
    m_nesting_level = parent->m_nesting_level + 1;
  }
}


StreamReader::grow_status BitstreamRange::wait_until_range_is_available()
{
  return m_istr->wait_for_file_size(m_istr->get_position() + m_remaining);
}


uint8_t BitstreamRange::read8()
{
  if (!prepare_read(1)) {
    return 0;
  }

  uint8_t buf;

  auto istr = get_istream();
  bool success = istr->read((char*) &buf, 1);

  if (!success) {
    set_eof_while_reading();
    return 0;
  }

  return buf;
}


uint16_t BitstreamRange::read16()
{
  if (!prepare_read(2)) {
    return 0;
  }

  uint8_t buf[2];

  auto istr = get_istream();
  bool success = istr->read((char*) buf, 2);

  if (!success) {
    set_eof_while_reading();
    return 0;
  }

  return static_cast<uint16_t>((buf[0] << 8) | (buf[1]));
}


uint32_t BitstreamRange::read32()
{
  if (!prepare_read(4)) {
    return 0;
  }

  uint8_t buf[4];

  auto istr = get_istream();
  bool success = istr->read((char*) buf, 4);

  if (!success) {
    set_eof_while_reading();
    return 0;
  }

  return ((buf[0] << 24) |
          (buf[1] << 16) |
          (buf[2] << 8) |
          (buf[3]));
}


std::string BitstreamRange::read_string()
{
  std::string str;

  // Reading a string when no more data is available, returns an empty string.
  // Such a case happens, for example, when reading a 'url' box without content.
  if (eof()) {
    return std::string();
  }

  for (;;) {
    if (!prepare_read(1)) {
      return std::string();
    }

    auto istr = get_istream();
    char c;
    bool success = istr->read(&c, 1);

    if (!success) {
      set_eof_while_reading();
      return std::string();
    }

    if (c == 0) {
      break;
    }
    else {
      str += (char) c;
    }
  }

  return str;
}


bool BitstreamRange::read(uint8_t* data, int64_t n)
{
  if (!prepare_read(n)) {
    return false;
  }

  auto istr = get_istream();
  bool success = istr->read(data, n);

  if (!success) {
    set_eof_while_reading();
  }

  return success;
}


bool BitstreamRange::prepare_read(int64_t nBytes)
{
  if (nBytes < 0) {
    // --- we cannot read negative amounts of bytes

    assert(false);
    return false;
  }
  else if (m_remaining < nBytes) {
    // --- not enough data left in box -> move to end of box and set error flag

    skip_to_end_of_box();

    m_error = true;
    return false;
  }
  else {
    // --- this is the normal case (m_remaining >= nBytes)

    if (m_parent_range) {
      if (!m_parent_range->prepare_read(nBytes)) {
        return false;
      }
    }

    m_remaining -= nBytes;

    return true;
  }
}


StreamReader::grow_status BitstreamRange::wait_for_available_bytes(int64_t nBytes)
{
  int64_t target_size = m_istr->get_position() + nBytes;

  return m_istr->wait_for_file_size(target_size);
}


void BitstreamRange::skip_without_advancing_file_pos(int64_t n)
{
  assert(n <= m_remaining);

  m_remaining -= n;

  if (m_parent_range) {
    m_parent_range->skip_without_advancing_file_pos(n);
  }
}


BitReader::BitReader(const uint8_t* buffer, int len)
{
  data = buffer;
  data_length = len;
  bytes_remaining = len;

  nextbits = 0;
  nextbits_cnt = 0;

  refill();
}

int BitReader::get_bits(int n)
{
  if (nextbits_cnt < n) {
    refill();
  }

  uint64_t val = nextbits;
  val >>= 64 - n;

  nextbits <<= n;
  nextbits_cnt -= n;

  return (int) val;
}

int BitReader::get_bits_fast(int n)
{
  assert(nextbits_cnt >= n);

  uint64_t val = nextbits;
  val >>= 64 - n;

  nextbits <<= n;
  nextbits_cnt -= n;

  return (int) val;
}

int BitReader::peek_bits(int n)
{
  if (nextbits_cnt < n) {
    refill();
  }

  uint64_t val = nextbits;
  val >>= 64 - n;

  return (int) val;
}

void BitReader::skip_bytes(int nBytes)
{
  // TODO: this is slow
  while (nBytes--) {
    skip_bits(8);
  }
}

void BitReader::skip_bits(int n)
{
  if (nextbits_cnt < n) {
    refill();
  }

  nextbits <<= n;
  nextbits_cnt -= n;
}

void BitReader::skip_bits_fast(int n)
{
  nextbits <<= n;
  nextbits_cnt -= n;
}

void BitReader::skip_to_byte_boundary()
{
  int nskip = (nextbits_cnt & 7);

  nextbits <<= nskip;
  nextbits_cnt -= nskip;
}

bool BitReader::get_uvlc(int* value)
{
  int num_zeros = 0;

  while (get_bits(1) == 0) {
    num_zeros++;

    if (num_zeros > MAX_UVLC_LEADING_ZEROS) { return false; }
  }

  int offset = 0;
  if (num_zeros != 0) {
    offset = (int) get_bits(num_zeros);
    *value = offset + (1 << num_zeros) - 1;
    assert(*value > 0);
    return true;
  }
  else {
    *value = 0;
    return true;
  }
}

bool BitReader::get_svlc(int* value)
{
  int v;
  if (!get_uvlc(&v)) {
    return false;
  }
  else if (v == 0) {
    *value = v;
    return true;
  }

  bool negative = ((v & 1) == 0);
  *value = negative ? -v / 2 : (v + 1) / 2;
  return true;
}

void BitReader::refill()
{
#if 0
  // TODO: activate me once I'm sure this works
  while (nextbits_cnt <= 64-8 && bytes_remaining) {
    uint64_t newval = *data++;
    bytes_remaining--;

    nextbits_cnt += 8;
    newval <<= 64-nextbits_cnt;
    nextbits |= newval;
  }
#else
  int shift = 64 - nextbits_cnt;

  while (shift >= 8 && bytes_remaining) {
    uint64_t newval = *data++;
    bytes_remaining--;

    shift -= 8;
    newval <<= shift;
    nextbits |= newval;
  }

  nextbits_cnt = 64 - shift;
#endif
}


void StreamWriter::write8(uint8_t v)
{
  if (m_position == m_data.size()) {
    m_data.push_back(v);
    m_position++;
  }
  else {
    m_data[m_position++] = v;
  }
}


void StreamWriter::write16(uint16_t v)
{
  size_t required_size = m_position + 2;

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  m_data[m_position++] = uint8_t((v >> 8) & 0xFF);
  m_data[m_position++] = uint8_t(v & 0xFF);
}


void StreamWriter::write32(uint32_t v)
{
  size_t required_size = m_position + 4;

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  m_data[m_position++] = uint8_t((v >> 24) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 16) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 8) & 0xFF);
  m_data[m_position++] = uint8_t(v & 0xFF);
}


void StreamWriter::write64(uint64_t v)
{
  size_t required_size = m_position + 8;

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  m_data[m_position++] = uint8_t((v >> 56) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 48) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 40) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 32) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 24) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 16) & 0xFF);
  m_data[m_position++] = uint8_t((v >> 8) & 0xFF);
  m_data[m_position++] = uint8_t(v & 0xFF);
}


void StreamWriter::write(int size, uint64_t value)
{
  if (size == 1) {
    assert(value <= 0xFF);
    write8((uint8_t) value);
  }
  else if (size == 2) {
    assert(value <= 0xFFFF);
    write16((uint16_t) value);
  }
  else if (size == 4) {
    assert(value <= 0xFFFFFFFF);
    write32((uint32_t) value);
  }
  else if (size == 8) {
    write64((uint64_t) value);
  }
  else {
    assert(false); // unimplemented size
  }
}


void StreamWriter::write(const std::string& str)
{
  size_t required_size = m_position + str.size() + 1;

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  for (size_t i = 0; i < str.size(); i++) {
    m_data[m_position++] = str[i];
  }

  m_data[m_position++] = 0;
}


void StreamWriter::write(const std::vector<uint8_t>& vec)
{
  size_t required_size = m_position + vec.size();

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  memcpy(m_data.data() + m_position, vec.data(), vec.size());
  m_position += vec.size();
}


void StreamWriter::write(const StreamWriter& writer)
{
  size_t required_size = m_position + writer.get_data().size();

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  const auto& data = writer.get_data();

  memcpy(m_data.data() + m_position, data.data(), data.size());

  m_position += data.size();
}


void StreamWriter::skip(int n)
{
  assert(m_position == m_data.size());
  m_data.resize(m_data.size() + n);
  m_position += n;
}


void StreamWriter::insert(int nBytes)
{
  assert(nBytes >= 0);

  if (nBytes == 0) {
    return;
  }

  m_data.resize(m_data.size() + nBytes);

  if (m_position < m_data.size() - nBytes) {
    memmove(m_data.data() + m_position + nBytes,
            m_data.data() + m_position,
            m_data.size() - nBytes - m_position);
  }
}
