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

#include "bitstream.h"

#include <utility>
#include <cstring>
#include <cassert>

#include "common_utils.h"

#if !defined(HAVE_BIT)
#include <type_traits>
#else
#include <bit>
#endif

#define MAX_UVLC_LEADING_ZEROS 20


StreamReader_istream::StreamReader_istream(std::unique_ptr<std::istream>&& istr)
    : m_istr(std::move(istr))
{
  m_istr->seekg(0, std::ios_base::end);
  m_length = m_istr->tellg();
  m_istr->seekg(0, std::ios_base::beg);
}

uint64_t StreamReader_istream::get_position() const
{
  return m_istr->tellg();
}

StreamReader::grow_status StreamReader_istream::wait_for_file_size(uint64_t target_size)
{
  return (target_size > m_length) ? grow_status::size_beyond_eof : grow_status::size_reached;
}

bool StreamReader_istream::read(void* data, size_t size)
{
  uint64_t end_pos = get_position() + size;
  if (end_pos > m_length) {
    return false;
  }

  m_istr->read((char*) data, size);
  return true;
}

bool StreamReader_istream::seek(uint64_t position)
{
  if (position > m_length)
    return false;

  m_istr->seekg(position, std::ios_base::beg);
  return true;
}


StreamReader_memory::StreamReader_memory(const uint8_t* data, size_t size, bool copy)
    : m_length(size),
      m_position(0)
{
  if (copy) {
    m_owned_data = new uint8_t[m_length];
    memcpy(m_owned_data, data, size);

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

uint64_t StreamReader_memory::get_position() const
{
  return m_position;
}

StreamReader::grow_status StreamReader_memory::wait_for_file_size(uint64_t target_size)
{
  return (target_size > m_length) ? grow_status::size_beyond_eof : grow_status::size_reached;
}

bool StreamReader_memory::read(void* data, size_t size)
{
  uint64_t end_pos = m_position + size;
  if (end_pos > m_length) {
    return false;
  }

  memcpy(data, &m_data[m_position], size);
  m_position += size;

  return true;
}

bool StreamReader_memory::seek(uint64_t position)
{
  if (position > m_length)
    return false;

  m_position = position;
  return true;
}


StreamReader_CApi::StreamReader_CApi(const heif_reader* func_table, void* userdata)
    : m_func_table(func_table), m_userdata(userdata)
{
}

StreamReader::grow_status StreamReader_CApi::wait_for_file_size(uint64_t target_size)
{
  heif_reader_grow_status status = m_func_table->wait_for_file_size(target_size, m_userdata);
  switch (status) {
    case heif_reader_grow_status_size_reached:
      return grow_status::size_reached;
    case heif_reader_grow_status_timeout:
      return grow_status::timeout;
    case heif_reader_grow_status_size_beyond_eof:
      return grow_status::size_beyond_eof;
    default:
      assert(0);
      return grow_status::size_beyond_eof;
  }
}


BitstreamRange::BitstreamRange(std::shared_ptr<StreamReader> istr,
                               size_t length,
                               BitstreamRange* parent)
    : m_istr(std::move(istr)), m_parent_range(parent), m_remaining(length)
{
  if (parent) {
    m_nesting_level = parent->m_nesting_level + 1;
  }
}


BitstreamRange::BitstreamRange(std::shared_ptr<StreamReader> istr,
                               size_t start,
                               size_t end) // one past end
  : m_istr(std::move(istr)), m_remaining(end)
{
  bool success = m_istr->seek(start);
  assert(success);
  (void)success; // TODO
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


int16_t BitstreamRange::read16s()
{
  uint16_t v = read16();

  if (v & 0x8000) {
    auto val = static_cast<int16_t>((~v) & 0x7fff);
    return static_cast<int16_t>(-val - 1);
  }
  else {
    return static_cast<int16_t>(v);
  }
}


uint32_t BitstreamRange::read24()
{
  if (!prepare_read(3)) {
    return 0;
  }

  uint8_t buf[3];

  auto istr = get_istream();
  bool success = istr->read((char*) buf, 3);

  if (!success) {
    set_eof_while_reading();
    return 0;
  }

  return (uint32_t) ((buf[0] << 16) |
                     (buf[1] << 8) |
                     (buf[2]));
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

  return four_bytes_to_uint32(buf[0], buf[1], buf[2], buf[3]);
}


uint64_t BitstreamRange::read_uint(int len)
{
  switch (len)
  {
    case 8:
      return read8();
    case 16:
      return read16();
    case 24:
      return read24();
    case 32:
      return read32();
    case 64:
      return read64();
    default:
      assert(false);
      return 0;
  }
}


int32_t BitstreamRange::read32s()
{
  uint32_t v = read32();

  if (v & 0x80000000) {
    return -static_cast<int32_t>((~v) & 0x7fffffff) -1;
  }
  else {
    return static_cast<int32_t>(v);
  }
}


uint64_t BitstreamRange::read64()
{
  if (!prepare_read(8)) {
    return 0;
  }

  uint8_t buf[8];

  auto istr = get_istream();
  bool success = istr->read((char*) buf, 8);

  if (!success) {
    set_eof_while_reading();
    return 0;
  }

  return ((static_cast<uint64_t>(buf[0]) << 56) |
          (static_cast<uint64_t>(buf[1]) << 48) |
          (static_cast<uint64_t>(buf[2]) << 40) |
          (static_cast<uint64_t>(buf[3]) << 32) |
          (static_cast<uint64_t>(buf[4]) << 24) |
          (static_cast<uint64_t>(buf[5]) << 16) |
          (static_cast<uint64_t>(buf[6]) << 8) |
          (static_cast<uint64_t>(buf[7])));
}


int64_t BitstreamRange::read64s()
{
  uint64_t v = read64();

  if (v & 0x8000000000000000) {
    return -static_cast<int64_t >((~v) & 0x7fffffffffffffff) -1;
  }
  else {
    return static_cast<int64_t >(v);
  }
}


float BitstreamRange::read_float32()
{
#if __cpp_lib_bit_cast >= 201806L
  uint32_t i = read32();
  return std::bit_cast<float>(i); // this works directly on the value layout, thus we do not have to worry about memory layout
#else
  // compiler too old to support bit_cast

  // TODO: I am not sure this works everywhere as there seem to be systems where
  //       the float byte order is different from the integer endianness
  //       https://en.wikipedia.org/wiki/Endianness#Floating_point
  uint32_t i = read32();
  float f;
  memcpy(&f, &i, sizeof(float));
  return f;
#endif
}


void StreamWriter::write_float32(float v)
{
#if __cpp_lib_bit_cast >= 201806L
  write32(std::bit_cast<uint32_t>(v)); // this works directly on the value layout, thus we do not have to worry about memory layout
#else
  // compiler too old to support bit_cast

  // TODO: I am not sure this works everywhere as there seem to be systems where
  //       the float byte order is different from the integer endianness
  //       https://en.wikipedia.org/wiki/Endianness#Floating_point
  uint32_t i;
  memcpy(&i, &v, sizeof(float));
  write32(i);
#endif
}


std::string BitstreamRange::read_string()
{
  std::string str;

  // Reading a string when no more data is available, returns an empty string.
  // Such a case happens, for example, when reading a 'url' box without content.
  if (eof()) {
    return std::string();
  }

  auto istr = get_istream();

  for (;;) {
    if (!prepare_read(1)) {
      return std::string();
    }

    char c;
    bool success = istr->read(&c, 1);

    if (!success) {
      set_eof_while_reading();
      return std::string();
    }

    if (c == 0 || m_remaining==0) {
      break;
    }
    else {
      str += (char) c;
    }
  }

  return str;
}


std::string BitstreamRange::read_fixed_string(int len)
{
  std::string str;

  if (!prepare_read(len)) {
    return std::string();
  }

  auto istr = get_istream();

  uint8_t n;
  bool success = istr->read(&n, 1);
  if (!success || n > len - 1) {
    return {};
  }

  for (int i = 0; i < n; i++) {
    char c;
    success = istr->read(&c, 1);

    if (!success) {
      set_eof_while_reading();
      return std::string();
    }

    str += (char) c;
  }

  istr->seek_cur(len-n-1);

  return str;
}


std::string BitstreamRange::read_string_until_eof()
{
  size_t n = get_remaining_bytes();

  [[maybe_unused]] bool success = prepare_read(n);
  assert(success); // we are reading exactly the rest of the box

  std::string str;
  str.resize(n);
  get_istream()->read(str.data(), n);

  return str;
}


bool BitstreamRange::read(uint8_t* data, size_t n)
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


bool BitstreamRange::prepare_read(size_t nBytes)
{
  // Note: we do not test for negative nBytes anymore because we now use the unsigned size_t

  if (m_remaining < nBytes) {
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


StreamReader::grow_status BitstreamRange::wait_for_available_bytes(size_t nBytes)
{
  int64_t target_size = m_istr->get_position() + nBytes;

  return m_istr->wait_for_file_size(target_size);
}


void BitstreamRange::skip_without_advancing_file_pos(size_t n)
{
  assert(n <= m_remaining);

  m_remaining -= n;

  if (m_parent_range) {
    m_parent_range->skip_without_advancing_file_pos(n);
  }
}


BitReader::BitReader(const uint8_t* buffer, int len)
  : data_start(buffer),
    data_length(len)
{
  data = buffer;
  bytes_remaining = len;

  nextbits = 0;
  nextbits_cnt = 0;

  refill();
}


void BitReader::reset()
{
  data = data_start;
  bytes_remaining = data_length;

  nextbits = 0;
  nextbits_cnt = 0;

  refill();
}


uint32_t BitReader::get_bits(int n)
{
  assert(n <= 32);

  if (nextbits_cnt < n) {
    refill();
  }

  uint64_t val = nextbits;
  val >>= 64 - n;

#if AVOID_FUZZER_FALSE_POSITIVE
  // Shifting an unsigned integer left such that some MSBs fall out is well defined in C++ despite the fuzzer claiming otherwise.
  nextbits &= (0xffffffffffffffffULL >> n);
#endif

  nextbits <<= n;
  nextbits_cnt -= n;

  return static_cast<uint32_t>(val);
}


uint8_t BitReader::get_bits8(int n)
{
  assert(n>0 && n <= 8);
  return static_cast<uint8_t>(get_bits(n));
}

uint16_t BitReader::get_bits16(int n)
{
  assert(n>0 && n <= 16);
  return static_cast<uint16_t>(get_bits(n));
}

uint32_t BitReader::get_bits32(int n)
{
  assert(n>0 && n <= 32);
  return static_cast<uint32_t>(get_bits(n));
}

int32_t BitReader::get_bits32s()
{
  uint32_t bits = get_bits(32);
  return static_cast<int32_t>(bits);
}


bool BitReader::get_flag()
{
  return (get_bits(1) == 0x01);
}

std::vector<uint8_t> BitReader::read_bytes(uint32_t n)
{
  // TODO: this implementation isn't very efficient
  std::vector<uint8_t> bytes;
  for (uint32_t i = 0; i < n; i++) {
    bytes.push_back(get_bits8(8));
  }
  return bytes;
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

#if AVOID_FUZZER_FALSE_POSITIVE
  nextbits &= (0xffffffffffffffffULL >> n);
#endif

  nextbits <<= n;
  nextbits_cnt -= n;
}

void BitReader::skip_bits_fast(int n)
{
#if AVOID_FUZZER_FALSE_POSITIVE
  nextbits &= (0xffffffffffffffffULL >> n);
#endif

  nextbits <<= n;
  nextbits_cnt -= n;
}

void BitReader::skip_to_byte_boundary()
{
  int nskip = (nextbits_cnt & 7);

#if AVOID_FUZZER_FALSE_POSITIVE
  nextbits &= (0xffffffffffffffffULL >> nskip);
#endif

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


void StreamWriter::write16s(int16_t v16s)
{
  uint16_t v;
  if (v16s >= 0) {
    v = static_cast<uint16_t>(v16s);
  }
  else {
    auto val = static_cast<uint16_t>((-v16s-1));
    v = static_cast<uint16_t>(~val);
  }

  write16(v);
}


void StreamWriter::write24(uint32_t v)
{
  size_t required_size = m_position + 3;

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  m_data[m_position++] = uint8_t((v >> 16) & 0xFF);
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


void StreamWriter::write32s(int32_t v32s)
{
  uint32_t v;
  if (v32s >= 0) {
    v = static_cast<uint32_t>(v32s);
  }
  else {
    v = ~static_cast<uint32_t>((-v32s-1));
  }

  write32(v);
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


void StreamWriter::write64s(int64_t v)
{
  write64(reinterpret_cast<uint64_t&>(v));
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


void StreamWriter::write(const std::string& str, bool end_with_null)
{
  size_t required_size = m_position + str.size() + (end_with_null ? 1 : 0);

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  for (size_t i = 0; i < str.size(); i++) {
    m_data[m_position++] = str[i];
  }

  if (end_with_null) {
    m_data[m_position++] = 0;
  }
}


void StreamWriter::write_fixed_string(std::string s, size_t len)
{
  size_t required_size = m_position + len;

  if (required_size > m_data.size()) {
    m_data.resize(required_size);
  }

  size_t n_chars = std::min(s.length(), len - 1);
  assert(n_chars <= 255);
  m_data[m_position++] = static_cast<uint8_t>(n_chars);

  for (size_t i = 0; i < s.size() && i < len - 1; i++) {
    m_data[m_position++] = s[i];
  }

  for (size_t i = s.size(); i < len - 1; i++) {
    m_data[m_position++] = 0;
  }
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
