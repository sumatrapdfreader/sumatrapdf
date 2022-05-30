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

#ifndef LIBHEIF_ERROR_H
#define LIBHEIF_ERROR_H

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
#include <ostream>
#include <sstream>


#include "heif.h"


static constexpr char kSuccess[] = "Success";


namespace heif {

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
    enum heif_error_code error_code = heif_error_Ok;
    enum heif_suberror_code sub_error_code = heif_suberror_Unspecified;
    std::string message;

    Error();

    Error(heif_error_code c,
          heif_suberror_code sc = heif_suberror_Unspecified,
          const std::string& msg = "");

    static Error Ok;

    static const char kSuccess[];

    bool operator==(const Error& other) const
    { return error_code == other.error_code; }

    bool operator!=(const Error& other) const
    { return !(*this == other); }

    operator bool() const
    { return error_code != heif_error_Ok; }

    static const char* get_error_string(heif_error_code err);

    static const char* get_error_string(heif_suberror_code err);

    heif_error error_struct(ErrorBuffer* error_buffer) const;
  };


  inline std::ostream& operator<<(std::ostream& ostr, const Error& err)
  {
    ostr << err.error_code << "/" << err.sub_error_code;
    return ostr;
  }
}

#endif
