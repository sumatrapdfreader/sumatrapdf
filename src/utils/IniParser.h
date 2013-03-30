/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#ifndef IniParser_h
#define IniParser_h

struct IniLine {
    const char *key;
    const char *value;

    IniLine() : key(NULL), value(NULL) { }
    IniLine(const char *key, const char *value) : key(key), value(value) { }
};

class IniSection {
public:
    IniSection(const char *name) : name(name) { }
    const char *name;

    Vec<IniLine> lines;
    IniLine *FindLine(const char *key) const;
};

class IniFile {
    ScopedMem<char> data;
    void ParseData();

public:
    IniFile(const WCHAR *path);
    // data must be NULL-terminated
    IniFile(const char *data);
    ~IniFile() { DeleteVecMembers(sections); }

    Vec<IniSection *> sections;
    IniSection *FindSection(const char *name, size_t idx=0) const;
};

#endif
