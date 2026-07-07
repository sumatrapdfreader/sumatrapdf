/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

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
    if (str::Eq(mtStr, "application/epub+zip")) {
        return true;
    }
    return str::Eq(mtStr, "application/x-ibooks+zip");
}

static bool IsXpsArchive(Archive* archive) {
    bool res = archive->GetFileId("_rels/.rels") >= 0 || archive->GetFileId("_rels/.rels/[0].piece") >= 0 ||
               archive->GetFileId("_rels/.rels/[0].last.piece") >= 0;
    return res;
}

static bool IsFb2Archive(Archive* archive) {
    auto files = archive->GetFileInfos();
    if (len(files) != 1) {
        return false;
    }
    auto fi = files[0];
    auto name = fi->name;
    return str::EndsWithI(name, ".fb2");
}

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

    Str d = Str((char*)buf, n);
    auto res = GuessFileTypeFromContent(d);
    if (res == kindFileZip) {
        ArchiveExtractProgressCb emptyCb;
        Archive* archive = OpenArchiveFromFile(path, /*eagerLoad=*/false, emptyCb);
        if (archive) {
            if (IsXpsArchive(archive)) {
                res = kindFileXps;
            }
            if (IsEpubArchive(archive)) {
                res = kindFileEpub;
            }
            if (IsFb2Archive(archive)) {
                res = kindFileFb2z;
            }
            delete archive;
        }
    }
    return res;
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
