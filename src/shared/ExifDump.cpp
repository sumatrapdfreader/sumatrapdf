/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Exif.h"
#include "base/File.h"

#include "Settings.h"
#include "Flags.h"
#include "ExifDump.h"

static void CliPrint(Str s) {
    WriteStdout(s);
    WriteStdout(StrL("\n"));
}

// Dump all EXIF metadata for path to stdout (exif-py compatible format).
static void DumpExifFile(Str path) {
    if (len(path) == 0) {
        return;
    }
    CliPrint(fmt("Opening: %s", path));
    Str data = file::ReadFile(path);
    AutoFree dataOwner(data.s);
    if (len(data) == 0) {
        CliPrint(StrL("No EXIF information found"));
        return;
    }

    ExifParser parser;
    if (!parser.Parse(data)) {
        CliPrint(StrL("No EXIF information found"));
        return;
    }

    if (parser.hasJpegThumbnail) {
        CliPrint(StrL("File has JPEG thumbnail"));
    }

    for (Str line : parser.dumpLines) {
        CliPrint(line);
    }
}

void DumpExif(const Flags& flags) {
    for (Str path : flags.fileNames) {
        DumpExifFile(path);
    }
    if (len(flags.fileNames) == 0) {
        CliPrint(StrL("No file specified for -dump-exif"));
    }
}
