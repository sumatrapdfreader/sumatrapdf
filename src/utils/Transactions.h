/* Copyright 2011 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING) */

#ifndef Transactions_h
#define Transactions_h

#include "BaseUtil.h"

// cf. http://www.codeproject.com/KB/vista/VistaKTM.aspx for the inspiration

class FileTransaction {
    HANDLE hTrans;

public:
    FileTransaction();
    ~FileTransaction();
    // if Commit is never called on a transaction, it will be
    // rolled back when the FileTransaction object is destroyed
    bool Commit();

    HANDLE CreateFile(const TCHAR *filePath, DWORD dwDesiredAccess, DWORD dwCreationDisposition);
    // same signatures as in FileUtil.h
    bool WriteAll(const TCHAR *filePath, void *data, size_t dataLen);
    bool Delete(const TCHAR *filePath);
    bool SetModificationTime(const TCHAR *filePath, FILETIME lastMod);
};

#endif
