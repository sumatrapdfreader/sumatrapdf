/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Exif.h"
#include "base/File.h"

#include "Settings.h"
#include "Flags.h"
#include "ExifDump.h"

// Dump all EXIF metadata for path to stdout (exif-py compatible format).
static void DumpExifFile(Str path) {
    if (len(path) == 0) {
        return;
    }
    WriteStdoutLn(fmt("Opening: %s", path));
    Str data = file::ReadFile(path);
    AutoFree dataOwner(data.s);
    ExifParser parser;
    if (len(data) == 0 || !parser.Parse(data)) {
        WriteStdoutLn(StrL("No EXIF information found"));
        return;
    }

    if (parser.hasJpegThumbnail) {
        WriteStdoutLn(StrL("File has JPEG thumbnail"));
    }

    for (Str line : parser.dumpLines) {
        WriteStdoutLn(line);
    }
}

void DumpExif(const Flags& flags) {
    for (Str path : flags.fileNames) {
        DumpExifFile(path);
    }
    if (len(flags.fileNames) == 0) {
        WriteStdoutLn(StrL("No file specified for -dump-exif"));
    }
}
