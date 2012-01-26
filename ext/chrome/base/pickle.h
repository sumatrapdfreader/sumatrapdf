// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PICKLE_H__
#define BASE_PICKLE_H__
#pragma once

#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "base/string16.h"

// This class provides facilities for basic binary value packing and unpacking.
//
// The Pickle class supports appending primitive values (ints, strings, etc.)
// to a pickle instance.  The Pickle instance grows its internal memory buffer
// dynamically to hold the sequence of primitive values.   The internal memory
// buffer is exposed as the "data" of the Pickle.  This "data" can be passed
// to a Pickle object to initialize it for reading.
//
// When reading from a Pickle object, it is important for the consumer to know
// what value types to read and in what order to read them as the Pickle does
// not keep track of the type of data written to it.
//
// The Pickle's data has a header which contains the size of the Pickle's
// payload.  It can optionally support additional space in the header.  That
// space is controlled by the header_size parameter passed to the Pickle
// constructor.
//
class BASE_EXPORT Pickle {
 public:
  // Initialize a Pickle object using the default header size.
  Pickle();

  // Initialize a Pickle object with the specified header size in bytes, which
  // must be greater-than-or-equal-to sizeof(Pickle::Header).  The header size
  // will be rounded up to ensure that the header size is 32bit-aligned.
  explicit Pickle(int header_size);

  // Initializes a Pickle from a const block of data.  The data is not copied;
  // instead the data is merely referenced by this Pickle.  Only const methods
  // should be used on the Pickle when initialized this way.  The header
  // padding size is deduced from the data length.
  Pickle(const char* data, int data_len);

  // Initializes a Pickle as a deep copy of another Pickle.
  Pickle(const Pickle& other);

  // Note: There are no virtual methods in this class.  This destructor is
  // virtual as an element of defensive coding.  Other classes have derived from
  // this class, and there is a *chance* that they will cast into this base
  // class before destruction.  At least one such class does have a virtual
  // destructor, suggesting at least some need to call more derived destructors.
  virtual ~Pickle();

  // Performs a deep copy.
  Pickle& operator=(const Pickle& other);

  // Returns the size of the Pickle's data.
  size_t size() const { return header_size_ + header_->payload_size; }

  // Returns the data for this Pickle.
  const void* data() const { return header_; }

  // Methods for reading the payload of the Pickle.  To read from the start of
  // the Pickle, initialize *iter to NULL.  If successful, these methods return
  // true.  Otherwise, false is returned to indicate that the result could not
  // be extracted.
  bool ReadBool(void** iter, bool* result) const;
  bool ReadInt(void** iter, int* result) const;
  bool ReadLong(void** iter, long* result) const;
  bool ReadSize(void** iter, size_t* result) const;
  bool ReadUInt16(void** iter, uint16* result) const;
  bool ReadUInt32(void** iter, uint32* result) const;
  bool ReadInt64(void** iter, int64* result) const;
  bool ReadUInt64(void** iter, uint64* result) const;
  bool ReadString(void** iter, std::string* result) const;
  bool ReadWString(void** iter, std::wstring* result) const;
  bool ReadString16(void** iter, string16* result) const;
  bool ReadData(void** iter, const char** data, int* length) const;
  bool ReadBytes(void** iter, const char** data, int length) const;

  // Safer version of ReadInt() checks for the result not being negative.
  // Use it for reading the object sizes.
  bool ReadLength(void** iter, int* result) const;

  // Methods for adding to the payload of the Pickle.  These values are
  // appended to the end of the Pickle's payload.  When reading values from a
  // Pickle, it is important to read them in the order in which they were added
  // to the Pickle.
  bool WriteBool(bool value) {
    return WriteInt(value ? 1 : 0);
  }
  bool WriteInt(int value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteLong(long value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteSize(size_t value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt16(uint16 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt32(uint32 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteInt64(int64 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteUInt64(uint64 value) {
    return WriteBytes(&value, sizeof(value));
  }
  bool WriteString(const std::string& value);
  bool WriteWString(const std::wstring& value);
  bool WriteString16(const string16& value);
  bool WriteData(const char* data, int length);
  bool WriteBytes(const void* data, int data_len);

  // Same as WriteData, but allows the caller to write directly into the
  // Pickle. This saves a copy in cases where the data is not already
  // available in a buffer. The caller should take care to not write more
  // than the length it declares it will. Use ReadData to get the data.
  // Returns NULL on failure.
  //
  // The returned pointer will only be valid until the next write operation
  // on this Pickle.
  char* BeginWriteData(int length);

  // For Pickles which contain variable length buffers (e.g. those created
  // with BeginWriteData), the Pickle can
  // be 'trimmed' if the amount of data required is less than originally
  // requested.  For example, you may have created a buffer with 10K of data,
  // but decided to only fill 10 bytes of that data.  Use this function
  // to trim the buffer so that we don't send 9990 bytes of unused data.
  // You cannot increase the size of the variable buffer; only shrink it.
  // This function assumes that the length of the variable buffer has
  // not been changed.
  void TrimWriteData(int length);

  // Payload follows after allocation of Header (header size is customizable).
  struct Header {
    uint32 payload_size;  // Specifies the size of the payload.
  };

  // Returns the header, cast to a user-specified type T.  The type T must be a
  // subclass of Header and its size must correspond to the header_size passed
  // to the Pickle constructor.
  template <class T>
  T* headerT() {
    DCHECK_EQ(header_size_, sizeof(T));
    return static_cast<T*>(header_);
  }
  template <class T>
  const T* headerT() const {
    DCHECK_EQ(header_size_, sizeof(T));
    return static_cast<const T*>(header_);
  }

  // Returns true if the given iterator could point to data with the given
  // length. If there is no room for the given data before the end of the
  // payload, returns false.
  bool IteratorHasRoomFor(const void* iter, int len) const {
    if ((len < 0) || (iter < header_) || iter > end_of_payload())
      return false;
    const char* end_of_region = reinterpret_cast<const char*>(iter) + len;
    // Watch out for overflow in pointer calculation, which wraps.
    return (iter <= end_of_region) && (end_of_region <= end_of_payload());
  }

 protected:
  size_t payload_size() const { return header_->payload_size; }

  char* payload() {
    return reinterpret_cast<char*>(header_) + header_size_;
  }
  const char* payload() const {
    return reinterpret_cast<const char*>(header_) + header_size_;
  }

  // Returns the address of the byte immediately following the currently valid
  // header + payload.
  char* end_of_payload() {
    // We must have a valid header_.
    return payload() + payload_size();
  }
  const char* end_of_payload() const {
    // This object may be invalid.
    return header_ ? payload() + payload_size() : NULL;
  }

  size_t capacity() const {
    return capacity_;
  }

  // Resizes the buffer for use when writing the specified amount of data. The
  // location that the data should be written at is returned, or NULL if there
  // was an error. Call EndWrite with the returned offset and the given length
  // to pad out for the next write.
  char* BeginWrite(size_t length);

  // Completes the write operation by padding the data with NULL bytes until it
  // is padded. Should be paired with BeginWrite, but it does not necessarily
  // have to be called after the data is written.
  void EndWrite(char* dest, int length);

  // Resize the capacity, note that the input value should include the size of
  // the header: new_capacity = sizeof(Header) + desired_payload_capacity.
  // A realloc() failure will cause a Resize failure... and caller should check
  // the return result for true (i.e., successful resizing).
  bool Resize(size_t new_capacity);

  // Aligns 'i' by rounding it up to the next multiple of 'alignment'
  static size_t AlignInt(size_t i, int alignment) {
    return i + (alignment - (i % alignment)) % alignment;
  }

  // Moves the iterator by the given number of bytes, making sure it is aligned.
  // Pointer (iterator) is NOT aligned, but the change in the pointer
  // is guaranteed to be a multiple of sizeof(uint32).
  static void UpdateIter(void** iter, int bytes) {
    *iter = static_cast<char*>(*iter) + AlignInt(bytes, sizeof(uint32));
  }

  // Find the end of the pickled data that starts at range_start.  Returns NULL
  // if the entire Pickle is not found in the given data range.
  static const char* FindNext(size_t header_size,
                              const char* range_start,
                              const char* range_end);

  // The allocation granularity of the payload.
  static const int kPayloadUnit;

 private:
  Header* header_;
  size_t header_size_;  // Supports extra data between header and payload.
  // Allocation size of payload (or -1 if allocation is const).
  size_t capacity_;
  size_t variable_buffer_offset_;  // IF non-zero, then offset to a buffer.

  FRIEND_TEST_ALL_PREFIXES(PickleTest, Resize);
  FRIEND_TEST_ALL_PREFIXES(PickleTest, FindNext);
  FRIEND_TEST_ALL_PREFIXES(PickleTest, FindNextWithIncompleteHeader);
  FRIEND_TEST_ALL_PREFIXES(PickleTest, IteratorHasRoom);
};

#endif  // BASE_PICKLE_H__
