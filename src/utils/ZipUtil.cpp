/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "ZipUtil.h"

#include "FileUtil.h"

// mini(un)zip
#include <ioapi.h>
#include <iowin32.h>
#include <iowin32s.h>
#include <zip.h>

ZipFile::ZipFile(const WCHAR *path, ZipMethod method, Allocator *allocator) :
    filenames(0, allocator), fileinfo(0, allocator), filepos(0, allocator),
    allocator(allocator), commentLen(0)
{
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    uf = unzOpen2_64(path, &ffunc);
    if (uf)
        ExtractFilenames(method);
}

ZipFile::ZipFile(IStream *stream, ZipMethod method, Allocator *allocator) :
    filenames(0, allocator), fileinfo(0, allocator), filepos(0, allocator),
    allocator(allocator), commentLen(0)
{
    zlib_filefunc64_def ffunc;
    fill_win32s_filefunc64(&ffunc);
    uf = unzOpen2_64(stream, &ffunc);
    if (uf)
        ExtractFilenames(method);
}

ZipFile::~ZipFile()
{
    if (!uf)
        return;
    unzClose(uf);
}

// cf. http://www.pkware.com/documents/casestudies/APPNOTE.TXT Appendix D
#define CP_ZIP 437

#define INVALID_ZIP_FILE_POS ((ZPOS64_T)-1)

void ZipFile::ExtractFilenames(ZipMethod method)
{
    if (!uf)
        return;

    unz_global_info64 ginfo;
    int err = unzGetGlobalInfo64(uf, &ginfo);
    if (err != UNZ_OK)
        return;
    unzGoToFirstFile(uf);

    for (int i = 0; i < ginfo.number_entry && UNZ_OK == err; i++) {
        unz_file_info64 finfo;
        char fileName[MAX_PATH];
        err = unzGetCurrentFileInfo64(uf, &finfo, fileName, dimof(fileName), NULL, 0, NULL, 0);
        // some file format specifications only allow Deflate as compression method (e.g. XPS and EPUB)
        bool isSupported = Zip_Any == method || Zip_None == finfo.compression_method ||
                                                method == finfo.compression_method;
        if (err == UNZ_OK && isSupported) {
            WCHAR fileNameT[MAX_PATH];
            UINT cp = (finfo.flag & (1 << 11)) ? CP_UTF8 : CP_ZIP;
            str::conv::FromCodePageBuf(fileNameT, dimof(fileNameT), fileName, cp);
            filenames.Append((WCHAR *)Allocator::Dup(allocator, fileNameT,
                (str::Len(fileNameT) + 1) * sizeof(WCHAR)));
            fileinfo.Append(finfo);

            unz64_file_pos fpos;
            err = unzGetFilePos64(uf, &fpos);
            if (err != UNZ_OK)
                fpos.num_of_file = INVALID_ZIP_FILE_POS;
            filepos.Append(fpos);
        }
        err = unzGoToNextFile(uf);
    }
    commentLen = ginfo.size_comment;
}

size_t ZipFile::GetFileIndex(const WCHAR *filename)
{
    return filenames.FindI(filename);
}

size_t ZipFile::GetFileCount() const
{
    assert(filenames.Count() == fileinfo.Count());
    return filenames.Count();
}

const WCHAR *ZipFile::GetFileName(size_t fileindex)
{
    if (fileindex >= filenames.Count())
        return NULL;
    return filenames.At(fileindex);
}

char *ZipFile::GetFileData(const WCHAR *filename, size_t *len)
{
    return GetFileData(GetFileIndex(filename), len);
}

char *ZipFile::GetFileData(size_t fileindex, size_t *len)
{
    if (!uf)
        return NULL;
    if (fileindex >= filenames.Count())
        return NULL;

    int err = -1;
    if (filepos.At(fileindex).num_of_file != INVALID_ZIP_FILE_POS)
        err = unzGoToFilePos64(uf, &filepos.At(fileindex));
    if (err != UNZ_OK) {
        char fileNameA[MAX_PATH];
        UINT cp = (fileinfo.At(fileindex).flag & (1 << 11)) ? CP_UTF8 : CP_ZIP;
        str::conv::ToCodePageBuf(fileNameA, dimof(fileNameA), filenames.At(fileindex), cp);
        err = unzLocateFile(uf, fileNameA, 0);
    }
    if (err != UNZ_OK)
        return NULL;
    err = unzOpenCurrentFilePassword(uf, NULL);
    if (err != UNZ_OK)
        return NULL;

    size_t len2 = (size_t)fileinfo.At(fileindex).uncompressed_size;
    // overflow check
    if (len2 != fileinfo.At(fileindex).uncompressed_size ||
        len2 + sizeof(WCHAR) < sizeof(WCHAR)) {
        unzCloseCurrentFile(uf);
        return NULL;
    }

    char *result = (char *)Allocator::Alloc(allocator, len2 + sizeof(WCHAR));
    if (result) {
        unsigned int readBytes = unzReadCurrentFile(uf, result, (unsigned int)len2);
        // zero-terminate for convenience
        result[len2] = result[len2 + 1] = '\0';
        if (readBytes != len2) {
            Allocator::Free(allocator, result);
            result = NULL;
        }
        else if (len) {
            *len = len2;
        }
    }

    err = unzCloseCurrentFile(uf);
    if (err != UNZ_OK) {
        // CRC mismatch, file content is likely damaged
        Allocator::Free(allocator, result);
        result = NULL;
    }

    return result;
}

FILETIME ZipFile::GetFileTime(const WCHAR *filename)
{
    return GetFileTime(GetFileIndex(filename));
}

FILETIME ZipFile::GetFileTime(size_t fileindex)
{
    FILETIME ft = { -1, -1 };
    if (uf && fileindex < fileinfo.Count()) {
        FILETIME ftLocal;
        DWORD dosDate = fileinfo.At(fileindex).dosDate;
        DosDateTimeToFileTime(HIWORD(dosDate), LOWORD(dosDate), &ftLocal);
        LocalFileTimeToFileTime(&ftLocal, &ft);
    }
    return ft;
}

char *ZipFile::GetComment(size_t *len)
{
    if (!uf)
        return NULL;
    char *comment = (char *)Allocator::Alloc(allocator, commentLen + 1);
    if (!comment)
        return NULL;
    int read = unzGetGlobalComment(uf, comment, commentLen);
    if (read <= 0) {
        Allocator::Free(allocator, comment);
        return NULL;
    }
    comment[commentLen] = '\0';
    if (len)
        *len = commentLen;
    return comment;
}

bool ZipFile::UnzipFile(const WCHAR *filename, const WCHAR *dir, const WCHAR *unzippedName)
{
    size_t len;
    char *data = GetFileData(filename, &len);
    if (!data)
        return false;

    str::Str<WCHAR> filePath(MAX_PATH * 2, allocator);
    filePath.Append(dir);
    if (!str::EndsWith(filePath.Get(), L"\\"))
        filePath.Append(L"\\");
    if (unzippedName) {
        filePath.Append(unzippedName);
    } else {
        filePath.Append(filename);
        str::TransChars(filePath.Get(), L"/", L"\\");
    }

    bool ok = file::WriteAll(filePath.Get(), data, len);
    Allocator::Free(allocator, data);
    return ok;
}


class ZipCreatorData {
public:
    WStrVec pathsAndZipNames;
};

ZipCreator::ZipCreator()
{
    d = new ZipCreatorData;
}

ZipCreator::~ZipCreator()
{
    delete d;
}

// add a given file under (optional) nameInZip 
// it's not a good idea to save absolute windows-style
// in the zip file, so we try to pick an intelligent
// name if filePath is absolute and nameInZip is NULL
bool ZipCreator::AddFile(const WCHAR *filePath, const WCHAR *nameInZip)
{
    if (!file::Exists(filePath))
        return false;
    d->pathsAndZipNames.Append(str::Dup(filePath));
    if (NULL == nameInZip) {
        if (path::IsAbsolute(filePath)) {
            nameInZip = path::GetBaseName(filePath);
        } else {
            nameInZip = filePath;
        }
    }
    d->pathsAndZipNames.Append(str::Dup(nameInZip));
    return true;
}

// filePath must be in dir, we use the filePath relative to dir
// as the zip name
bool ZipCreator::AddFileFromDir(const WCHAR *filePath, const WCHAR *dir)
{
    if (!str::StartsWith(filePath, dir))
        return false;
    const WCHAR *nameInZip = filePath + str::Len(dir);
    if (path::IsSep(*nameInZip))
        ++nameInZip;
    return AddFile(filePath, nameInZip);
}

bool ZipCreator::SaveAs(const WCHAR *zipFilePath)
{
    if (d->pathsAndZipNames.Count() == 0)
        return false;
    bool result = true;
    zipFile zf;
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64(&ffunc);
    zf = zipOpen2_64((const void*)zipFilePath, 0, NULL, &ffunc);
    if (!zf)
        return false;
    zip_fileinfo zi = { 0 };
    int err;

    size_t fileCount = d->pathsAndZipNames.Count() / 2;
    for (size_t i=0; i<fileCount; i++) {
        WCHAR *fileName = d->pathsAndZipNames.At(2*i);
        WCHAR *nameInZip = d->pathsAndZipNames.At(2*i+1);
        ScopedMem<char> nameInZipUtf(str::conv::ToUtf8(nameInZip));
        size_t fileSize;
        char *fileData = file::ReadAll(fileName, &fileSize);
        if (!fileData)
            goto Error;
        err = zipOpenNewFileInZip64(zf, nameInZipUtf, &zi, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION, 1);
        if (err != ZIP_OK)
            goto Error;
        err = zipWriteInFileInZip(zf, fileData, fileSize);
        if (err != ZIP_OK)
            goto Error;
        err = zipCloseFileInZip(zf);
        if (err != ZIP_OK)
            goto Error;
    }
    goto Exit;
Error:
    result = false;
Exit:
    err = zipClose(zf, NULL);
    if (err != ZIP_OK)
        result = false;
    return result;
}
