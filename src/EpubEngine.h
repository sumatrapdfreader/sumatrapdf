/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EpubEngine_h
#define EpubEngine_h

#include "BaseEngine.h"

class EpubEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static EpubEngine *CreateFromFile(const WCHAR *fileName);
    static EpubEngine *CreateFromStream(IStream *stream);
};

class Fb2Engine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static Fb2Engine *CreateFromFile(const WCHAR *fileName);
};

class MobiEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static MobiEngine *CreateFromFile(const WCHAR *fileName);
};

class PdbEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static PdbEngine *CreateFromFile(const WCHAR *fileName);
};

class Chm2Engine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static Chm2Engine *CreateFromFile(const WCHAR *fileName);
};

class TcrEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static TcrEngine *CreateFromFile(const WCHAR *fileName);
};

class HtmlEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static HtmlEngine *CreateFromFile(const WCHAR *fileName);
};

class TxtEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static TxtEngine *CreateFromFile(const WCHAR *fileName);
};

#endif
