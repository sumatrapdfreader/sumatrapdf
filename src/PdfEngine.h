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
    // caller must free() the result
    virtual char *GetDecryptionKey() const = 0;
    virtual void RunGC() = 0;

protected:
    virtual bool load(const TCHAR *fileName, PasswordUI *pwdUI=NULL) = 0;
    virtual bool load(IStream *stream, PasswordUI *pwdUI=NULL) = 0;

public:
    static PdfEngine *CreateFromFileName(const TCHAR *fileName, PasswordUI *pwdUI=NULL);
    static PdfEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI=NULL);
};

class XpsEngine : public BaseEngine {
public:
protected:
    virtual bool load(const TCHAR *fileName) = 0;
    virtual bool load(IStream *stream) = 0;

public:
    static XpsEngine *CreateFromFileName(const TCHAR *fileName);
    static XpsEngine *CreateFromStream(IStream *stream);
};

#endif
