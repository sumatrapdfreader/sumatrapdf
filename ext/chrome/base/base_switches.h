// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "base" command-line switches.

#ifndef BASE_BASE_SWITCHES_H_
#define BASE_BASE_SWITCHES_H_
#pragma once

namespace switches {

extern const char kDebugOnStart[];
extern const char kDisableBreakpad[];
extern const char kEnableDCHECK[];
extern const char kFullMemoryCrashReport[];
extern const char kNoErrorDialogs[];
extern const char kNoMessageBox[];
extern const char kTestChildProcess[];
extern const char kV[];
extern const char kVModule[];
extern const char kWaitForDebugger[];

}  // namespace switches

#endif  // BASE_BASE_SWITCHES_H_
