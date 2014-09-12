/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ChmDoc_h
#define ChmDoc_h

#include "BaseEngine.h"

class EbookTocVisitor;

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
    bool ParseTocOrIndex(EbookTocVisitor *visitor, const char *path, bool isIndex);

    bool Load(const WCHAR *fileName);

public:
    ChmDoc() : chmHandle(NULL), codepage(0) { }
    ~ChmDoc();

    bool HasData(const char *fileName);
    unsigned char *GetData(const char *fileName, size_t *lenOut);

    char *ToUtf8(const unsigned char *text, UINT overrideCP=0);
    WCHAR *ToStr(const char *text);

    WCHAR *GetProperty(DocumentProperty prop);
    const char *GetHomePath();
    Vec<char *> *GetAllPaths();

    bool HasToc() const;
    bool ParseToc(EbookTocVisitor *visitor);
    bool HasIndex() const;
    bool ParseIndex(EbookTocVisitor *visitor);

    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static ChmDoc *CreateFromFile(const WCHAR *fileName);
};

#endif
