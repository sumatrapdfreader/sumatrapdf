/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace DjVuEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}
