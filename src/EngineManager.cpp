/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/TreeModel.h"

#include "EngineBase.h"
#include "EngineDjVu.h"
#include "EngineEbook.h"
#include "EngineImages.h"
#include "EnginePdf.h"
#include "EnginePs.h"
#include "EngineXps.h"
#include "EngineManager.h"

namespace EngineManager {

bool IsSupportedFile(const WCHAR* filePath, bool sniff, bool enableEbookEngines) {
    return PdfEngine::IsSupportedFile(filePath, sniff) || XpsEngine::IsSupportedFile(filePath, sniff) ||
           DjVuEngine::IsSupportedFile(filePath, sniff) || ImageEngine::IsSupportedFile(filePath, sniff) ||
           ImageDirEngine::IsSupportedFile(filePath, sniff) || CbxEngine::IsSupportedFile(filePath, sniff) ||
           PsEngine::IsSupportedFile(filePath, sniff) || ChmEngine::IsSupportedFile(filePath, sniff) ||
           enableEbookEngines &&
               (EpubEngine::IsSupportedFile(filePath, sniff) || Fb2Engine::IsSupportedFile(filePath, sniff) ||
                MobiEngine::IsSupportedFile(filePath, sniff) || PdbEngine::IsSupportedFile(filePath, sniff) ||
                HtmlEngine::IsSupportedFile(filePath, sniff) || TxtEngine::IsSupportedFile(filePath, sniff));
}

BaseEngine* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI, bool enableChmEngine, bool enableEbookEngines) {
    CrashIf(!filePath);

    BaseEngine* engine = nullptr;
    bool sniff = false;
RetrySniffing:
    if (PdfEngine::IsSupportedFile(filePath, sniff)) {
        engine = PdfEngine::CreateFromFile(filePath, pwdUI);
    } else if (XpsEngine::IsSupportedFile(filePath, sniff)) {
        engine = XpsEngine::CreateFromFile(filePath);
    } else if (DjVuEngine::IsSupportedFile(filePath, sniff)) {
        engine = DjVuEngine::CreateFromFile(filePath);
    } else if (ImageEngine::IsSupportedFile(filePath, sniff)) {
        engine = ImageEngine::CreateFromFile(filePath);
    } else if (ImageDirEngine::IsSupportedFile(filePath, sniff)) {
        engine = ImageDirEngine::CreateFromFile(filePath);
    } else if (CbxEngine::IsSupportedFile(filePath, sniff)) {
        engine = CbxEngine::CreateFromFile(filePath);
    } else if (PsEngine::IsSupportedFile(filePath, sniff)) {
        engine = PsEngine::CreateFromFile(filePath);
    } else if (enableChmEngine && ChmEngine::IsSupportedFile(filePath, sniff)) {
        engine = ChmEngine::CreateFromFile(filePath);
    } else if (!enableEbookEngines) {
        // don't try to create any of the below ebook engines
    } else if (EpubEngine::IsSupportedFile(filePath, sniff)) {
        engine = EpubEngine::CreateFromFile(filePath);
    } else if (Fb2Engine::IsSupportedFile(filePath, sniff)) {
        engine = Fb2Engine::CreateFromFile(filePath);
    } else if (MobiEngine::IsSupportedFile(filePath, sniff)) {
        engine = MobiEngine::CreateFromFile(filePath);
    } else if (PdbEngine::IsSupportedFile(filePath, sniff)) {
        engine = PdbEngine::CreateFromFile(filePath);
    } else if (HtmlEngine::IsSupportedFile(filePath, sniff)) {
        engine = HtmlEngine::CreateFromFile(filePath);
    } else if (TxtEngine::IsSupportedFile(filePath, sniff)) {
        engine = TxtEngine::CreateFromFile(filePath);
    }

    if (!engine && !sniff) {
        // try sniffing the file instead
        sniff = true;
        goto RetrySniffing;
    }
    CrashIf(engine && !IsSupportedFile(filePath, sniff, enableEbookEngines));

    return engine;
}

} // namespace EngineManager
