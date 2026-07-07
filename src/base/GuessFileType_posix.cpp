/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

Kind GuessFileTypeFromFile(Str path) {
    ReportIf(!path);
    if (path::IsDirectory(path)) {
        TempStr mimetypePath = path::JoinTemp(path, StrL("mimetype"));
        if (file::StartsWith(mimetypePath, "application/epub+zip")) {
            return kindFileEpub;
        }
        return nullptr;
    }

    char buf[2048 + 1]{};
    int n = file::ReadN(path, (u8*)buf, dimof(buf) - 1);
    if (n <= 0) {
        return nullptr;
    }

    return GuessFileTypeFromContent(Str(buf, n));
}

Kind GuessFileType(Str path, bool sniff) {
    if (sniff) {
        Kind kind = GuessFileTypeFromFile(path);
        if (kind) {
            return kind;
        }
        return GuessFileTypeFromName(path);
    }
    return GuessFileTypeFromName(path);
}
