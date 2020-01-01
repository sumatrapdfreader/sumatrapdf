/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
#include "utils/CmdLineParser.h"
#include "utils/FileUtil.h"
#include "utils/WinUtil.h"
#include "utils/LzmaSimpleArchive.h"

namespace lzsa {

struct ISzCrtAlloc : ISzAlloc {
    static void* _Alloc(void* p, size_t size) {
        UNUSED(p);
        return malloc(size);
    }
    static void _Free(void* p, void* ptr) {
        UNUSED(p);
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
    CrashIf(*compressedSize < uncompressedSize + 1);
    if (*compressedSize < uncompressedSize + 1)
        return false;

    size_t lzma_size = (size_t)-1;

    if (*compressedSize >= LZMA_HEADER_SIZE) {
        ISzCrtAlloc lzmaAlloc;
        CLzmaEncProps props;
        LzmaEncProps_Init(&props);

        // always apply the BCJ filter for speed (else two or three compression passes would be required)
        ScopedMem<uint8_t> bcj_enc(AllocArray<uint8_t>(uncompressedSize));
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
        compressed[0] = (uint8_t)-1;
        memcpy(compressed + 1, uncompressed, uncompressedSize);
        *compressedSize = uncompressedSize + 1;
    }

    return true;
}

static bool AppendEntry(str::Str& data, str::Str& content, const WCHAR* filePath, const char* inArchiveName,
                        lzma::FileInfo* fi = nullptr) {
    size_t nameLen = str::Len(inArchiveName);
    CrashIf(nameLen > UINT32_MAX - 25);
    uint32_t headerSize = 25 + (uint32_t)nameLen;
    FILETIME ft = file::GetModificationTime(filePath);
    if (fi && FileTimeEq(ft, fi->ftModified)) {
    ReusePrevious:
        ByteWriter meta = MakeByteWriterLE(data.AppendBlanks(24), 24);
        meta.Write32(headerSize);
        meta.Write32(fi->compressedSize);
        meta.Write32(fi->uncompressedSize);
        meta.Write32(fi->uncompressedCrc32);
        meta.Write32(ft.dwLowDateTime);
        meta.Write32(ft.dwHighDateTime);
        data.Append(inArchiveName, nameLen + 1);
        return content.Append(fi->compressedData, fi->compressedSize);
    }

    AutoFree fileData(file::ReadFile(filePath));
    if (!fileData.data || fileData.size() >= UINT32_MAX) {
        fprintf(stderr, "Failed to read \"%S\" for compression\n", filePath);
        return false;
    }
    uint32_t fileDataCrc = crc32(0, (const u8*)fileData.data, (uint32_t)fileData.size());
    if (fi && fi->uncompressedCrc32 == fileDataCrc && fi->uncompressedSize == fileData.size())
        goto ReusePrevious;

    size_t compressedSize = fileData.size() + 1;
    AutoFree compressed((char*)malloc(compressedSize));
    if (!compressed)
        return false;
    if (!Compress(fileData.data, fileData.size(), compressed, &compressedSize))
        return false;

    ByteWriter meta = MakeByteWriterLE(data.AppendBlanks(24), 24);
    meta.Write32(headerSize);
    meta.Write32((uint32_t)compressedSize);
    meta.Write32((uint32_t)fileData.size());
    meta.Write32(fileDataCrc);
    meta.Write32(ft.dwLowDateTime);
    meta.Write32(ft.dwHighDateTime);
    data.Append(inArchiveName, nameLen + 1);
    return content.Append(compressed, compressedSize);
}

// creates an archive from files (starting at index skipFiles);
// file paths may be relative to the current directory or absolute and
// may end in a colon followed by the desired path in the archive
// (this is required for absolute paths)
bool CreateArchive(const WCHAR* archivePath, WStrVec& files, size_t skipFiles = 0) {
    AutoFree prevData(file::ReadFile(archivePath));
    size_t prevDataLen = prevData.size();
    lzma::SimpleArchive prevArchive;
    if (!lzma::ParseSimpleArchive(prevData.data, prevDataLen, &prevArchive))
        prevArchive.filesCount = 0;

    str::Str data;
    str::Str content;

    ByteWriter lzsaHeader = MakeByteWriterLE(data.AppendBlanks(8), 8);
    lzsaHeader.Write32(LZMA_MAGIC_ID);
    lzsaHeader.Write32((uint32_t)(files.size() - skipFiles));

    for (size_t i = skipFiles; i < files.size(); i++) {
        AutoFreeWstr filePath(str::Dup(files.at(i)));
        WCHAR* sep = str::FindCharLast(filePath, ':');
        AutoFree utf8Name;
        if (sep) {
            utf8Name = strconv::WstrToUtf8(sep + 1);
            *sep = '\0';
        } else {
            utf8Name = strconv::WstrToUtf8(filePath);
        }

        str::TransChars(utf8Name, "/", "\\");
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

    uint32_t headerCrc32 = crc32(0, (const uint8_t*)data.Get(), (uint32_t)data.size());
    MakeByteWriterLE(data.AppendBlanks(4), 4).Write32(headerCrc32);
    if (!data.Append(content.Get(), content.size()))
        return false;

    return file::WriteFile(archivePath, data.as_view());
}

} // namespace lzsa

#define FailIf(cond, msg, ...)                  \
    if (cond) {                                 \
        fprintf(stderr, msg "\n", __VA_ARGS__); \
        return errorStep;                       \
    }                                           \
    errorStep++

int mainVerify(const WCHAR* archivePath) {
    int errorStep = 1;
    AutoFree fileData(file::ReadFile(archivePath));
    FailIf(!fileData.data, "Failed to read \"%S\"", archivePath);

    lzma::SimpleArchive lzsa;
    bool ok = lzma::ParseSimpleArchive(fileData.data, fileData.size(), &lzsa);
    FailIf(!ok, "\"%S\" is no valid LzSA file", archivePath);

    for (int i = 0; i < lzsa.filesCount; i++) {
        AutoFree data(lzma::GetFileDataByIdx(&lzsa, i, nullptr));
        FailIf(!data, "Failed to extract data for \"%s\"", lzsa.files[i].name);
    }

    printf("Verified all %d archive entries\n", lzsa.filesCount);
    return 0;
}

int main(int argc, char** argv) {
    UNUSED(argc);
    UNUSED(argv);
#ifdef DEBUG
    // report memory leaks on stderr
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    WStrVec args;
    ParseCmdLine(GetCommandLine(), args);
    int errorStep = 1;

    if (args.size() == 2 && file::Exists(args.at(1)))
        return mainVerify(args.at(1));

    FailIf(args.size() < 3, "Syntax: %S <archive.lzsa> <filename>[:<in-archive name>] [...]",
           path::GetBaseNameNoFree(args.at(0)));

    bool ok = lzsa::CreateArchive(args.at(1), args, 2);
    FailIf(!ok, "Failed to create \"%S\"", args.at(1));

    return 0;
}
