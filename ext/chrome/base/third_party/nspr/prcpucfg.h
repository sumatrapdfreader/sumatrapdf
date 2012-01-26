// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
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

#ifndef BASE_THIRD_PARTY_NSPR_PRCPUCFG_H__
#define BASE_THIRD_PARTY_NSPR_PRCPUCFG_H__

#if defined(WIN32)
#include "base/third_party/nspr/prcpucfg_win.h"
#elif defined(__APPLE__)
#include "base/third_party/nspr/prcpucfg_mac.h"
#elif defined(__native_client__)
#include "base/third_party/nspr/prcpucfg_nacl.h"
#elif defined(__linux__) || defined(ANDROID)
#include "base/third_party/nspr/prcpucfg_linux.h"
#elif defined(__FreeBSD__)
#include "base/third_party/nspr/prcpucfg_freebsd.h"
#elif defined(__OpenBSD__)
#include "base/third_party/nspr/prcpucfg_openbsd.h"
#elif defined(__sun)
#include "base/third_party/nspr/prcpucfg_solaris.h"
#else
#error Provide a prcpucfg.h appropriate for your platform
#endif

#endif  // BASE_THIRD_PARTY_NSPR_PRCPUCFG_H__
