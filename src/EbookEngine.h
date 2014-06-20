/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EpubEngine_h
#define EpubEngine_h

class BaseEngine;

namespace EpubEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}

namespace Fb2Engine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}

namespace MobiEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}

namespace PdbEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}

namespace Chm2Engine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}

namespace HtmlEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}

namespace TxtEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);

}

void SetDefaultEbookFont(const WCHAR *name, float size);

#endif
