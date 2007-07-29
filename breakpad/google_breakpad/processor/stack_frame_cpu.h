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

// stack_frame_cpu.h: CPU-specific StackFrame extensions.
//
// These types extend the StackFrame structure to carry CPU-specific register
// state.  They are defined in this header instead of stack_frame.h to
// avoid the need to include minidump_format.h when only the generic
// StackFrame type is needed.
//
// Author: Mark Mentovai

#ifndef GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_CPU_H__
#define GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_CPU_H__

#include "google_breakpad/common/minidump_format.h"
#include "google_breakpad/processor/stack_frame.h"

namespace google_breakpad {

struct StackFrameX86 : public StackFrame {
  // ContextValidity has one entry for each relevant hardware pointer register
  // (%eip and %esp) and one entry for each nonvolatile (callee-save) register.
  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_EIP  = 1 << 0,
    CONTEXT_VALID_ESP  = 1 << 1,
    CONTEXT_VALID_EBP  = 1 << 2,
    CONTEXT_VALID_EBX  = 1 << 3,
    CONTEXT_VALID_ESI  = 1 << 4,
    CONTEXT_VALID_EDI  = 1 << 5,
    CONTEXT_VALID_ALL  = -1
  };

  StackFrameX86() : context(), context_validity(CONTEXT_VALID_NONE) {}

  // Register state.  This is only fully valid for the topmost frame in a
  // stack.  In other frames, the values of nonvolatile registers may be
  // present, given sufficient debugging information.  Refer to
  // context_validity.
  MDRawContextX86 context;

  // context_validity is actually ContextValidity, but int is used because
  // the OR operator doesn't work well with enumerated types.  This indicates
  // which fields in context are valid.
  int context_validity;
};

struct StackFramePPC : public StackFrame {
  // ContextValidity should eventually contain entries for the validity of
  // other nonvolatile (callee-save) registers as in
  // StackFrameX86::ContextValidity, but the ppc stackwalker doesn't currently
  // locate registers other than the ones listed here.
  enum ContextValidity {
    CONTEXT_VALID_NONE = 0,
    CONTEXT_VALID_SRR0 = 1 << 0,
    CONTEXT_VALID_GPR1 = 1 << 1,
    CONTEXT_VALID_ALL  = -1
  };

  StackFramePPC() : context(), context_validity(CONTEXT_VALID_NONE) {}

  // Register state.  This is only fully valid for the topmost frame in a
  // stack.  In other frames, the values of nonvolatile registers may be
  // present, given sufficient debugging information.  Refer to
  // context_validity.
  MDRawContextPPC context;

  // context_validity is actually ContextValidity, but int is used because
  // the OR operator doesn't work well with enumerated types.  This indicates
  // which fields in context are valid.
  int context_validity;
};

}  // namespace google_breakpad

#endif  // GOOGLE_BREAKPAD_PROCESSOR_STACK_FRAME_CPU_H__
