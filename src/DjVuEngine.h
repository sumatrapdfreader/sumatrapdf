/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef DjVuEngine_h
#define DjVuEngine_h

bool IsSupportedDjVuEngineFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateDjVuEngineFromFile(const WCHAR *fileName);

#endif
