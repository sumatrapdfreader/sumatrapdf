/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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
#include "EngineMulti.h"
#include "EngineManager.h"

namespace EngineManager {

bool IsSupportedFile(const WCHAR* filePath, bool sniff, bool enableEngineEbooks) {
    if (IsEnginePdfSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsEngineMultiSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsXpsEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsDjVuEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsImageDirEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsCbxEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsEnginePdfSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsPsEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsChmEngineSupportedFile(filePath, sniff)) {
        return true;
    }

    if (!enableEngineEbooks) {
        return false;
    }

    if (IsEpubEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsFb2EngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsMobiEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsPdbEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsHtmlEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    if (IsTxtEngineSupportedFile(filePath, sniff)) {
        return true;
    }
    return false;
}

EngineBase* CreateEngine(const WCHAR* filePath, PasswordUI* pwdUI, bool enableChmEngine, bool enableEngineEbooks) {
    CrashIf(!filePath);

    EngineBase* engine = nullptr;
    bool sniff = false;
RetrySniffing:
    if (IsEnginePdfSupportedFile(filePath, sniff)) {
        engine = CreateEnginePdfFromFile(filePath, pwdUI);
    } else if (IsEngineMultiSupportedFile(filePath, sniff)) {
        engine = CreateEngineMultiFromFile(filePath, pwdUI);
    } else if (IsXpsEngineSupportedFile(filePath, sniff)) {
        engine = CreateXpsEngineFromFile(filePath);
    } else if (IsDjVuEngineSupportedFile(filePath, sniff)) {
        engine = CreateDjVuEngineFromFile(filePath);
    } else if (IsImageEngineSupportedFile(filePath, sniff)) {
        engine = CreateImageEngineFromFile(filePath);
    } else if (IsImageDirEngineSupportedFile(filePath, sniff)) {
        engine = CreateImageDirEngineFromFile(filePath);
    } else if (IsCbxEngineSupportedFile(filePath, sniff)) {
        engine = CreateCbxEngineFromFile(filePath);
    } else if (IsPsEngineSupportedFile(filePath, sniff)) {
        engine = CreatePsEngineFromFile(filePath);
    } else if (enableChmEngine && IsChmEngineSupportedFile(filePath, sniff)) {
        engine = CreateChmEngineFromFile(filePath);
    } else if (!enableEngineEbooks) {
        // don't try to create any of the below ebook engines
    } else if (IsEpubEngineSupportedFile(filePath, sniff)) {
        engine = CreateEpubEngineFromFile(filePath);
    } else if (IsFb2EngineSupportedFile(filePath, sniff)) {
        engine = CreateFb2EngineFromFile(filePath);
    } else if (IsMobiEngineSupportedFile(filePath, sniff)) {
        engine = CreateMobiEngineFromFile(filePath);
    } else if (IsPdbEngineSupportedFile(filePath, sniff)) {
        engine = CreatePdbEngineFromFile(filePath);
    } else if (IsHtmlEngineSupportedFile(filePath, sniff)) {
        engine = CreateHtmlEngineFromFile(filePath);
    } else if (IsTxtEngineSupportedFile(filePath, sniff)) {
        engine = CreateTxtEngineFromFile(filePath);
    }

    if (engine) {
        return engine;
    }

    if (!sniff) {
        // try sniffing the file instead
        sniff = true;
        goto RetrySniffing;
    }

    // CrashIf(engine && !IsSupportedFile(filePath, sniff, enableEngineEbooks));
    return engine;
}

} // namespace EngineManager
