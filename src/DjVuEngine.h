/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DjVuEngine_h
#define DjVuEngine_h

#include "BaseEngine.h"

class DjVuEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static DjVuEngine *CreateFromFileName(const TCHAR *fileName);
    static DjVuEngine *CreateFromStream(IStream *stream) { return NULL; }
};

#endif
