/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"

class ImageEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ImageEngine *CreateFromFileName(const TCHAR *fileName);
    static ImageEngine *CreateFromStream(IStream *stream);
};

class ImageDirEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ImageDirEngine *CreateFromFileName(const TCHAR *fileName);
};

class CbxEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static CbxEngine *CreateFromFileName(const TCHAR *fileName);
};

RenderedBitmap *LoadRenderedBitmap(const TCHAR *filePath);
bool SaveRenderedBitmap(RenderedBitmap *bmp, const TCHAR *filePath);

#endif
