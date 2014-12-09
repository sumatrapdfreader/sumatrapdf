/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// MakeLzSA creates LzSA archives as described in utils/LzmaSimpleArchive.cpp
// Such archives use LZMA compression with an x86 bytecode filter which produces
// best results for installer payloads. See ../makefile.msvc for a use case.

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include <LzmaEnc.h>
#include <Bra.h>
#include <zlib.h> // for crc32
#include "ByteWriter.h"
#include "CmdLineParser.h"
#include "FileUtil.h"
#include "LzmaSimpleArchive.h"

namespace lzsa {

struct ISzCrtAlloc : ISzAlloc {
    static void *_Alloc(void *p, size_t size) { return malloc(size); }
    static void _Free(void *p, void *ptr) { free(ptr); }

    ISzCrtAlloc() { this->Alloc = _Alloc; this->Free = _Free; }
};

#define LZMA_MAGIC_ID 0x41537a4c
#define LZMA_HEADER_SIZE (1 + LZMA_PROPS_SIZE)

static bool Compress(const char *uncompressed, size_t uncompressedSize, char *compressed, size_t *compressedSize)
{
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
        SRes res = LzmaEncode((Byte *)compressed + LZMA_HEADER_SIZE, &outSize,
                              bcj_enc ? bcj_enc : (const Byte *)uncompressed, uncompressedSize,
                              &props, (Byte *)compressed + 1, &propsSize,
                              TRUE /* add EOS marker */, nullptr, &lzmaAlloc, &lzmaAlloc);
        if (SZ_OK == res && propsSize == LZMA_PROPS_SIZE)
            lzma_size = outSize + LZMA_HEADER_SIZE;
    }

    if (lzma_size <= uncompressedSize) {
        *compressedSize = lzma_size;
    }
    else {
        compressed[0] = (uint8_t)-1;
        memcpy(compressed + 1, uncompressed, uncompressedSize);
        *compressedSize = uncompressedSize + 1;
    }

    return true;
}

static bool AppendEntry(str::Str<char>& data, str::Str<char>& content, const WCHAR *filePath, const char *inArchiveName, lzma::FileInfo *fi=nullptr)
{
    size_t nameLen = str::Len(inArchiveName);
    CrashIf(nameLen > UINT32_MAX - 25);
    uint32_t headerSize = 25 + (uint32_t)nameLen;
    FILETIME ft = file::GetModificationTime(filePath);
    if (fi && FileTimeEq(ft, fi->ftModified)) {
ReusePrevious:
        ByteWriterLE meta(data.AppendBlanks(24), 24);
        meta.Write32(headerSize);
        meta.Write32(fi->compressedSize);
        meta.Write32(fi->uncompressedSize);
        meta.Write32(fi->uncompressedCrc32);
        meta.Write32(ft.dwLowDateTime);
        meta.Write32(ft.dwHighDateTime);
        data.Append(inArchiveName, nameLen + 1);
        return content.AppendChecked(fi->compressedData, fi->compressedSize);
    }

    size_t fileDataLen;
    ScopedMem<char> fileData(file::ReadAll(filePath, &fileDataLen));
    if (!fileData || fileDataLen >= UINT32_MAX) {
        fprintf(stderr, "Failed to read \"%S\" for compression\n", filePath);
        return false;
    }
    uint32_t fileDataCrc = crc32(0, (const uint8_t *)fileData.Get(), (uint32_t)fileDataLen);
    if (fi && fi->uncompressedCrc32 == fileDataCrc && fi->uncompressedSize == fileDataLen)
        goto ReusePrevious;

    size_t compressedSize = fileDataLen + 1;
    ScopedMem<char> compressed((char *)malloc(compressedSize));
    if (!compressed)
        return false;
    if (!Compress(fileData, fileDataLen, compressed, &compressedSize))
        return false;

    ByteWriterLE meta(data.AppendBlanks(24), 24);
    meta.Write32(headerSize);
    meta.Write32((uint32_t)compressedSize);
    meta.Write32((uint32_t)fileDataLen);
    meta.Write32(fileDataCrc);
    meta.Write32(ft.dwLowDateTime);
    meta.Write32(ft.dwHighDateTime);
    data.Append(inArchiveName, nameLen + 1);
    return content.AppendChecked(compressed, compressedSize);
}

// creates an archive from files (starting at index skipFiles);
// file paths may be relative to the current directory or absolute and
// may end in a colon followed by the desired path in the archive
// (this is required for absolute paths)
bool CreateArchive(const WCHAR *archivePath, WStrVec& files, size_t skipFiles=0)
{
    size_t prevDataLen = 0;
    ScopedMem<char> prevData(file::ReadAll(archivePath, &prevDataLen));
    lzma::SimpleArchive prevArchive;
    if (!lzma::ParseSimpleArchive(prevData, prevDataLen, &prevArchive))
        prevArchive.filesCount = 0;

    str::Str<char> data;
    str::Str<char> content;

    ByteWriterLE lzsaHeader(data.AppendBlanks(8), 8);
    lzsaHeader.Write32(LZMA_MAGIC_ID);
    lzsaHeader.Write32((uint32_t)(files.Count() - skipFiles));

    for (size_t i = skipFiles; i < files.Count(); i++) {
        ScopedMem<WCHAR> filePath(str::Dup(files.At(i)));
        WCHAR *sep = str::FindCharLast(filePath, ':');
        ScopedMem<char> utf8Name;
        if (sep) {
            utf8Name.Set(str::conv::ToUtf8(sep + 1));
            *sep = '\0';
        }
        else {
            utf8Name.Set(str::conv::ToUtf8(filePath));
        }

        str::TransChars(utf8Name, "/", "\\");
        if ('/' == *utf8Name || str::Find(utf8Name, "../")) {
            fprintf(stderr, "In-archive name must not be an absolute path: %s\n", utf8Name.Get());
            return false;
        }

        int idx = GetIdxFromName(&prevArchive, utf8Name);
        lzma::FileInfo *fi = nullptr;
        if (idx != -1)
            fi = &prevArchive.files[idx];
        if (!AppendEntry(data, content, filePath, utf8Name, fi))
            return false;
    }

    uint32_t headerCrc32 = crc32(0, (const uint8_t *)data.Get(), (uint32_t)data.Size());
    ByteWriterLE(data.AppendBlanks(4), 4).Write32(headerCrc32);
    if (!data.AppendChecked(content.Get(), content.Size()))
        return false;

    return file::WriteAll(archivePath, data.Get(), data.Size());
}

}

#define FailIf(cond, msg, ...) if (cond) { fprintf(stderr, msg "\n", __VA_ARGS__); return errorStep; } errorStep++

int mainVerify(const WCHAR *archivePath)
{
    int errorStep = 1;
    size_t fileDataLen;
    ScopedMem<char> fileData(file::ReadAll(archivePath, &fileDataLen));
    FailIf(!fileData, "Failed to read \"%S\"", archivePath);

    lzma::SimpleArchive lzsa;
    bool ok = lzma::ParseSimpleArchive(fileData, fileDataLen, &lzsa);
    FailIf(!ok, "\"%S\" is no valid LzSA file", archivePath);

    for (int i = 0; i < lzsa.filesCount; i++) {
        ScopedMem<char> data(lzma::GetFileDataByIdx(&lzsa, i, nullptr));
        FailIf(!data, "Failed to extract data for \"%s\"", lzsa.files[i].name);
    }

    printf("Verified all %d archive entries\n", lzsa.filesCount);
    return 0;
}

int main(int argc, char **argv)
{
#ifdef DEBUG
    // report memory leaks on stderr
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    WStrVec args;
    ParseCmdLine(GetCommandLine(), args);
    int errorStep = 1;

    if (args.Count() == 2 && file::Exists(args.At(1)))
        return mainVerify(args.At(1));

    FailIf(args.Count() < 3, "Syntax: %S <archive.lzsa> <filename>[:<in-archive name>] [...]", path::GetBaseName(args.At(0)));

    bool ok = lzsa::CreateArchive(args.At(1), args, 2);
    FailIf(!ok, "Failed to create \"%S\"", args.At(1));

    return 0;
}
