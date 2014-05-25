/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PsEngine_h
#define PsEngine_h

class BaseEngine;

namespace PsEngine {

bool IsAvailable();
bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}

#endif
