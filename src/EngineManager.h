/* Copyright 2006-2012 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#ifndef EngineManager_h
#define EngineManager_h

enum EngineType {
    Engine_None,
    Engine_DjVu,
    Engine_Image, Engine_ImageDir, Engine_ComicBook,
    Engine_PDF, Engine_XPS,
    Engine_PS,
    Engine_Chm,
};

#include "BaseEngine.h"
#include "DjVuEngine.h"
#include "ImagesEngine.h"
#include "PdfEngine.h"
#include "PsEngine.h"
#include "ChmEngine.h"

class EngineManager {
public:
    static BaseEngine *CreateEngine(const TCHAR *filePath, PasswordUI *pwdUI=NULL, EngineType *type_out=NULL) {
        assert(filePath);
        if (!filePath) return NULL;

        BaseEngine *engine = NULL;
        EngineType engineType = Engine_None;
        bool sniff = false;
RetrySniffing:
        if (PdfEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_PDF) {
            engine = PdfEngine::CreateFromFile(filePath, pwdUI);
            engineType = Engine_PDF;
        } else if (XpsEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_XPS) {
            engine = XpsEngine::CreateFromFile(filePath);
            engineType = Engine_XPS;
        } else if (DjVuEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_DjVu) {
            engine = DjVuEngine::CreateFromFile(filePath);
            engineType = Engine_DjVu;
        } else if (CbxEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ComicBook) {
            engine = CbxEngine::CreateFromFile(filePath);
            engineType = Engine_ComicBook;
        } else if (ImageEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Image) {
            engine = ImageEngine::CreateFromFile(filePath);
            engineType = Engine_Image;
        } else if (ImageDirEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ImageDir) {
            engine = ImageDirEngine::CreateFromFile(filePath);
            engineType = Engine_ImageDir;
        } else if (PsEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_PS) {
            engine = PsEngine::CreateFromFile(filePath);
            engineType = Engine_PS;
        } else if (ChmEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Chm) {
            engine = ChmEngine::CreateFromFile(filePath);
            engineType = Engine_Chm;
        }

        if (!engine && !sniff) {
            // try sniffing the file instead
            sniff = true;
            goto RetrySniffing;
        }

        if (type_out)
            *type_out = engine ? engineType : Engine_None;
        return engine;
    }
};

#endif
