/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfEngine_h
#define PdfEngine_h

#include "BaseEngine.h"

class PasswordUI {
public:
    virtual TCHAR * GetPassword(const TCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) = 0;
};

class PdfEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName) {
        // note: the plugin hands in files with a different extension (.tmp),
        //       so callers may want to try to load even "unsupported" files
        return Str::EndsWithI(fileName, _T(".pdf"));
    }
    static PdfEngine *CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    static PdfEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI=NULL);
};

class XpsEngine : public BaseEngine {
public:
    static bool IsSupportedFile(const TCHAR *fileName) {
        return Str::EndsWithI(fileName, _T(".xps"));
    }
    static XpsEngine *CreateFromFileName(const TCHAR *fileName);
    static XpsEngine *CreateFromStream(IStream *stream);
};

void CalcMD5Digest(void *data, size_t byteCount, unsigned char digest[16]);

#endif
