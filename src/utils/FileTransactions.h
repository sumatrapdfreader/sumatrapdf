/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef FileTransactions_h
#define FileTransactions_h

// cf. http://www.codeproject.com/KB/vista/VistaKTM.aspx for the inspiration

class FileTransaction {
    HANDLE hTrans;

public:
    FileTransaction();
    ~FileTransaction();

    // if Commit is never called on a transaction, it will be
    // rolled back when the FileTransaction object is destroyed
    bool Commit();

    // all these functions fall back on untransacted operations,
    // if either transactions aren't supported at all or if they're
    // not supported by the file system (e.g. on FAT32 or a network drive)
    HANDLE CreateFile(const WCHAR *filePath, DWORD dwDesiredAccess, DWORD dwCreationDisposition);
    // same signatures as in FileUtil.h
    bool WriteAll(const WCHAR *filePath, const void *data, size_t dataLen);
    bool Delete(const WCHAR *filePath);
    bool SetModificationTime(const WCHAR *filePath, FILETIME lastMod);
};

#endif
