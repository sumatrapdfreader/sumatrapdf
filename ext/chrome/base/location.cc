// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#if defined(COMPILER_MSVC)
// MSDN says to #include <intrin.h>, but that breaks the VS2005 build.
extern "C" {
  void* _ReturnAddress();
}
#endif

#include "base/location.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"

namespace tracked_objects {

Location::Location(const char* function_name,
                   const char* file_name,
                   int line_number,
                   const void* program_counter)
    : function_name_(function_name),
      file_name_(file_name),
      line_number_(line_number),
      program_counter_(program_counter) {
}

Location::Location()
    : function_name_("Unknown"),
      file_name_("Unknown"),
      line_number_(-1),
      program_counter_(NULL) {
}

std::string Location::ToString() const {
  return std::string(function_name_) + "@" + file_name_ + ":" +
      base::IntToString(line_number_);
}

void Location::Write(bool display_filename, bool display_function_name,
                     std::string* output) const {
  base::StringAppendF(output, "%s[%d] ",
      display_filename ? file_name_ : "line",
      line_number_);

  if (display_function_name) {
    WriteFunctionName(output);
    output->push_back(' ');
  }
}

void Location::WriteFunctionName(std::string* output) const {
  // Translate "<" to "&lt;" for HTML safety.
  // TODO(jar): Support ASCII or html for logging in ASCII.
  for (const char *p = function_name_; *p; p++) {
    switch (*p) {
      case '<':
        output->append("&lt;");
        break;

      case '>':
        output->append("&gt;");
        break;

      default:
        output->push_back(*p);
        break;
    }
  }
}

base::DictionaryValue* Location::ToValue() const {
  base::DictionaryValue* dictionary = new base::DictionaryValue;
  dictionary->Set("file_name", base::Value::CreateStringValue(file_name_));
  // Note: This function name is not escaped, and templates have less than
  // characters, which means this is not suitable for display as HTML unless
  // properly escaped.
  dictionary->Set("function_name",
                  base::Value::CreateStringValue(function_name_));
  dictionary->Set("line_number", base::Value::CreateIntegerValue(line_number_));
  return dictionary;
}

#if defined(COMPILER_MSVC)
__declspec(noinline)
#endif
BASE_EXPORT const void* GetProgramCounter() {
#if defined(COMPILER_MSVC)
  return _ReturnAddress();
#elif defined(COMPILER_GCC)
  return __builtin_extract_return_addr(__builtin_return_address(0));
#endif  // COMPILER_GCC

  return NULL;
}

}  // namespace tracked_objects
