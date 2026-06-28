/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Flags;

// Dump all EXIF metadata for path to stdout (exif-py compatible format).
// Returns true if any EXIF was found.
bool DumpExifFile(Str path);

void DumpExif(const Flags& flags);