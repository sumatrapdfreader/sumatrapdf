/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef PdfEngine_h
#define PdfEngine_h

class BaseEngine;

class PasswordUI {
public:
    virtual WCHAR * GetPassword(const WCHAR *fileName, unsigned char *fileDigest,
                                unsigned char decryptionKeyOut[32], bool *saveKey) = 0;
    virtual ~PasswordUI() { }
};

namespace PdfEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName, PasswordUI *pwdUI=NULL);
BaseEngine *CreateFromStream(IStream *stream, PasswordUI *pwdUI=NULL);

}

namespace XpsEngine {

bool IsSupportedFile(const WCHAR *fileName, bool sniff=false);
BaseEngine *CreateFromFile(const WCHAR *fileName);
BaseEngine *CreateFromStream(IStream *stream);

}

// swaps Fitz' draw device with the GDI+ device
void DebugGdiPlusDevice(bool enable);

#endif
