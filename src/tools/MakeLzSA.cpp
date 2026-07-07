/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// MakeLzSA creates LzSA archives as described in utils/LzmaSimpleArchive.cpp
// Such archives use LZMA compression with an x86 bytecode filter which produces
// best results for installer payloads. See ../makefile.msvc for a use case.

#define __STDC_LIMIT_MACROS
#include "base/Base.h"
#include <LzmaEnc.h>
#include <Bra.h>
#include <zlib.h> // for crc32
#include "base/ByteWriter.h"
#include "base/CmdLineArgsIter.h"
#include "base/File.h"
#include "base/DirIter.h"
#include "base/Win.h"
#include "base/LzmaSimpleArchive.h"

namespace lzsa {

struct ISzCrtAlloc : ISzAlloc {
    static void* _Alloc(__unused void* p, size_t size) { return malloc(size); }
    static void _Free(__unused void* p, void* ptr) { free(ptr); }

    ISzCrtAlloc() {
        this->Alloc = _Alloc;
        this->Free = _Free;
    }
};

#define LZMA_MAGIC_ID 0x41537a4c
#define LZMA_HEADER_SIZE (1 + LZMA_PROPS_SIZE)

static bool Compress(const char* uncompressed, size_t uncompressedSize, char* compressed, size_t* compressedSize) {
    ReportIf(*compressedSize < uncompressedSize + 1);
    if (*compressedSize < uncompressedSize + 1) return false;

    size_t lzma_size = (size_t)-1;

    if (*compressedSize >= LZMA_HEADER_SIZE) {
        ISzCrtAlloc lzmaAlloc;
        CLzmaEncProps props;
        LzmaEncProps_Init(&props);

        // always apply the BCJ filter for speed (else two or three compression passes would be required)
        ScopedMem<u8> bcj_enc(AllocArray<u8>(uncompressedSize));
        if (bcj_enc) {
            memcpy(bcj_enc, uncompressed, uncompressedSize);
            UInt32 x86State;
            x86_Convert_Init(x86State);
            x86_Convert(bcj_enc, uncompressedSize, 0, &x86State, 1);
        }
        compressed[0] = bcj_enc ? 1 : 0;

        SizeT outSize = *compressedSize - LZMA_HEADER_SIZE;
        SizeT propsSize = LZMA_PROPS_SIZE;
        SRes res =
            LzmaEncode((Byte*)compressed + LZMA_HEADER_SIZE, &outSize, bcj_enc ? bcj_enc : (const Byte*)uncompressed,
                       uncompressedSize, &props, (Byte*)compressed + 1, &propsSize, TRUE /* add EOS marker */, nullptr,
                       &lzmaAlloc, &lzmaAlloc);
        if (SZ_OK == res && propsSize == LZMA_PROPS_SIZE) lzma_size = outSize + LZMA_HEADER_SIZE;
    }

    if (lzma_size <= uncompressedSize) {
        *compressedSize = lzma_size;
    } else {
        compressed[0] = (u8)-1;
        memcpy(compressed + 1, uncompressed, uncompressedSize);
        *compressedSize = uncompressedSize + 1;
    }

    return true;
}

static bool AppendEntry(str::Builder& data, str::Builder& content, Str filePath, Str inArchiveName,
                        lzma::FileInfo* fi = nullptr) {
    size_t nameLen = (size_t)inArchiveName.len;
    ReportIf(nameLen > UINT32_MAX - 25);
    u32 headerSize = 25 + (u32)nameLen;
    FILETIME ft = file::GetModificationTime(filePath);

    constexpr int kBufSize = 24;

    if (fi && FileTimeEq(ft, fi->ftModified)) {
    ReusePrevious:
        ByteWriterLE meta(kBufSize);
        meta.Write32(headerSize);
        meta.Write32(fi->compressedSize);
        meta.Write32(fi->uncompressedSize);
        meta.Write32(fi->uncompressedCrc32);
        meta.Write32(ft.dwLowDateTime);
        meta.Write32(ft.dwHighDateTime);
        ReportIf(meta.Size() != kBufSize);
        data.Append(meta.AsByteSlice());
        data.Append(inArchiveName);
        data.AppendChar('\0');
        return content.Append(Str((char*)fi->compressedData, (int)fi->compressedSize));
    }

    Str fileData = file::ReadFile(filePath);
    if (!(u8*)fileData.s || (size_t)fileData.len >= UINT32_MAX) {
        fprintf(stderr, "Failed to read \"%s\" for compression\n", filePath.s);
        return false;
    }
    u32 fileDataCrc = crc32(0, (const u8*)fileData.s, (u32)fileData.len);
    if (fi && fi->uncompressedCrc32 == fileDataCrc && fi->uncompressedSize == (size_t)fileData.len) goto ReusePrevious;

    size_t compressedSize = (size_t)fileData.len + 1;
    char* compressed = (char*)malloc(compressedSize);
    AutoCall freeCompressed(free, (void*)compressed);
    if (!compressed) {
        return false;
    }
    if (!Compress((const char*)fileData.s, (size_t)fileData.len, compressed, &compressedSize)) {
        return false;
    }

    ByteWriterLE meta(kBufSize);
    meta.Write32(headerSize);
    meta.Write32((u32)compressedSize);
    meta.Write32((u32)fileData.len);
    meta.Write32(fileDataCrc);
    meta.Write32(ft.dwLowDateTime);
    meta.Write32(ft.dwHighDateTime);
    ReportIf(meta.Size() != kBufSize);
    data.Append(meta.AsByteSlice());
    data.Append(inArchiveName);
    data.AppendChar('\0');
    return content.Append(Str(compressed.Get(), (int)compressedSize));
}

// creates an archive from files (starting at index skipFiles);
// file paths may be relative to the current directory or absolute and
// may end in a colon followed by the desired path in the archive
// (this is required for absolute paths)
bool CreateArchive(Str archivePath, StrVec& files, size_t skipFiles = 0) {
    Str prevData = file::ReadFile(archivePath);
    lzma::SimpleArchive prevArchive;
    if (!lzma::ParseSimpleArchive((const u8*)prevData.s, prevData.len, &prevArchive)) {
        prevArchive.filesCount = 0;
    }

    str::Builder data;
    str::Builder content;

    constexpr int kBufSize = 8;
    ByteWriterLE lzsaHeader(kBufSize);
    lzsaHeader.Write32(LZMA_MAGIC_ID);
    lzsaHeader.Write32((u32)(files.Size() - skipFiles));
    ReportIf(lzsaHeader.Size() != kBufSize);
    data.Append(lzsaHeader.AsByteSlice());

    for (int i = skipFiles; i < files.Size(); i++) {
        TempStr filePath = str::DupTemp(files[i]);
        Str sep = str::SliceFromCharLast(filePath, ':');
        TempStr utf8Name;
        if (sep) {
            utf8Name = str::DupTemp(Str(sep.s + 1));
            *sep.s = '\0';
            filePath.len = (int)(sep.s - filePath.s);
        } else {
            utf8Name = str::DupTemp(filePath);
        }

        str::TransCharsInPlace(utf8Name, StrL("/"), StrL("\\"));
        if ('/' == utf8Name.s[0] || str::Contains(utf8Name, StrL("../"))) {
            fprintf(stderr, "In-archive name must not be an absolute path: %s\n", utf8Name.s);
            return false;
        }

        int idx = GetIdxFromName(&prevArchive, utf8Name);
        lzma::FileInfo* fi = nullptr;
        if (idx != -1) fi = &prevArchive.files[idx];
        if (!AppendEntry(data, content, filePath, utf8Name, fi)) return false;
    }

    Str hdr = ToStr(data);
    u32 headerCrc32 = crc32(0, (const u8*)hdr.s, (u32)hdr.len);
    ByteWriterLE buf(4);
    buf.Write32(headerCrc32);
    ReportIf(buf.Size() != 4);
    data.Append(buf.AsByteSlice());
    if (!data.Append(ToStr(content))) return false;

    Str d = ToStr(data);
    return file::WriteFile(archivePath, d);
}

bool CreateArchiveFromDir(Str archivePath, Str dir) {
    StrVec files;
    int n = dir.len;
    DirIter di{dir};
    di.recurse = true;
    di.includeFiles = true;
    di.includeDirs = false;
    for (DirIterEntry* de : di) {
        Str path = de->filePath;
        Str archiveName = Str(path.s + n, path.len - n);
        if (archiveName && (archiveName.s[0] == '\\' || archiveName.s[0] == '/')) {
            archiveName = Str(archiveName.s + 1, archiveName.len - 1);
        }
        TempStr s = str::JoinTemp(path, StrL(":"), archiveName);
        files.Append(s);
    }
    return CreateArchive(archivePath, files, 0);
}

} // namespace lzsa

#define FailIf(cond, msg, ...)                  \
    if (cond) {                                 \
        fprintf(stderr, msg "\n", __VA_ARGS__); \
        return errorStep;                       \
    }

static void MyParseCmdLine(WStr cmdLine, StrVec& args) {
    int nArgs = 0;
    WCHAR** argsArr = CommandLineToArgvW(cmdLine.s, &nArgs);
    for (int i = 0; i < nArgs; i++) {
        Str arg = ToUtf8Temp(argsArr[i]);
        args.Append(arg);
    }
    LocalFree(argsArr);
}

int mainVerify(Str archivePath) {
    int errorStep = 1;
    Str fileData = file::ReadFile(archivePath);
    FailIf(!(u8*)fileData.s, "Failed to read \"%s\"", archivePath.s);
    errorStep++;

    lzma::SimpleArchive lzsa;
    bool ok = lzma::ParseSimpleArchive((const u8*)fileData.s, fileData.len, &lzsa);
    FailIf(!ok, "\"%s\" is no valid LzSA file", archivePath.s);
    errorStep++;

    for (int i = 0; i < lzsa.filesCount; i++) {
        auto data = lzma::GetFileDataByIdx(&lzsa, i, nullptr);
        AutoCall freeData(free, (void*)data);
        FailIf(!data, "Failed to extract data for \"%s\"", lzsa.files[i].name.s);
        errorStep++;
    }

    printf("Verified all %d archive entries\n", lzsa.filesCount);
    return 0;
}

int printUsage(Str exeName) {
    int errorStep = 0;
    FailIf(true,
           "Usage:\n  %s <archive.lzsa>\n    verify archive\n  %s <archive.lzsa> <filename>[:<in-archive name>] "
           "[...]\n    "
           "create archive from files\n  %s <archive.lzsa> <dir>\n    create archive from directory",
           exeName.s, exeName.s, exeName.s);
}

int main(__unused int argc, __unused char** argv) {
#ifdef DEBUG
    // report memory leaks on stderr
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    StrVec args;
    MyParseCmdLine(GetCommandLine(), args);
    int errorStep = 1;

    auto exeName = path::GetBaseNameTemp(args[0]);

    int nArgs = args.Size();
    // first arg is exe path, the rest is
    if (nArgs < 2) {
        return printUsage(exeName);
    }

    Str archiveName = args[1];
    if (nArgs == 2 && file::Exists(archiveName)) {
        return mainVerify(archiveName);
    }

    if (nArgs == 3) {
        auto dir = args[2];
        if (dir::Exists(dir)) {
            bool ok = lzsa::CreateArchiveFromDir(archiveName, dir);
            if (!ok) {
                return printUsage(exeName);
            }
            return 0;
        }
    }

    if (nArgs < 3) {
        return printUsage(exeName);
    }
    errorStep++;

    bool ok = lzsa::CreateArchive(archiveName, args, 2);
    FailIf(!ok, "Failed to create \"%s\"", args[1].s);

    return 0;
}
