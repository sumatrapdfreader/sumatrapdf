// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"

namespace switches {

// If the program includes base/debug/debug_on_start_win.h, the process will
// (on Windows only) start the JIT system-registered debugger on itself and
// will wait for 60 seconds for the debugger to attach to itself. Then a break
// point will be hit.
const char kDebugOnStart[]                  = "debug-on-start";

// Disables the crash reporting.
const char kDisableBreakpad[]               = "disable-breakpad";

// Enable DCHECKs in release mode.
const char kEnableDCHECK[]                  = "enable-dcheck";

// Generates full memory crash dump.
const char kFullMemoryCrashReport[]         = "full-memory-crash-report";

// Suppresses all error dialogs when present.
const char kNoErrorDialogs[]                = "noerrdialogs";

// Disable ui::MessageBox.  This is useful when running as part of scripts that
// do not have a user interface.
const char kNoMessageBox[]                  = "no-message-box";

// When running certain tests that spawn child processes, this switch indicates
// to the test framework that the current process is a child process.
const char kTestChildProcess[]              = "test-child-process";

// Gives the default maximal active V-logging level; 0 is the default.
// Normally positive values are used for V-logging levels.
const char kV[]                             = "v";

// Gives the per-module maximal V-logging levels to override the value
// given by --v.  E.g. "my_module=2,foo*=3" would change the logging
// level for all code in source files "my_module.*" and "foo*.*"
// ("-inl" suffixes are also disregarded for this matching).
//
// Any pattern containing a forward or backward slash will be tested
// against the whole pathname and not just the module.  E.g.,
// "*/foo/bar/*=2" would change the logging level for all code in
// source files under a "foo/bar" directory.
const char kVModule[]                       = "vmodule";

// Will wait for 60 seconds for a debugger to come to attach to the process.
const char kWaitForDebugger[]               = "wait-for-debugger";

}  // namespace switches
