/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/FileTypeSniff.h"

#include "wingui/TreeModel.h"

#include "SumatraConfig.h"
#include "Annotation.h"
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

bool IsSupportedFile(const WCHAR* path, bool sniff, bool enableEngineEbooks) {
    if (IsEnginePdfSupportedFile(path, sniff)) {
        return true;
    }
    if (IsEngineMultiSupportedFile(path, sniff)) {
        return true;
    }
    if (IsXpsEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsDjVuEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsImageDirEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsCbxEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsEnginePdfSupportedFile(path, sniff)) {
        return true;
    }
    if (IsPsEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsChmEngineSupportedFile(path, sniff)) {
        return true;
    }

    if (!enableEngineEbooks) {
        return false;
    }

    if (IsEpubEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsFb2EngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsMobiEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsPdbEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsHtmlEngineSupportedFile(path, sniff)) {
        return true;
    }
    if (IsTxtEngineSupportedFile(path, sniff)) {
        return true;
    }
    return false;
}

static EngineBase* CreateEngineOld(const WCHAR* path, PasswordUI* pwdUI, bool enableChmEngine,
                                   bool enableEngineEbooks) {
    CrashIf(!path);

    EngineBase* engine = nullptr;
    bool sniff = false;
RetrySniffing:
    if (IsEnginePdfSupportedFile(path, sniff)) {
        engine = CreateEnginePdfFromFile(path, pwdUI);
    } else if (IsEngineMultiSupportedFile(path, sniff)) {
        engine = CreateEngineMultiFromFile(path, pwdUI);
    } else if (IsXpsEngineSupportedFile(path, sniff)) {
        engine = CreateXpsEngineFromFile(path);
    } else if (IsDjVuEngineSupportedFile(path, sniff)) {
        engine = CreateDjVuEngineFromFile(path);
    } else if (IsImageEngineSupportedFile(path, sniff)) {
        engine = CreateImageEngineFromFile(path);
    } else if (IsImageDirEngineSupportedFile(path, sniff)) {
        engine = CreateImageDirEngineFromFile(path);
    } else if (IsCbxEngineSupportedFile(path, sniff)) {
        engine = CreateCbxEngineFromFile(path);
    } else if (IsPsEngineSupportedFile(path, sniff)) {
        engine = CreatePsEngineFromFile(path);
    } else if (enableChmEngine && IsChmEngineSupportedFile(path, sniff)) {
        engine = CreateChmEngineFromFile(path);
    } else if (!enableEngineEbooks) {
        // don't try to create any of the below ebook engines
    } else if (IsEpubEngineSupportedFile(path, sniff)) {
        engine = CreateEpubEngineFromFile(path);
    } else if (IsFb2EngineSupportedFile(path, sniff)) {
        engine = CreateFb2EngineFromFile(path);
    } else if (IsMobiEngineSupportedFile(path, sniff)) {
        engine = CreateMobiEngineFromFile(path);
    } else if (IsPdbEngineSupportedFile(path, sniff)) {
        engine = CreatePdbEngineFromFile(path);
    } else if (IsHtmlEngineSupportedFile(path, sniff)) {
        engine = CreateHtmlEngineFromFile(path);
    } else if (IsTxtEngineSupportedFile(path, sniff)) {
        engine = CreateTxtEngineFromFile(path);
    }

    if (engine) {
        return engine;
    }

    if (!sniff) {
        // try sniffing the file instead
        sniff = true;
        goto RetrySniffing;
    }

    // CrashIf(engine && !IsSupportedFile(path, sniff, enableEngineEbooks));
    return engine;
}

static EngineBase* CreateEngineForKind(Kind kind, const WCHAR* path, PasswordUI* pwdUI, bool enableChmEngine,
                                       bool enableEngineEbooks) {
    if (!kind) {
        return nullptr;
    }
    EngineBase* engine = nullptr;
    if (kind == kindFilePDF) {
        engine = CreateEnginePdfFromFile(path, pwdUI);
    } else if (kind == kindFileVbkm) {
        engine = CreateEngineMultiFromFile(path, pwdUI);
    } else if (kind == kindFileXps) {
        engine = CreateXpsEngineFromFile(path);
    } else if (kind == kindFileDjVu) {
        engine = CreateDjVuEngineFromFile(path);
    } else if (IsImageEngineKind(kind)) {
        engine = CreateImageEngineFromFile(path);
    } else if (kind == kindFileDir) {
        if (IsXpsDirectory(path)) {
            engine = CreateXpsEngineFromFile(path);
        }
        // in ra-micro builds, prioritize opening folders as multiple PDFs
        if (!engine && gIsRaMicroBuild) {
            engine = CreateEngineMultiFromFile(path, pwdUI);
        }
        if (!engine) {
            engine = CreateImageDirEngineFromFile(path);
        }
    } else if (IsCbxEngineKind(kind)) {
        engine = CreateCbxEngineFromFile(path);
    } else if (kind == kindFilePS) {
        if (IsPsEngineAvailable()) {
            engine = CreatePsEngineFromFile(path);
        }

    }

    return engine;
    /*
        } else if (enableChmEngine && IsChmEngineSupportedFile(path, sniff)) {
            engine = CreateChmEngineFromFile(path);
        } else if (!enableEngineEbooks) {
            // don't try to create any of the below ebook engines
        } else if (IsEpubEngineSupportedFile(path, sniff)) {
            engine = CreateEpubEngineFromFile(path);
        } else if (IsFb2EngineSupportedFile(path, sniff)) {
            engine = CreateFb2EngineFromFile(path);
        } else if (IsMobiEngineSupportedFile(path, sniff)) {
            engine = CreateMobiEngineFromFile(path);
        } else if (IsPdbEngineSupportedFile(path, sniff)) {
            engine = CreatePdbEngineFromFile(path);
        } else if (IsHtmlEngineSupportedFile(path, sniff)) {
            engine = CreateHtmlEngineFromFile(path);
        } else if (IsTxtEngineSupportedFile(path, sniff)) {
            engine = CreateTxtEngineFromFile(path);
        }
    */
}

EngineBase* CreateEngine(const WCHAR* path, PasswordUI* pwdUI, bool enableChmEngine, bool enableEngineEbooks) {
    CrashIf(!path);

    Kind kind = FileTypeFromFileName(path);
    EngineBase* engine = CreateEngineForKind(kind, path, pwdUI, enableChmEngine, enableEngineEbooks);
    if (engine) {
        return engine;
    }

    // retry if the type based
    Kind newKind = SniffFileType(path);
    if (kind != newKind) {
        engine = CreateEngineForKind(newKind, path, pwdUI, enableChmEngine, enableEngineEbooks);
    }
    if (engine) {
        return engine;
    }
    // TODO: fallback to the old path
    engine = CreateEngineOld(path, pwdUI, enableChmEngine, enableEngineEbooks);
    return engine;
}

} // namespace EngineManager
