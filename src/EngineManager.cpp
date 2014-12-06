/* Copyright 2014 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
// model (engines, helpers)
#include "BaseEngine.h"
#include "DjVuEngine.h"
#include "EbookEngine.h"
#include "ImagesEngine.h"
#include "PdfEngine.h"
#include "PsEngine.h"
#include "EngineManager.h"

namespace EngineManager {

bool IsSupportedFile(const WCHAR *filePath, bool sniff, bool enableEbookEngines)
{
    return PdfEngine::IsSupportedFile(filePath, sniff)  ||
           XpsEngine::IsSupportedFile(filePath, sniff)  ||
           DjVuEngine::IsSupportedFile(filePath, sniff) ||
           ImageEngine::IsSupportedFile(filePath, sniff)||
           ImageDirEngine::IsSupportedFile(filePath, sniff) ||
           CbxEngine::IsSupportedFile(filePath, sniff)  ||
           PsEngine::IsSupportedFile(filePath, sniff)   ||
           Chm2Engine::IsSupportedFile(filePath, sniff) ||
           enableEbookEngines && (
               EpubEngine::IsSupportedFile(filePath, sniff) ||
               Fb2Engine::IsSupportedFile(filePath, sniff)  ||
               MobiEngine::IsSupportedFile(filePath, sniff) ||
               PdbEngine::IsSupportedFile(filePath, sniff)  ||
               HtmlEngine::IsSupportedFile(filePath, sniff) ||
               TxtEngine::IsSupportedFile(filePath, sniff)
           );
}

BaseEngine *CreateEngine(const WCHAR *filePath, PasswordUI *pwdUI, EngineType *typeOut, bool enableChm2Engine, bool enableEbookEngines)
{
    CrashIf(!filePath);

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
    } else if (ImageEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Image) {
        engine = ImageEngine::CreateFromFile(filePath);
        engineType = Engine_Image;
    } else if (ImageDirEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ImageDir) {
        engine = ImageDirEngine::CreateFromFile(filePath);
        engineType = Engine_ImageDir;
    } else if (CbxEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_ComicBook) {
        engine = CbxEngine::CreateFromFile(filePath);
        engineType = Engine_ComicBook;
    } else if (PsEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_PS) {
        engine = PsEngine::CreateFromFile(filePath);
        engineType = Engine_PS;
    } else if (enableChm2Engine && Chm2Engine::IsSupportedFile(filePath, sniff) && engineType != Engine_Chm2) {
        engine = Chm2Engine::CreateFromFile(filePath);
        engineType = Engine_Chm2;
    } else if (!enableEbookEngines) {
        // don't try to create any of the below ebook engines
    } else if (EpubEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Epub) {
        engine = EpubEngine::CreateFromFile(filePath);
        engineType = Engine_Epub;
    } else if (Fb2Engine::IsSupportedFile(filePath, sniff) && engineType != Engine_Fb2) {
        engine = Fb2Engine::CreateFromFile(filePath);
        engineType = Engine_Fb2;
    } else if (MobiEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Mobi) {
        engine = MobiEngine::CreateFromFile(filePath);
        engineType = Engine_Mobi;
    } else if (PdbEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Pdb) {
        engine = PdbEngine::CreateFromFile(filePath);
        engineType = Engine_Pdb;
    } else if (HtmlEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Html) {
        engine = HtmlEngine::CreateFromFile(filePath);
        engineType = Engine_Html;
    } else if (TxtEngine::IsSupportedFile(filePath, sniff) && engineType != Engine_Txt) {
        engine = TxtEngine::CreateFromFile(filePath);
        engineType = Engine_Txt;
    }

    if (!engine && !sniff) {
        // try sniffing the file instead
        sniff = true;
        goto RetrySniffing;
    }
    CrashIf(engine && !IsSupportedFile(filePath, sniff, enableEbookEngines));

    if (typeOut)
        *typeOut = engine ? engineType : Engine_None;
    return engine;
}

} // namespace EngineManager
