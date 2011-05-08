/* Copyright 2006-2011 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EngineManager_h
#define EngineManager_h

enum EngineType {
    Engine_None,
    Engine_DjVu,
    Engine_Image, Engine_ImageDir, Engine_ComicBook,
    Engine_PDF, Engine_XPS,
    Engine_PS,
};

#include "BaseEngine.h"
#include "DjVuEngine.h"
#include "ImagesEngine.h"
#include "PdfEngine.h"
#include "PsEngine.h"

class EngineManager {
public:
    static BaseEngine *CreateEngine(const TCHAR *filePath, PasswordUI *pwdUI=NULL, EngineType *type_out=NULL) {
        assert(filePath);
        if (!filePath) return NULL;

        BaseEngine *engine = NULL;
        EngineType engineType;
        bool sniff = false;
RetrySniffing:
        if (PdfEngine::IsSupportedFile(filePath, sniff)) {
            engine = PdfEngine::CreateFromFileName(filePath, pwdUI);
            engineType = Engine_PDF;
        } else if (XpsEngine::IsSupportedFile(filePath, sniff)) {
            engine = XpsEngine::CreateFromFileName(filePath);
            engineType = Engine_XPS;
        } else if (DjVuEngine::IsSupportedFile(filePath, sniff)) {
            engine = DjVuEngine::CreateFromFileName(filePath);
            engineType = Engine_DjVu;
        } else if (CbxEngine::IsSupportedFile(filePath, sniff)) {
            engine = CbxEngine::CreateFromFileName(filePath);
            engineType = Engine_ComicBook;
        } else if (ImageEngine::IsSupportedFile(filePath, sniff)) {
            engine = ImageEngine::CreateFromFileName(filePath);
            engineType = Engine_Image;
        } else if (ImageDirEngine::IsSupportedFile(filePath, sniff)) {
            engine = ImageDirEngine::CreateFromFileName(filePath);
            engineType = Engine_ImageDir;
        } else if (PsEngine::IsSupportedFile(filePath, sniff)) {
            engine = PsEngine::CreateFromFileName(filePath);
            engineType = Engine_PS;
        }
        if (!engine && !sniff) {
            // try sniffing the file instead
            sniff = true;
            goto RetrySniffing;
        }
        if (!engine)
            engineType = Engine_None;

        if (type_out)
            *type_out = engineType;
        return engine;
    }
};

#endif
