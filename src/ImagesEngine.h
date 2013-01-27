/* Copyright 2013 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"

class ImageEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static ImageEngine *CreateFromFile(const WCHAR *fileName);
    static ImageEngine *CreateFromStream(IStream *stream);
};

class ImageDirEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static ImageDirEngine *CreateFromFile(const WCHAR *fileName);
};

class CbxEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
    static CbxEngine *CreateFromFile(const WCHAR *fileName);
    static CbxEngine *CreateFromStream(IStream *stream);
};

RenderedBitmap *LoadRenderedBitmap(const WCHAR *filePath);
bool SaveRenderedBitmap(RenderedBitmap *bmp, const WCHAR *filePath);

#endif
