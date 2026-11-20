/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: hand the command line to an already running instance off Windows (where
// orig's WM_COPYDATA / DDE do it). ReuseInstance_posix.cpp.

#if !OS_WIN && !OS_WASM
// true when a running instance took the command line and this process is done
bool ForwardToRunningInstance(Flags& flags, bool* otherInstanceRunning);
// lets later processes forward to this one
void StartInstanceServer();
void StopInstanceServer();
#endif
