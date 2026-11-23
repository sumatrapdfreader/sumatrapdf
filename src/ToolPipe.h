/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// sumatrapdf-tool.exe runs SumatraPDF.exe for a console:
//
//   sumatrapdf-tool.exe info a.pdf
//     creates <name>-args, <name>-out, <name>-err, <name>-in (server ends)
//     starts SumatraPDF.exe -tool-pipe <name>
//       <name>-args --> [u32 byte count][each arg as UTF-8 + '\0']
//       <name>-out  <-- SumatraPDF.exe's stdout
//       <name>-err  <-- SumatraPDF.exe's stderr
//       <name>-in   --> the tool's stdin, only when that is a file or a pipe;
//                       a console stays the child's (interactive `run`)
//     copies -out / -err to its own stdout / stderr, returns the exit code
//
// SumatraPDF.exe runs those args as its command line, as a fresh instance: no
// single-instance reuse, no session restore, no settings save. Used by both
// sides, so plain Win32 types only.

#define kToolPipeFlag L"-tool-pipe"
#define kToolPipeArgsSuffix L"-args"
#define kToolPipeOutSuffix L"-out"
#define kToolPipeErrSuffix L"-err"
#define kToolPipeInSuffix L"-in"

// cap on the args payload; a Windows command line is at most 32K chars
constexpr unsigned int kToolPipeMaxArgsBytes = 1024 * 1024;
