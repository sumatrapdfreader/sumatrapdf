/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// the parts of file-type guessing that depend on base/Archive.h (and thus
// ext/libarchive); GuessFileType.cpp must stay free of external-library
// dependencies

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/Archive.h"

static bool IsEpubArchive(Archive* archive) {
    auto* container = archive->GetFileDataByName("META-INF/container.xml");
    if (container && container->data) {
        return true;
    }

    auto* mimeType = archive->GetFileDataByName("mimetype");
    if (!mimeType || !mimeType->data) {
        return false;
    }

    char* mt = mimeType->data;
    int n = mimeType->fileSizeUncompressed;
    for (int i = n; i > 0; i--) {
        if (!str::IsWs(mt[i - 1])) {
            n = i;
            break;
        }
        mt[i - 1] = '\0';
        if (i == 1) {
            n = 0;
        }
    }

    Str mtStr = Str(mt, n);
    if (str::Eq(mtStr, StrL("application/epub+zip"))) {
        return true;
    }
    return str::Eq(mtStr, StrL("application/x-ibooks+zip"));
}

static bool IsXpsArchive(Archive* archive) {
    bool res = archive->GetFileId("_rels/.rels") >= 0 || archive->GetFileId("_rels/.rels/[0].piece") >= 0 ||
               archive->GetFileId("_rels/.rels/[0].last.piece") >= 0;
    return res;
}

// Single .fb2 member, optionally with companion .url files (same rule as
// Fb2Doc::loadFromFile / loadFromData).
static bool IsFb2Archive(Archive* archive) {
    auto files = archive->GetFileInfos();
    int nFb2 = 0;
    for (auto* fi : files) {
        auto name = fi->name;
        if (str::EndsWithI(name, StrL(".fb2"))) {
            nFb2++;
        } else if (!str::EndsWithI(name, StrL(".url"))) {
            return false;
        }
    }
    return nFb2 == 1;
}

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

    Str d = Str((char*)buf, n);
    auto res = GuessFileTypeFromData(d);
    if (res != FileType::Zip) {
        return res;
    }
    ArchiveExtractProgressCb emptyCb;
    Archive* archive = OpenArchiveFromFile(path, /*eagerLoad=*/false, emptyCb);
    if (!archive) {
        return res;
    }
    if (IsXpsArchive(archive)) {
        res = FileType::Xps;
    }
    if (IsEpubArchive(archive)) {
        res = FileType::Epub;
    }
    if (IsFb2Archive(archive)) {
        res = FileType::Fb2z;
    }
    delete archive;
    return res;
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
