// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Abstract interface to return function/file/line info for a memory address.

#ifndef GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_INTERFACE_H__
#define GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_INTERFACE_H__

#include <string>
#include "google_breakpad/common/breakpad_types.h"

namespace google_breakpad {

using std::string;

struct StackFrame;
struct StackFrameInfo;

class SourceLineResolverInterface {
 public:
  typedef u_int64_t MemAddr;

  virtual ~SourceLineResolverInterface() {}

  // Adds a module to this resolver, returning true on success.
  //
  // module_name may be an arbitrary string.  Typically, it will be the
  // filename of the module, optionally with version identifiers.
  //
  // map_file should contain line/address mappings for this module.
  virtual bool LoadModule(const string &module_name,
                          const string &map_file) = 0;

  // Returns true if a module with the given name has been loaded.
  virtual bool HasModule(const string &module_name) const = 0;

  // Fills in the function_base, function_name, source_file_name,
  // and source_line fields of the StackFrame.  The instruction and
  // module_name fields must already be filled in.  Additional debugging
  // information, if available, is returned.  If the information is not
  // available, returns NULL.  A NULL return value does not indicate an
  // error.  The caller takes ownership of any returned StackFrameInfo
  // object.
  virtual StackFrameInfo* FillSourceLineInfo(StackFrame *frame) const = 0;

 protected:
  // SourceLineResolverInterface cannot be instantiated except by subclasses
  SourceLineResolverInterface() {}
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_SOURCE_LINE_RESOLVER_INTERFACE_H__
