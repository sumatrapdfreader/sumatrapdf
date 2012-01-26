// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Copied from strings/stringpiece.h with modifications
//
// A string-like object that points to a sized piece of memory.
//
// Functions or methods may use const StringPiece& parameters to accept either
// a "const char*" or a "string" value that will be implicitly converted to
// a StringPiece.  The implicit conversion means that it is often appropriate
// to include this .h file in other files rather than forward-declaring
// StringPiece as would be appropriate for most other Google classes.
//
// Systematic usage of StringPiece is encouraged as it will reduce unnecessary
// conversions from "const char*" to "string" and back again.
//
// StringPiece16 is similar to StringPiece but for base::string16 instead of
// std::string. We do not define as large of a subset of the STL functions
// from basic_string as in StringPiece, but this can be changed if these
// functions (find, find_first_of, etc.) are found to be useful in this context.
//

#ifndef BASE_STRING_PIECE_H_
#define BASE_STRING_PIECE_H_
#pragma once

#include <iosfwd>
#include <string>

#include "base/base_export.h"
#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/string16.h"

namespace base {

template <typename STRING_TYPE> class BasicStringPiece;
typedef BasicStringPiece<std::string> StringPiece;
typedef BasicStringPiece<string16> StringPiece16;

namespace internal {

// Defines the types, methods, operators, and data members common to both
// StringPiece and StringPiece16. Do not refer to this class directly, but
// rather to BasicStringPiece, StringPiece, or StringPiece16.
template <typename STRING_TYPE> class StringPieceDetail {
 public:
  // standard STL container boilerplate
  typedef size_t size_type;
  typedef typename STRING_TYPE::value_type value_type;
  typedef const value_type* pointer;
  typedef const value_type& reference;
  typedef const value_type& const_reference;
  typedef ptrdiff_t difference_type;
  typedef const value_type* const_iterator;
  typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

  static const size_type npos;

 public:
  // We provide non-explicit singleton constructors so users can pass
  // in a "const char*" or a "string" wherever a "StringPiece" is
  // expected (likewise for char16, string16, StringPiece16).
  StringPieceDetail() : ptr_(NULL), length_(0) {}
  StringPieceDetail(const value_type* str)
      : ptr_(str),
        length_((str == NULL) ? 0 : STRING_TYPE::traits_type::length(str)) {}
  StringPieceDetail(const STRING_TYPE& str)
      : ptr_(str.data()), length_(str.size()) {}
  StringPieceDetail(const value_type* offset, size_type len)
      : ptr_(offset), length_(len) {}
  StringPieceDetail(const typename STRING_TYPE::const_iterator& begin,
                    const typename STRING_TYPE::const_iterator& end)
      : ptr_((end > begin) ? &(*begin) : NULL),
        length_((end > begin) ? (size_type)(end - begin) : 0) {}

  // data() may return a pointer to a buffer with embedded NULs, and the
  // returned buffer may or may not be null terminated.  Therefore it is
  // typically a mistake to pass data() to a routine that expects a NUL
  // terminated string.
  const value_type* data() const { return ptr_; }
  size_type size() const { return length_; }
  size_type length() const { return length_; }
  bool empty() const { return length_ == 0; }

  void clear() {
    ptr_ = NULL;
    length_ = 0;
  }
  void set(const value_type* data, size_type len) {
    ptr_ = data;
    length_ = len;
  }
  void set(const value_type* str) {
    ptr_ = str;
    length_ = str ? STRING_TYPE::traits_type::length(str) : 0;
  }

  value_type operator[](size_type i) const { return ptr_[i]; }

  void remove_prefix(size_type n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(size_type n) {
    length_ -= n;
  }

  int compare(const BasicStringPiece<STRING_TYPE>& x) const {
    int r = wordmemcmp(
        ptr_, x.ptr_, (length_ < x.length_ ? length_ : x.length_));
    if (r == 0) {
      if (length_ < x.length_) r = -1;
      else if (length_ > x.length_) r = +1;
    }
    return r;
  }

  STRING_TYPE as_string() const {
    // std::string doesn't like to take a NULL pointer even with a 0 size.
    return empty() ? STRING_TYPE() : STRING_TYPE(data(), size());
  }

  const_iterator begin() const { return ptr_; }
  const_iterator end() const { return ptr_ + length_; }
  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(ptr_ + length_);
  }
  const_reverse_iterator rend() const {
    return const_reverse_iterator(ptr_);
  }

  size_type max_size() const { return length_; }
  size_type capacity() const { return length_; }

  static int wordmemcmp(const value_type* p,
                        const value_type* p2,
                        size_type N) {
    return STRING_TYPE::traits_type::compare(p, p2, N);
  }

 protected:
  const value_type* ptr_;
  size_type     length_;
};

template <typename STRING_TYPE>
const typename StringPieceDetail<STRING_TYPE>::size_type
StringPieceDetail<STRING_TYPE>::npos =
    typename StringPieceDetail<STRING_TYPE>::size_type(-1);

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
extern template class BASE_EXPORT StringPieceDetail<std::string>;
extern template class BASE_EXPORT StringPieceDetail<string16>;
#endif

BASE_EXPORT void CopyToString(const StringPiece& self, std::string* target);
BASE_EXPORT void AppendToString(const StringPiece& self, std::string* target);
BASE_EXPORT StringPieceDetail<std::string>::size_type copy(
    const StringPiece& self,
    char* buf,
    StringPieceDetail<std::string>::size_type n,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find(
    const StringPiece& self,
    const StringPiece& s,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find(
    const StringPiece& self,
    char c,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type rfind(
    const StringPiece& self,
    const StringPiece& s,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type rfind(
    const StringPiece& self,
    char c,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_first_of(
    const StringPiece& self,
    const StringPiece& s,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_first_not_of(
    const StringPiece& self,
    const StringPiece& s,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_first_not_of(
    const StringPiece& self,
    char c,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_last_of(
    const StringPiece& self,
    const StringPiece& s,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_last_of(
    const StringPiece& self,
    char c,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_last_not_of(
    const StringPiece& self,
    const StringPiece& s,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPieceDetail<std::string>::size_type find_last_not_of(
    const StringPiece& self,
    char c,
    StringPieceDetail<std::string>::size_type pos);
BASE_EXPORT StringPiece substr(const StringPiece& self,
                               StringPieceDetail<std::string>::size_type pos,
                               StringPieceDetail<std::string>::size_type n);
}  // namespace internal

// Defines the template type that is instantiated as either StringPiece or
// StringPiece16.
template <typename STRING_TYPE> class BasicStringPiece :
    public internal::StringPieceDetail<STRING_TYPE> {
 public:
  typedef typename internal::StringPieceDetail<STRING_TYPE>::value_type
      value_type;
  typedef typename internal::StringPieceDetail<STRING_TYPE>::size_type
      size_type;

  BasicStringPiece() {}
  BasicStringPiece(const value_type*str)
      : internal::StringPieceDetail<STRING_TYPE>(str) {}
  BasicStringPiece(const STRING_TYPE& str)
      : internal::StringPieceDetail<STRING_TYPE>(str) {}
  BasicStringPiece(const value_type* offset, size_type len)
      : internal::StringPieceDetail<STRING_TYPE>(offset, len) {}
  BasicStringPiece(const typename STRING_TYPE::const_iterator& begin,
                   const typename STRING_TYPE::const_iterator& end)
      : internal::StringPieceDetail<STRING_TYPE>(begin, end) {}
};

// Specializes BasicStringPiece for std::string to add a few operations that
// are not needed for string16.
template <> class BasicStringPiece<std::string> :
    public internal::StringPieceDetail<std::string> {
 public:
  BasicStringPiece() {}
  BasicStringPiece(const char* str)
      : internal::StringPieceDetail<std::string>(str) {}
  BasicStringPiece(const std::string& str)
      : internal::StringPieceDetail<std::string>(str) {}
  BasicStringPiece(const char* offset, size_type len)
      : internal::StringPieceDetail<std::string>(offset, len) {}
  BasicStringPiece(const std::string::const_iterator& begin,
                   const std::string::const_iterator& end)
      : internal::StringPieceDetail<std::string>(begin, end) {}

  // Prevent the following overload of set() from hiding the definitions in the
  // base class.
  using internal::StringPieceDetail<std::string>::set;

  void set(const void* data, size_type len) {
    ptr_ = reinterpret_cast<const value_type*>(data);
    length_ = len;
  }

  void CopyToString(std::string* target) const {
    internal::CopyToString(*this, target);
  }

  void AppendToString(std::string* target) const {
    internal::AppendToString(*this, target);
  }

  // Does "this" start with "x"
  bool starts_with(const BasicStringPiece& x) const {
    return ((length_ >= x.length_) &&
            (wordmemcmp(ptr_, x.ptr_, x.length_) == 0));
  }

  // Does "this" end with "x"
  bool ends_with(const BasicStringPiece& x) const {
    return ((length_ >= x.length_) &&
            (wordmemcmp(ptr_ + (length_-x.length_), x.ptr_, x.length_) == 0));
  }

  size_type copy(char* buf, size_type n, size_type pos = 0) const {
    return internal::copy(*this, buf, n, pos);
  }

  size_type find(const BasicStringPiece& s, size_type pos = 0) const {
    return internal::find(*this, s, pos);
  }

  size_type find(char c, size_type pos = 0) const {
    return internal::find(*this, c, pos);
  }

  size_type rfind(const BasicStringPiece& s, size_type pos = npos) const {
    return internal::rfind(*this, s, pos);
  }

  size_type rfind(char c, size_type pos = npos) const {
    return internal::rfind(*this, c, pos);
  }

  size_type find_first_of(const BasicStringPiece& s, size_type pos = 0) const {
    return internal::find_first_of(*this, s, pos);
  }

  size_type find_first_of(char c, size_type pos = 0) const {
    return find(c, pos);
  }

  size_type find_first_not_of(const BasicStringPiece& s,
                              size_type pos = 0) const {
    return internal::find_first_not_of(*this, s, pos);
  }

  size_type find_first_not_of(char c, size_type pos = 0) const {
    return internal::find_first_not_of(*this, c, pos);
  }

  size_type find_last_of(const BasicStringPiece& s,
                         size_type pos = npos) const {
    return internal::find_last_of(*this, s, pos);
  }

  size_type find_last_of(char c, size_type pos = npos) const {
    return rfind(c, pos);
  }

  size_type find_last_not_of(const BasicStringPiece& s,
                             size_type pos = npos) const {
    return internal::find_last_not_of(*this, s, pos);
  }

  size_type find_last_not_of(char c, size_type pos = npos) const {
    return internal::find_last_not_of(*this, c, pos);
  }

  BasicStringPiece substr(size_type pos, size_type n = npos) const {
    return internal::substr(*this, pos, n);
  }
};

// MSVC doesn't like complex extern templates and DLLs.
#if !defined(COMPILER_MSVC)
// We can't explicitly declare the std::string instantiation here because it was
// already instantiated when specialized, above. Not only is it a no-op, but
// currently it also crashes Clang (see http://crbug.com/107412).
extern template class BASE_EXPORT BasicStringPiece<string16>;
#endif

BASE_EXPORT bool operator==(const StringPiece& x, const StringPiece& y);

inline bool operator!=(const StringPiece& x, const StringPiece& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece& x, const StringPiece& y) {
  const int r = StringPiece::wordmemcmp(
      x.data(), y.data(), (x.size() < y.size() ? x.size() : y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece& x, const StringPiece& y) {
  return y < x;
}

inline bool operator<=(const StringPiece& x, const StringPiece& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece& x, const StringPiece& y) {
  return !(x < y);
}

inline bool operator==(const StringPiece16& x, const StringPiece16& y) {
  if (x.size() != y.size())
    return false;

  return StringPiece16::wordmemcmp(x.data(), y.data(), x.size()) == 0;
}

inline bool operator!=(const StringPiece16& x, const StringPiece16& y) {
  return !(x == y);
}

inline bool operator<(const StringPiece16& x, const StringPiece16& y) {
  const int r = StringPiece16::wordmemcmp(
      x.data(), y.data(), (x.size() < y.size() ? x.size() : y.size()));
  return ((r < 0) || ((r == 0) && (x.size() < y.size())));
}

inline bool operator>(const StringPiece16& x, const StringPiece16& y) {
  return y < x;
}

inline bool operator<=(const StringPiece16& x, const StringPiece16& y) {
  return !(x > y);
}

inline bool operator>=(const StringPiece16& x, const StringPiece16& y) {
  return !(x < y);
}

BASE_EXPORT std::ostream& operator<<(std::ostream& o,
                                     const StringPiece& piece);

}  // namespace base

// We provide appropriate hash functions so StringPiece and StringPiece16 can
// be used as keys in hash sets and maps.

// This hash function is copied from base/hash_tables.h. We don't use the
// ones already defined for string and string16 directly because it would
// require the string constructors to be called, which we don't want.
#define HASH_STRING_PIECE(StringPieceType, string_piece)                \
  std::size_t result = 0;                                               \
  for (StringPieceType::const_iterator i = string_piece.begin();        \
       i != string_piece.end(); ++i)                                    \
    result = (result * 131) + *i;                                       \
  return result;                                                        \

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)

template<>
struct hash<base::StringPiece> {
  std::size_t operator()(const base::StringPiece& sp) const {
    HASH_STRING_PIECE(base::StringPiece, sp);
  }
};
template<>
struct hash<base::StringPiece16> {
  std::size_t operator()(const base::StringPiece16& sp16) const {
    HASH_STRING_PIECE(base::StringPiece16, sp16);
  }
};

#elif defined(COMPILER_MSVC)

inline size_t hash_value(const base::StringPiece& sp) {
  HASH_STRING_PIECE(base::StringPiece, sp);
}
inline size_t hash_value(const base::StringPiece16& sp16) {
  HASH_STRING_PIECE(base::StringPiece16, sp16);
}

#endif  // COMPILER

}  // namespace BASE_HASH_NAMESPACE

#endif  // BASE_STRING_PIECE_H_
