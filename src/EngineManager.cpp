/* Copyright 2015 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// utils
#include "BaseUtil.h"
// rendering engines
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
           ChmEngine::IsSupportedFile(filePath, sniff) ||
           enableEbookEngines && (
               EpubEngine::IsSupportedFile(filePath, sniff) ||
               Fb2Engine::IsSupportedFile(filePath, sniff)  ||
               MobiEngine::IsSupportedFile(filePath, sniff) ||
               PdbEngine::IsSupportedFile(filePath, sniff)  ||
               HtmlEngine::IsSupportedFile(filePath, sniff) ||
               TxtEngine::IsSupportedFile(filePath, sniff)
           );
}

BaseEngine *CreateEngine(const WCHAR *filePath, PasswordUI *pwdUI, EngineType *typeOut, bool enableChmEngine, bool enableEbookEngines)
{
    CrashIf(!filePath);

    BaseEngine *engine = nullptr;
    EngineType engineType = EngineType::None;
    bool sniff = false;
RetrySniffing:
    if (PdfEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::PDF) {
        engine = PdfEngine::CreateFromFile(filePath, pwdUI);
        engineType = EngineType::PDF;
    } else if (XpsEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::XPS) {
        engine = XpsEngine::CreateFromFile(filePath);
        engineType = EngineType::XPS;
    } else if (DjVuEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::DjVu) {
        engine = DjVuEngine::CreateFromFile(filePath);
        engineType = EngineType::DjVu;
    } else if (ImageEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Image) {
        engine = ImageEngine::CreateFromFile(filePath);
        engineType = EngineType::Image;
    } else if (ImageDirEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::ImageDir) {
        engine = ImageDirEngine::CreateFromFile(filePath);
        engineType = EngineType::ImageDir;
    } else if (CbxEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::ComicBook) {
        engine = CbxEngine::CreateFromFile(filePath);
        engineType = EngineType::ComicBook;
    } else if (PsEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::PS) {
        engine = PsEngine::CreateFromFile(filePath);
        engineType = EngineType::PS;
    } else if (enableChmEngine && ChmEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Chm) {
        engine = ChmEngine::CreateFromFile(filePath);
        engineType = EngineType::Chm;
    } else if (!enableEbookEngines) {
        // don't try to create any of the below ebook engines
    } else if (EpubEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Epub) {
        engine = EpubEngine::CreateFromFile(filePath);
        engineType = EngineType::Epub;
    } else if (Fb2Engine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Fb2) {
        engine = Fb2Engine::CreateFromFile(filePath);
        engineType = EngineType::Fb2;
    } else if (MobiEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Mobi) {
        engine = MobiEngine::CreateFromFile(filePath);
        engineType = EngineType::Mobi;
    } else if (PdbEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Pdb) {
        engine = PdbEngine::CreateFromFile(filePath);
        engineType = EngineType::Pdb;
    } else if (HtmlEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Html) {
        engine = HtmlEngine::CreateFromFile(filePath);
        engineType = EngineType::Html;
    } else if (TxtEngine::IsSupportedFile(filePath, sniff) && engineType != EngineType::Txt) {
        engine = TxtEngine::CreateFromFile(filePath);
        engineType = EngineType::Txt;
    }

    if (!engine && !sniff) {
        // try sniffing the file instead
        sniff = true;
        goto RetrySniffing;
    }
    CrashIf(engine && !IsSupportedFile(filePath, sniff, enableEbookEngines));

    if (typeOut)
        *typeOut = engine ? engineType : EngineType::None;
    return engine;
}

} // namespace EngineManager
