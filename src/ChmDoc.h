/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmDoc_h
#define ChmDoc_h

#include "BaseUtil.h"
#include "Scoped.h"
#include "Vec.h"

#define CP_CHM_DEFAULT 1252

class ChmTocVisitor {
public:
    virtual void visit(const TCHAR *name, const TCHAR *url, int level) = 0;
};

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
    ~ChmDoc();

    bool HasData(const char *fileName);
    unsigned char *GetData(const char *fileName, size_t *lenOut);

    char *ToUtf8(const unsigned char *text);
    TCHAR *ToStr(const char *text);

    TCHAR *GetProperty(const char *name);
    const char *GetIndexPath();
    Vec<char *> *GetAllPaths();

    bool HasToc();
    bool ParseToc(ChmTocVisitor *visitor);

    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ChmDoc *CreateFromFile(const TCHAR *fileName);
};

#endif
