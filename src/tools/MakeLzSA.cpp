/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// MakeLzSA creates LzSA archives as described in utils/LzmaSimpleArchive.cpp
// Such archives use LZMA compression with an x86 bytecode filter which produces
// best results for installer payloads. See ../makefile.msvc for a use case.

#define __STDC_LIMIT_MACROS
#include "utils/BaseUtil.h"
#include <LzmaEnc.h>
#include <Bra.h>
#include <zlib.h> // for crc32
#include "utils/ByteWriter.h"
#include "utils/CmdLineArgsIter.h"
#include "utils/FileUtil.h"
#include "utils/DirIter.h"
#include "utils/WinUtil.h"
#include "utils/LzmaSimpleArchive.h"

namespace lzsa {

struct ISzCrtAlloc : ISzAlloc {
    static void* _Alloc(__unused void* p, size_t size) {
        return malloc(size);
    }
    static void _Free(__unused void* p, void* ptr) {
        free(ptr);
    }

    ISzCrtAlloc() {
        this->Alloc = _Alloc;
        this->Free = _Free;
    }
};

#define LZMA_MAGIC_ID 0x41537a4c
#define LZMA_HEADER_SIZE (1 + LZMA_PROPS_SIZE)

static bool Compress(const char* uncompressed, size_t uncompressedSize, char* compressed, size_t* compressedSize) {
    ReportIf(*compressedSize < uncompressedSize + 1);
    if (*compressedSize < uncompressedSize + 1)
        return false;

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
        if (SZ_OK == res && propsSize == LZMA_PROPS_SIZE)
            lzma_size = outSize + LZMA_HEADER_SIZE;
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

static bool AppendEntry(str::Str& data, str::Str& content, const char* filePath, const char* inArchiveName,
                        lzma::FileInfo* fi = nullptr) {
    size_t nameLen = str::Len(inArchiveName);
    ReportIf(nameLen > UINT32_MAX - 25);
    u32 headerSize = 25 + (u32)nameLen;
    FILETIME ft = file::GetModificationTime(filePath);

    constexpr size_t kBufSize = 24;

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
        data.AppendSlice(meta.AsByteSlice());
        data.Append(inArchiveName, nameLen + 1);
        return content.Append(fi->compressedData, fi->compressedSize);
    }

    ByteSlice fileData = file::ReadFile(filePath);
    if (!fileData.data() || fileData.size() >= UINT32_MAX) {
        fprintf(stderr, "Failed to read \"%s\" for compression\n", filePath);
        return false;
    }
    u32 fileDataCrc = crc32(0, (const u8*)fileData.data(), (u32)fileData.size());
    if (fi && fi->uncompressedCrc32 == fileDataCrc && fi->uncompressedSize == fileData.size())
        goto ReusePrevious;

    size_t compressedSize = fileData.size() + 1;
    AutoFree compressed((char*)malloc(compressedSize));
    if (!compressed.Get()) {
        return false;
    }
    if (!Compress((const char*)fileData.data(), fileData.size(), compressed, &compressedSize)) {
        return false;
    }

    ByteWriterLE meta(kBufSize);
    meta.Write32(headerSize);
    meta.Write32((u32)compressedSize);
    meta.Write32((u32)fileData.size());
    meta.Write32(fileDataCrc);
    meta.Write32(ft.dwLowDateTime);
    meta.Write32(ft.dwHighDateTime);
    ReportIf(meta.Size() != kBufSize);
    data.AppendSlice(meta.AsByteSlice());
    data.Append(inArchiveName, nameLen + 1);
    return content.Append(compressed, compressedSize);
}

// creates an archive from files (starting at index skipFiles);
// file paths may be relative to the current directory or absolute and
// may end in a colon followed by the desired path in the archive
// (this is required for absolute paths)
bool CreateArchive(const char* archivePath, StrVec& files, size_t skipFiles = 0) {
    ByteSlice prevData(file::ReadFile(archivePath));
    size_t prevDataLen = prevData.size();
    lzma::SimpleArchive prevArchive;
    if (!lzma::ParseSimpleArchive((const u8*)prevData.data(), prevDataLen, &prevArchive)) {
        prevArchive.filesCount = 0;
    }

    str::Str data;
    str::Str content;

    constexpr size_t kBufSize = 8;
    ByteWriterLE lzsaHeader(kBufSize);
    lzsaHeader.Write32(LZMA_MAGIC_ID);
    lzsaHeader.Write32((u32)(files.Size() - skipFiles));
    ReportIf(lzsaHeader.Size() != kBufSize);
    data.AppendSlice(lzsaHeader.AsByteSlice());

    for (int i = skipFiles; i < files.Size(); i++) {
        AutoFreeStr filePath(str::Dup(files.at(i)));
        char* sep = str::FindCharLast(filePath, ':');
        AutoFreeStr utf8Name;
        if (sep) {
            utf8Name = str::Dup(sep + 1);
            *sep = '\0';
        } else {
            utf8Name = str::Dup(filePath);
        }

        str::TransCharsInPlace(utf8Name, "/", "\\");
        if ('/' == *utf8Name || str::Find(utf8Name, "../")) {
            fprintf(stderr, "In-archive name must not be an absolute path: %s\n", utf8Name.Get());
            return false;
        }

        int idx = GetIdxFromName(&prevArchive, utf8Name);
        lzma::FileInfo* fi = nullptr;
        if (idx != -1)
            fi = &prevArchive.files[idx];
        if (!AppendEntry(data, content, filePath, utf8Name, fi))
            return false;
    }

    u32 headerCrc32 = crc32(0, (const u8*)data.Get(), (u32)data.size());
    ByteWriterLE buf(4);
    buf.Write32(headerCrc32);
    ReportIf(buf.Size() != 4);
    data.AppendSlice(buf.AsByteSlice());
    if (!data.Append(content.Get(), content.size()))
        return false;

    ByteSlice d = data.AsByteSlice();
    return file::WriteFile(archivePath, d);
}

bool CreateArchiveFromDir(const char* archivePath, const char* dir) {
    StrVec files;
    int n = str::Len(dir);
    bool ok = DirTraverse(dir, true, [&files, n](const char* path) -> bool {
        const char* archiveName = path + n;
        if ('\\' == *archiveName)
            archiveName++;
        if ('/' == *archiveName)
            archiveName++;
        TempStr s = str::JoinTemp(path, ":", archiveName);
        files.Append(s);
        return true;
    });
    if (!ok) {
        return false;
    }
    return CreateArchive(archivePath, files, 0);
}

} // namespace lzsa

void _uploadDebugReportIfFunc(bool, const char*) {
    // no-op implementation to satisfy SubmitBugReport()
}

#define FailIf(cond, msg, ...)                  \
    if (cond) {                                 \
        fprintf(stderr, msg "\n", __VA_ARGS__); \
        return errorStep;                       \
    }

static void MyParseCmdLine(const WCHAR* cmdLine, StrVec& args) {
    int nArgs = 0;
    WCHAR** argsArr = CommandLineToArgvW(cmdLine, &nArgs);
    for (int i = 0; i < nArgs; i++) {
        char* arg = ToUtf8Temp(argsArr[i]);
        args.Append(arg);
    }
    LocalFree(argsArr);
}

int mainVerify(const char* archivePath) {
    int errorStep = 1;
    ByteSlice fileData = file::ReadFile(archivePath);
    FailIf(!fileData.data(), "Failed to read \"%s\"", archivePath);
    errorStep++;

    lzma::SimpleArchive lzsa;
    bool ok = lzma::ParseSimpleArchive((const u8*)fileData.data(), fileData.size(), &lzsa);
    FailIf(!ok, "\"%s\" is no valid LzSA file", archivePath);
    errorStep++;

    for (int i = 0; i < lzsa.filesCount; i++) {
        AutoFree data(lzma::GetFileDataByIdx(&lzsa, i, nullptr));
        FailIf(!data, "Failed to extract data for \"%s\"", lzsa.files[i].name);
        errorStep++;
    }

    printf("Verified all %d archive entries\n", lzsa.filesCount);
    return 0;
}

int printUsage(const char* exeName) {
    int errorStep = 0;
    FailIf(true,
           "Usage:\n  %s <archive.lzsa>\n    verify archive\n  %s <archive.lzsa> <filename>[:<in-archive name>] "
           "[...]\n    "
           "create archive from files\n  %s <archive.lzsa> <dir>\n    create archive from directory",
           exeName, exeName, exeName);
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

    auto exeName = path::GetBaseNameTemp(args.at(0));

    int nArgs = args.Size();
    // first arg is exe path, the rest is
    if (nArgs < 2) {
        return printUsage(exeName);
    }

    char* archiveName = args.at(1);
    if (nArgs == 2 && file::Exists(archiveName)) {
        return mainVerify(archiveName);
    }

    if (nArgs == 3) {
        auto dir = args.at(2);
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
    FailIf(!ok, "Failed to create \"%s\"", args.at(1));

    return 0;
}
