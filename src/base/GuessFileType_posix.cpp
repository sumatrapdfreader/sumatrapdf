/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"

FileType GuessFileTypeFromFile(Str path) {
    ReportIf(!path);
    if (path::IsDirectory(path)) {
        TempStr mimetypePath = path::JoinTemp(path, StrL("mimetype"));
        if (file::StartsWith(mimetypePath, "application/epub+zip")) {
            return FileType::Epub;
        }
        return FileType::Unknown;
    }

    char buf[2048 + 1]{};
    int n = file::ReadN(path, (u8*)buf, dimof(buf) - 1);
    if (n <= 0) {
        return FileType::Unknown;
    }

    return GuessFileTypeFromContent(Str(buf, n));
}

FileType GuessFileType(Str path, bool sniff) {
    if (sniff) {
        FileType ft = GuessFileTypeFromFile(path);
        if (ft != FileType::Unknown) {
            return ft;
        }
        return GuessFileTypeFromName(path);
    }
    return GuessFileTypeFromName(path);
}
