/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#include "Transactions.h"
#include "FileUtil.h"
#include "WinUtil.h"
#include "Scopes.h"

// from Ktmw32.h
typedef HANDLE (WINAPI * CreateTransactionPtr)(LPSECURITY_ATTRIBUTES lpTransactionAttributes, LPGUID UOW, DWORD CreateOptions, DWORD IsolationLevel, DWORD IsolationFlags, DWORD Timeout, LPWSTR Description);
typedef BOOL (WINAPI * CommitTransactionPtr)(HANDLE TransactionHandle);
typedef BOOL (WINAPI * RollbackTransactionPtr)(HANDLE TransactionHandle);
// from WinBase.h
typedef HANDLE (WINAPI * CreateFileTransactedPtr)(LPCTSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile, HANDLE hTransaction, PUSHORT pusMiniVersion, PVOID pExtendedParameter);
typedef BOOL (WINAPI * DeleteFileTransactedPtr)(LPCTSTR lpFileName, HANDLE hTransaction);
// from WinError.h
#ifndef ERROR_TRANSACTIONAL_OPEN_NOT_ALLOWED
#define ERROR_TRANSACTIONAL_OPEN_NOT_ALLOWED 6832L
#endif

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

    HMODULE hLibKTM = SafeLoadLibrary(_T("ktmw32.dll"));
    HMODULE hLibKernel = SafeLoadLibrary(_T("kernel32.dll"));
    if (!hLibKTM || !hLibKernel)
        return;

#define Load(lib, func) _ ## func = (func ## Ptr)GetProcAddress(lib, #func)
    Load(hLibKTM, CreateTransaction);
    Load(hLibKTM, CommitTransaction);
    Load(hLibKTM, RollbackTransaction);
#undef Load
#ifdef UNICODE
#define Load(lib, func) _ ## func = (func ## Ptr)GetProcAddress(lib, #func "W")
#else
#define Load(lib, func) _ ## func = (func ## Ptr)GetProcAddress(lib, #func "A")
#endif
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

HANDLE FileTransaction::CreateFile(const TCHAR *filePath, DWORD dwDesiredAccess, DWORD dwCreationDisposition)
{
    if (hTrans) {
        HANDLE hFile = _CreateFileTransacted(filePath, dwDesiredAccess, 0, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL, hTrans, NULL, NULL);
        if (INVALID_HANDLE_VALUE != hFile || ERROR_TRANSACTIONAL_OPEN_NOT_ALLOWED != GetLastError())
            return hFile;
        // fall back to untransacted file I/O, if transactions aren't supported
    }
    return ::CreateFile(filePath, dwDesiredAccess, 0, NULL, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, NULL);
}

bool FileTransaction::WriteAll(const TCHAR *filePath, void *data, size_t dataLen)
{
    if (!hTrans)
        return file::WriteAll(filePath, data, dataLen);

    ScopedHandle h(CreateFile(filePath, GENERIC_WRITE, CREATE_ALWAYS));
    if (INVALID_HANDLE_VALUE == h)
        return false;

    DWORD size;
    BOOL ok = WriteFile(h, data, (DWORD)dataLen, &size, NULL);
    assert(!ok || (dataLen == (size_t)size));

    return ok && dataLen == (size_t)size;
}

bool FileTransaction::Delete(const TCHAR *filePath)
{
    if (hTrans) {
        BOOL ok = _DeleteFileTransacted(filePath, hTrans);
        if (ok || GetLastError() != ERROR_TRANSACTIONAL_OPEN_NOT_ALLOWED)
            return ok || GetLastError() == ERROR_FILE_NOT_FOUND;
        // fall back to an untransacted operation, if transactions aren't supported
    }
    return file::Delete(filePath);
}

bool FileTransaction::SetModificationTime(const TCHAR *filePath, FILETIME lastMod)
{
    if (!hTrans)
        return file::SetModificationTime(filePath, lastMod);

    ScopedHandle h(CreateFile(filePath, GENERIC_READ | GENERIC_WRITE, OPEN_EXISTING));
    if (INVALID_HANDLE_VALUE == h)
        return false;
    return SetFileTime(h, NULL, NULL, &lastMod);
}
