/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinDynCalls.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "WinUtil.h"

FileTransaction::FileTransaction() : hTrans(nullptr)
{
    if (DynCreateTransaction && DynCommitTransaction && DynRollbackTransaction && DynCreateFileTransactedW && DynDeleteFileTransactedW)
        hTrans = DynCreateTransaction(nullptr, 0, 0, 0, 0, 0, nullptr);
}

FileTransaction::~FileTransaction()
{
    CloseHandle(hTrans);
}

bool FileTransaction::Commit()
{
    if (!hTrans)
        return true;
    return DynCommitTransaction(hTrans);
}

HANDLE FileTransaction::CreateFile(const WCHAR *filePath, DWORD dwDesiredAccess, DWORD dwCreationDisposition)
{
    if (hTrans) {
        HANDLE hFile = DynCreateFileTransactedW(filePath, dwDesiredAccess, 0, nullptr, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr, hTrans, nullptr, nullptr);
        DWORD err = GetLastError();
        if (INVALID_HANDLE_VALUE != hFile || ERROR_FILE_NOT_FOUND == err || ERROR_ACCESS_DENIED == err || ERROR_FILE_EXISTS  == err || ERROR_ALREADY_EXISTS  == err)
            return hFile;
        // fall back to untransacted file I/O, if transactions aren't supported
    }
    return ::CreateFile(filePath, dwDesiredAccess, 0, nullptr, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool FileTransaction::WriteAll(const WCHAR *filePath, const void *data, size_t dataLen)
{
    if (!hTrans)
        return file::WriteAll(filePath, data, dataLen);

    ScopedHandle hFile(CreateFile(filePath, GENERIC_WRITE, CREATE_ALWAYS));
    if (INVALID_HANDLE_VALUE == hFile)
        return false;

    DWORD size;
    BOOL ok = WriteFile(hFile, data, (DWORD)dataLen, &size, nullptr);
    AssertCrash(!ok || (dataLen == (size_t)size));

    return ok && dataLen == (size_t)size;
}

bool FileTransaction::Delete(const WCHAR *filePath)
{
    if (hTrans) {
        BOOL ok = DynDeleteFileTransactedW(filePath, hTrans);
        DWORD err = GetLastError();
        if (ok || ERROR_FILE_NOT_FOUND == err || ERROR_ACCESS_DENIED == err)
            return ok || ERROR_FILE_NOT_FOUND == err;
        // fall back to an untransacted operation, if transactions aren't supported
    }
    return file::Delete(filePath);
}

bool FileTransaction::SetModificationTime(const WCHAR *filePath, FILETIME lastMod)
{
    if (!hTrans)
        return file::SetModificationTime(filePath, lastMod);

    ScopedHandle hFile(CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING));
    if (INVALID_HANDLE_VALUE == hFile)
        return false;
    return SetFileTime(hFile, nullptr, nullptr, &lastMod);
}
