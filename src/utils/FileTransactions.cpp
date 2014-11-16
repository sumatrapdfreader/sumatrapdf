/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "WinUtil.h"

// from Ktmw32.h
typedef HANDLE (WINAPI * CreateTransactionPtr)(LPSECURITY_ATTRIBUTES lpTransactionAttributes, LPGUID UOW, DWORD CreateOptions, DWORD IsolationLevel, DWORD IsolationFlags, DWORD Timeout, LPWSTR Description);
typedef BOOL (WINAPI * CommitTransactionPtr)(HANDLE TransactionHandle);
typedef BOOL (WINAPI * RollbackTransactionPtr)(HANDLE TransactionHandle);
// from WinBase.h
typedef HANDLE (WINAPI * CreateFileTransactedPtr)(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, HANDLE hTransaction, PUSHORT pusMiniVersion, PVOID pExtendedParameter);
typedef BOOL (WINAPI * DeleteFileTransactedPtr)(LPCWSTR lpFileName, HANDLE hTransaction);

static CreateTransactionPtr     _CreateTransaction = NULL;
static CommitTransactionPtr     _CommitTransaction = NULL;
static RollbackTransactionPtr   _RollbackTransaction = NULL;
static CreateFileTransactedPtr  _CreateFileTransacted = NULL;
static DeleteFileTransactedPtr  _DeleteFileTransacted = NULL;

static void InitializeTransactions()
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    HMODULE hLibKTM = SafeLoadLibrary(L"ktmw32.dll");
    HMODULE hLibKernel = SafeLoadLibrary(L"kernel32.dll");
    if (!hLibKTM || !hLibKernel)
        return;

#define Load(lib, func) _ ## func = (func ## Ptr)GetProcAddress(lib, #func)
    Load(hLibKTM, CreateTransaction);
    Load(hLibKTM, CommitTransaction);
    Load(hLibKTM, RollbackTransaction);
#undef Load
#define Load(lib, func) _ ## func = (func ## Ptr)GetProcAddress(lib, #func "W")
    Load(hLibKernel, CreateFileTransacted);
    Load(hLibKernel, DeleteFileTransacted);
#undef Load
}

FileTransaction::FileTransaction() : hTrans(NULL)
{
    InitializeTransactions();
    if (_CreateTransaction && _CommitTransaction && _RollbackTransaction && _CreateFileTransacted && _DeleteFileTransacted)
        hTrans = _CreateTransaction(NULL, 0, 0, 0, 0, 0, NULL);
}

FileTransaction::~FileTransaction()
{
    CloseHandle(hTrans);
}

bool FileTransaction::Commit()
{
    if (!hTrans)
        return true;
    return _CommitTransaction(hTrans);
}

HANDLE FileTransaction::CreateFile(const WCHAR *filePath, DWORD dwDesiredAccess, DWORD dwCreationDisposition)
{
    if (hTrans) {
        HANDLE hFile = _CreateFileTransacted(filePath, dwDesiredAccess, 0, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL, hTrans, NULL, NULL);
        DWORD err = GetLastError();
        if (INVALID_HANDLE_VALUE != hFile || ERROR_FILE_NOT_FOUND == err || ERROR_ACCESS_DENIED == err || ERROR_FILE_EXISTS  == err || ERROR_ALREADY_EXISTS  == err)
            return hFile;
        // fall back to untransacted file I/O, if transactions aren't supported
    }
    return ::CreateFile(filePath, dwDesiredAccess, 0, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
}

bool FileTransaction::WriteAll(const WCHAR *filePath, const void *data, size_t dataLen)
{
    if (!hTrans)
        return file::WriteAll(filePath, data, dataLen);

    ScopedHandle hFile(CreateFile(filePath, GENERIC_WRITE, CREATE_ALWAYS));
    if (INVALID_HANDLE_VALUE == hFile)
        return false;

    DWORD size;
    BOOL ok = WriteFile(hFile, data, (DWORD)dataLen, &size, NULL);
    assert(!ok || (dataLen == (size_t)size));

    return ok && dataLen == (size_t)size;
}

bool FileTransaction::Delete(const WCHAR *filePath)
{
    if (hTrans) {
        BOOL ok = _DeleteFileTransacted(filePath, hTrans);
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
    return SetFileTime(hFile, NULL, NULL, &lastMod);
}
