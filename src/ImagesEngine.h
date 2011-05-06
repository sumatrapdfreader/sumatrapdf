/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef ImagesEngine_H
#define ImagesEngine_H

#include "BaseEngine.h"

class ImageEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false) {
        return str::EndsWithI(fileName, _T(".png")) ||
               str::EndsWithI(fileName, _T(".jpg")) || str::EndsWithI(fileName, _T(".jpeg")) ||
               str::EndsWithI(fileName, _T(".gif")) ||
               str::EndsWithI(fileName, _T(".tif")) || str::EndsWithI(fileName, _T(".tiff")) ||
               str::EndsWithI(fileName, _T(".bmp"));
    }
    static ImageEngine *CreateFromFileName(const TCHAR *fileName);
    static ImageEngine *CreateFromStream(IStream *stream);
};

class ImageDirEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false);
    static ImageDirEngine *CreateFromFileName(const TCHAR *fileName);
    static ImageDirEngine *CreateFromStream(IStream *stream) { return NULL; }
};

class CbxEngine : public virtual BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName, bool sniff=false) {
        return str::EndsWithI(fileName, _T(".cbz")) ||
               str::EndsWithI(fileName, _T(".cbr"));
    }
    static CbxEngine *CreateFromFileName(const TCHAR *fileName);
    static CbxEngine *CreateFromStream(IStream *stream) { return NULL; }
};

RenderedBitmap *LoadRenderedBitmap(const TCHAR *filePath);
bool SaveRenderedBitmap(RenderedBitmap *bmp, const TCHAR *filePath);

#endif
