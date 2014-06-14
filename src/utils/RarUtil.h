/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef RarUtil_h
#define RarUtil_h

class RarFile {
    ScopedMem<WCHAR> path;
    WStrList filenames;

    void ExtractFilenames();

public:
    explicit RarFile(const WCHAR *path);
    ~RarFile() { }

    size_t GetFileCount() const;
    // the result is owned by RarFile
    const WCHAR *GetFileName(size_t fileindex);
    // reverts GetFileName
    size_t GetFileIndex(const WCHAR *filename);

    // caller must free() the result
    char *GetFileDataByName(const WCHAR *filename, size_t *len=NULL);
    char *GetFileDataByIdx(size_t fileindex, size_t *len=NULL);
};

#endif
