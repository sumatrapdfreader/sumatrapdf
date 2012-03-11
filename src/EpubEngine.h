/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EpubEngine_h
#define EpubEngine_h

#include "BaseEngine.h"

class EpubEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static EpubEngine *CreateFromFile(const TCHAR *fileName);
    static EpubEngine *CreateFromStream(IStream *stream);
};

class Fb2Engine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static Fb2Engine *CreateFromFile(const TCHAR *fileName);
};

#endif
