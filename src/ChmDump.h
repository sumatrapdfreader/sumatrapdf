/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Flags;

// Dump CHM metadata, file table, and TOC/index information to stdout.
// Returns 0 if every requested CHM opened, enumerated, and unpacked successfully.
int DumpChm(const Flags& flags);
