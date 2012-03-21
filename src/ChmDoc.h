/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmDoc_h
#define ChmDoc_h

#include "BaseUtil.h"
#include "Scoped.h"

#define CP_CHM_DEFAULT 1252

class ChmDoc {
    struct chmFile *chmHandle;

    // Data parsed from /#WINDOWS, /#STRINGS, /#SYSTEM files inside CHM file
    ScopedMem<char> title;
    ScopedMem<char> tocPath;
    ScopedMem<char> indexPath;
    ScopedMem<char> homePath;
    ScopedMem<char> creator;
    UINT codepage;

    void ParseWindowsData();
    bool ParseSystemData();

    bool Load(const TCHAR *fileName);

public:
    ChmDoc() : codepage(CP_CHM_DEFAULT) { }

    bool HasData(const char *fileName);
    unsigned char *GetData(const char *fileName, size_t *lenOut);
    TCHAR *GetProperty(const char *name);

    static ChmDoc *CreateFromFile(const TCHAR *fileName);
};

UINT GetChmCodepage(const TCHAR *fileName);

#endif
