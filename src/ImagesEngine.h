/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"

class ImageEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ImageEngine *CreateFromFile(const TCHAR *fileName);
    static ImageEngine *CreateFromStream(IStream *stream);
};

class ImageDirEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ImageDirEngine *CreateFromFile(const TCHAR *fileName);
};

class CbxEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static CbxEngine *CreateFromFile(const TCHAR *fileName);
    static CbxEngine *CreateFromStream(IStream *stream);
};

RenderedBitmap *LoadRenderedBitmap(const TCHAR *filePath);
bool SaveRenderedBitmap(RenderedBitmap *bmp, const TCHAR *filePath);

#endif
