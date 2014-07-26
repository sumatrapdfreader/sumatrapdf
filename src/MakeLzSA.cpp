/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// MakeLzSA creates LzSA archives as described in utils/LzmaSimpleArchive.cpp
// Such archives use LZMA compression with an x86 bytecode filter which produces
// best results for installer payloads. See ../makefile.msvc for a use case.

#define __STDC_LIMIT_MACROS
#include "BaseUtil.h"
#include "ByteWriter.h"
#include "CmdLineParser.h"
#include "FileUtil.h"
#include "LzmaSimpleArchive.h"

#include <LzmaEnc.h>
#include <Bra.h>
#include <zlib.h> // for crc32

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
    ISzCrtAlloc lzmaAlloc;

    if (*compressedSize < uncompressedSize + 1)
        return false;
    if (*compressedSize < LZMA_HEADER_SIZE)
        goto StoreUncompressed;

    CLzmaEncProps props;
    LzmaEncProps_Init(&props);

    // always apply the BJC filter for speed (else two or three compression passes would be required)
    size_t lzma_size = (size_t)-1;
    uint8_t *bjc_enc = (uint8_t *)malloc(uncompressedSize);
    if (bjc_enc) {
        memcpy(bjc_enc, uncompressed, uncompressedSize);
        UInt32 x86State;
        x86_Convert_Init(x86State);
        x86_Convert(bjc_enc, uncompressedSize, 0, &x86State, 1);
    }

    SizeT outSize = *compressedSize - LZMA_HEADER_SIZE;
    SizeT propsSize = LZMA_PROPS_SIZE;
    SRes res = LzmaEncode((Byte *)compressed + LZMA_HEADER_SIZE, &outSize,
                          bjc_enc ? bjc_enc : (const Byte *)uncompressed, uncompressedSize,
                          &props, (Byte *)compressed + 1, &propsSize,
                          TRUE /* add EOS marker */, NULL, &lzmaAlloc, &lzmaAlloc);
    if (SZ_OK == res && propsSize == LZMA_PROPS_SIZE)
        lzma_size = outSize + LZMA_HEADER_SIZE;
    free(bjc_enc);

    if (lzma_size < uncompressedSize + 1) {
        compressed[0] = bjc_enc ? 1 : 0;
        *compressedSize = lzma_size;
    }
    else {
StoreUncompressed:
        compressed[0] = (uint8_t)-1;
        memcpy(compressed + 1, uncompressed, uncompressedSize);
        *compressedSize = uncompressedSize + 1;
    }

    return true;
}

static bool AppendEntry(str::Str<char>& data, str::Str<char>& content, const WCHAR *filePath, const char *inArchiveName, lzma::FileInfo *fi=NULL)
{
    size_t headerSize = 25 + str::Len(inArchiveName);
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
        strcpy_s(data.AppendBlanks(headerSize - 24), headerSize - 24, inArchiveName);
        return content.AppendChecked(fi->compressedData, fi->compressedSize);
    }

    size_t fileDataLen;
    ScopedMem<char> fileData(file::ReadAll(filePath, &fileDataLen));
    if (!fileData || fileDataLen > UINT32_MAX)
        return false;
    uint32_t fileDataCrc = crc32(0, (const uint8_t *)fileData.Get(), (uint32_t)fileDataLen);
    if (fi && fi->uncompressedCrc32 == fileDataCrc && fi->uncompressedSize == fileDataLen)
        goto ReusePrevious;

    ScopedMem<char> compressed((char *)malloc(fileDataLen + 1));
    if (!compressed)
        return false;
    size_t compressedSize = fileDataLen + 1;
    if (!Compress(fileData, fileDataLen, compressed, &compressedSize))
        return false;

    ByteWriterLE meta(data.AppendBlanks(24), 24);
    meta.Write32(headerSize);
    meta.Write32(compressedSize);
    meta.Write32((uint32_t)fileDataLen);
    meta.Write32(fileDataCrc);
    meta.Write32(ft.dwLowDateTime);
    meta.Write32(ft.dwHighDateTime);
    strcpy_s(data.AppendBlanks(headerSize - 24), headerSize - 24, inArchiveName);
    return content.AppendChecked(compressed, compressedSize);
}

bool CreateArchive(const WCHAR *archivePath, const WCHAR *srcDir, WStrVec& names)
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
    lzsaHeader.Write32(names.Count() / 2);

    for (size_t i = 0; i < names.Count(); i += 2) {
        ScopedMem<WCHAR> filePath(path::Join(srcDir, names.At(i)));
        ScopedMem<char> utf8Name(str::conv::ToUtf8(names.At(names.At(i + 1) ? i + 1 : i)));
        str::TransChars(utf8Name, "/", "\\");
        int idx = GetIdxFromName(&prevArchive, utf8Name);
        lzma::FileInfo *fi = NULL;
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

int main(int argc, char **argv)
{
#ifdef DEBUG
    // report memory leaks on stderr
    _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    int errorStep = 1;

    WStrVec args;
    ParseCmdLine(GetCommandLine(), args);

    if (args.Count() == 2 && file::Exists(args.At(1))) {
        // verify LzSA file
        size_t fileDataLen;
        ScopedMem<char> fileData(file::ReadAll(args.At(1), &fileDataLen));
        FailIf(!fileData, "Failed to read \"%S\"", args.At(1));
        lzma::SimpleArchive lzsa;
        bool ok = lzma::ParseSimpleArchive(fileData, fileDataLen, &lzsa);
        FailIf(!ok, "\"%S\" is no valid LzSA file", args.At(1));
        for (int i = 0; i < lzsa.filesCount; i++) {
            ScopedMem<char> data(lzma::GetFileDataByIdx(&lzsa, i, NULL));
            FailIf(!data, "Failed to extract data for \"%s\"", lzsa.files[i].name);
        }
        printf("Verified all %d archive entries\n", lzsa.filesCount);
        return 0;
    }

    FailIf(args.Count() < 3, "Syntax: %S <archive.lzsa> <filename>[:<in-archive name>] [...]", path::GetBaseName(args.At(0)));

    WStrVec names;
    for (size_t i = 2; i < args.Count(); i++) {
        const WCHAR *sep = str::FindChar(args.At(i), ':');
        if (sep) {
            names.Append(str::DupN(args.At(i), sep - args.At(i)));
            names.Append(str::Dup(sep + 1));
        }
        else {
            names.Append(str::Dup(args.At(i)));
            names.Append(NULL);
        }
    }

    WCHAR srcDir[MAX_PATH];
    DWORD srcDirLen = GetCurrentDirectory(dimof(srcDir), srcDir);
    FailIf(!srcDirLen || srcDirLen == dimof(srcDir), "Failed to determine the current directory");

    bool ok = lzsa::CreateArchive(args.At(1), srcDir, names);
    FailIf(!ok, "Failed to create \"%S\"", args.At(1));

    return 0;
}
