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

#ifndef LIBHEIF_ERROR_H
#define LIBHEIF_ERROR_H

#include <cinttypes>
#include <cstddef>

#include <vector>
#include <string>
#include <memory>
#include <limits>
#include <istream>
#include <ostream>
#include <sstream>

#include "libheif/heif.h"
#include <cassert>
#include <variant>
#include <utility>


extern const heif_error heif_error_null_pointer_argument;


class ErrorBuffer
{
public:
  ErrorBuffer() = default;

  void set_success()
  {
    m_error_message = c_success;
  }

  void set_error(const std::string& err)
  {
    m_buffer = err;
    m_error_message = m_buffer.c_str();
  }

  const char* get_error() const
  {
    return m_error_message;
  }

private:
  constexpr static const char* c_success = "Success";
  std::string m_buffer;
  const char* m_error_message = c_success;
};


class Error
{
public:
  heif_error_code error_code = heif_error_Ok;
  heif_suberror_code sub_error_code = heif_suberror_Unspecified;
  std::string message;

  Error();

  Error(heif_error_code c,
        heif_suberror_code sc = heif_suberror_Unspecified,
        const std::string& msg = "");

  static Error from_heif_error(const heif_error&);

  static const Error Ok;

  static const Error InternalError;

  static const char kSuccess[];

  bool operator==(const Error& other) const { return error_code == other.error_code; }

  bool operator!=(const Error& other) const { return !(*this == other); }

  Error operator||(const Error& other) const {
    if (error_code != heif_error_Ok) {
      return *this;
    }
    else {
      return other;
    }
  }

  operator bool() const { return error_code != heif_error_Ok; }

  static const char* get_error_string(heif_error_code err);

  static const char* get_error_string(heif_suberror_code err);

  heif_error error_struct(ErrorBuffer* error_buffer) const;
};


inline std::ostream& operator<<(std::ostream& ostr, const Error& err)
{
  ostr << err.error_code << "/" << err.sub_error_code;
  return ostr;
}


template <typename T> class Result
{
public:
  Result() = default;

  Result(T v) : m_data(std::move(v)) {}

  Result(const Error& e) : m_data(e) {}

  operator bool() const { return std::holds_alternative<T>(m_data); }

  //void set(const T& v) { m_data = v; }

  // Pointer-like access for `r->member`
  T* operator->()
  {
    assert(*this); // Uses the operator bool() above
    return &std::get<T>(m_data);
  }

  // Dereference access for `*r`
  T& operator*()
  {
    assert(*this);
    return std::get<T>(m_data);
  }

  [[nodiscard]] bool is_error() const {
    return std::holds_alternative<Error>(m_data);
  }

  // Accessor for the error, if it exists
  [[nodiscard]] const Error& error() const
  {
    if (*this) {
      return Error::Ok;
    }

    return std::get<Error>(m_data);
  }

  // Directly get the C error struct.
  heif_error error_struct(ErrorBuffer* error_buffer) const
  {
    if (*this) {
      return heif_error_success;
    }

    return std::get<Error>(m_data).error_struct(error_buffer);
  }

  std::variant<T, Error> m_data;
};

#endif
