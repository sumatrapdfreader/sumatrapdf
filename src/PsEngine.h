/* Copyright 2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PsEngine_h
#define PsEngine_h

#include "BaseEngine.h"

class PsEngine : public BaseEngine {
public:
    virtual bool SaveFileAsPDF(const WCHAR *copyFileName) = 0;

public:
    static bool IsAvailable();
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static PsEngine *CreateFromFile(const WCHAR *fileName);
};

#endif
