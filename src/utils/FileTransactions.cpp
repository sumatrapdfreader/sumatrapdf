/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#include "BaseUtil.h"
#include "WinDynCalls.h"
#include "FileTransactions.h"
#include "FileUtil.h"
#include "WinUtil.h"

/*
We removed the use of transacted file ops because Microsoft discourages using
them: https://msdn.microsoft.com/en-us/library/windows/desktop/hh802690(v=vs.85).aspx

TODO: remove FileTransaction altogether and update clients

*/

FileTransaction::FileTransaction() : hTrans(nullptr)
{
}

FileTransaction::~FileTransaction()
{
}

bool FileTransaction::Commit()
{
    return true;
}

HANDLE FileTransaction::CreateFile(const WCHAR *filePath, DWORD dwDesiredAccess, DWORD dwCreationDisposition)
{
    return ::CreateFile(filePath, dwDesiredAccess, 0, nullptr, dwCreationDisposition, FILE_ATTRIBUTE_NORMAL, nullptr);
}

bool FileTransaction::WriteAll(const WCHAR *filePath, const void *data, size_t dataLen)
{
    return file::WriteAll(filePath, data, dataLen);
}

bool FileTransaction::Delete(const WCHAR *filePath)
{
    return file::Delete(filePath);
}

bool FileTransaction::SetModificationTime(const WCHAR *filePath, FILETIME lastMod)
{
    return file::SetModificationTime(filePath, lastMod);
}
