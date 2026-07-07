/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/Exif.h"
#include "base/File.h"
#include "base/Win.h"

#include "Settings.h"
#include "Flags.h"

// GUI-subsystem exes lose CRT stdout when spawned with a pipe (issue #5677).
static void CliWrite(Str s, int n = 0) {
    if (!s) {
        return;
    }
    if (n == 0) {
        n = s.len;
    }
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h && h != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(h, s.s, (DWORD)n, &written, nullptr);
        return;
    }
    fwrite(s.s, 1, (size_t)n, stdout);
}

static void CliPrint(Str s) {
    CliWrite(s);
    CliWrite(StrL("\n"), 1);
}

bool DumpExifFile(Str path) {
    if (!path) {
        return false;
    }
    CliPrint(fmt("Opening: %s", path));
    Str data = file::ReadFile(path);
    if (len(data) == 0) {
        CliPrint(StrL("No EXIF information found"));
        return false;
    }

    ExifParser parser;
    bool found = parser.Parse(data);
    if (!found) {
        CliPrint(StrL("No EXIF information found"));
        str::Free(data);
        return false;
    }

    if (parser.hasJpegThumbnail) {
        CliPrint(StrL("File has JPEG thumbnail"));
    }

    for (Str line : parser.dumpLines) {
        CliPrint(line);
    }

    str::Free(data);
    return true;
}

void DumpExif(const Flags& flags) {
    bool any = false;
    for (int i = 0; i < len(flags.fileNames); i++) {
        if (DumpExifFile(flags.fileNames[i])) {
            any = true;
        }
    }
    if (!any && len(flags.fileNames) == 0) {
        CliPrint("No file specified for -dump-exif");
    }
}
