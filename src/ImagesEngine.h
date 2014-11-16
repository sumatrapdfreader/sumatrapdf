/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

namespace ImageEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}

namespace ImageDirEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}

namespace CbxEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}

