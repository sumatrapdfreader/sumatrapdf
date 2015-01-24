/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace PsEngine {

bool IsAvailable();
bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}
