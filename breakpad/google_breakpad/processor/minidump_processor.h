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

#ifndef GOOGLE_BREAKPAD_PROCESSOR_MINIDUMP_PROCESSOR_H__
#define GOOGLE_BREAKPAD_PROCESSOR_MINIDUMP_PROCESSOR_H__

#include <string>

namespace google_breakpad {

using std::string;

class Minidump;
class ProcessState;
class SourceLineResolverInterface;
class SymbolSupplier;
class SystemInfo;

class MinidumpProcessor {
 public:
  // Return type for Process()
  enum ProcessResult {
    PROCESS_OK,           // the minidump was processed successfully
    PROCESS_ERROR,        // there was an error processing the minidump
    PROCESS_INTERRUPTED   // processing was interrupted by the SymbolSupplier
  };

  // Initializes this MinidumpProcessor.  supplier should be an
  // implementation of the SymbolSupplier abstract base class.
  MinidumpProcessor(SymbolSupplier *supplier,
                    SourceLineResolverInterface *resolver);
  ~MinidumpProcessor();

  // Processes the minidump file and fills process_state with the result.
  ProcessResult Process(const string &minidump_file,
                        ProcessState *process_state);

  // Populates the cpu_* fields of the |info| parameter with textual
  // representations of the CPU type that the minidump in |dump| was
  // produced on.  Returns false if this information is not available in
  // the minidump.
  static bool GetCPUInfo(Minidump *dump, SystemInfo *info);

  // Populates the os_* fields of the |info| parameter with textual
  // representations of the operating system that the minidump in |dump|
  // was produced on.  Returns false if this information is not available in
  // the minidump.
  static bool GetOSInfo(Minidump *dump, SystemInfo *info);

  // Returns a textual representation of the reason that a crash occurred,
  // if the minidump in dump was produced as a result of a crash.  Returns
  // an empty string if this information cannot be determined.  If address
  // is non-NULL, it will be set to contain the address that caused the
  // exception, if this information is available.  This will be a code
  // address when the crash was caused by problems such as illegal
  // instructions or divisions by zero, or a data address when the crash
  // was caused by a memory access violation.
  static string GetCrashReason(Minidump *dump, u_int64_t *address);

 private:
  SymbolSupplier *supplier_;
  SourceLineResolverInterface *resolver_;
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_MINIDUMP_PROCESSOR_H__
