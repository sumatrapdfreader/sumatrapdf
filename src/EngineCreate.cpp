/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/GuessFileType.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"

#include "SumatraConfig.h"
#include "Annotation.h"
#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "Flags.h"

static bool gEnableEpubWithPdfEngine = true;

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks) {
    if (IsEngineMupdfSupportedFileType(kind)) {
        return true;
    } else if (IsEngineDjVuSupportedFileType(kind)) {
        return true;
    } else if (IsEngineImageSupportedFileType(kind)) {
        return true;
    } else if (kind == kindDirectory) {
        // TODO: more complex
        return false;
    } else if (IsEngineCbxSupportedFileType(kind)) {
        return true;
    } else if (IsEnginePsSupportedFileType(kind)) {
        return true;
    }

    if (!enableEngineEbooks) {
        return false;
    }

    if (kind == kindFileEpub) {
        return true;
    } else if (kind == kindFileFb2) {
        return true;
    } else if (kind == kindFileFb2z) {
        return true;
    } else if (kind == kindFileMobi) {
        return true;
    } else if (kind == kindFilePalmDoc) {
        return true;
    } else if (kind == kindFileHTML) {
        return true;
    } else if (kind == kindFileTxt) {
        return true;
    }
    return false;
}

static EngineBase* CreateEngineForKind(Kind kind, const char* path, PasswordUI* pwdUI, bool enableChmEngine) {
    if (!kind) {
        return nullptr;
    }
    int dpi = DpiGet(nullptr);
    EngineBase* engine = nullptr;
    if (kind == kindFilePDF) {
        engine = CreateEngineMupdfFromFile(path, kind, dpi, pwdUI);
        return engine;
    }
    if (IsEngineDjVuSupportedFileType(kind)) {
        engine = CreateEngineDjVuFromFile(path);
        return engine;
    }
    if (IsEngineImageSupportedFileType(kind)) {
        engine = CreateEngineImageFromFile(path);
        return engine;
    }
    if (kind == kindDirectory) {
        // TODO: in 3.1.2 we open folder of images (IsEngineImageDirSupportedFile())
        // To avoid changing behavior, we open pdfs only in ramicro build
        // this should be controlled via cmd-line flag e.g. -folder-open-pdf
        // Then we could have more options, like -folder-open-images (default)
        // -folder-open-all (show all files we support in toc)
        if (!engine) {
            engine = CreateEngineImageDirFromFile(path);
        }
        return engine;
    }

    if (IsEngineCbxSupportedFileType(kind)) {
        engine = CreateEngineCbxFromFile(path);
        return engine;
    }
    if (IsEnginePsSupportedFileType(kind)) {
        engine = CreateEnginePsFromFile(path);
        return engine;
    }
    if (enableChmEngine && (kind == kindFileChm)) {
        engine = CreateEngineChmFromFile(path);
        return engine;
    }
    if (gEnableEpubWithPdfEngine && IsEngineMupdfSupportedFileType(kind)) {
        engine = CreateEngineMupdfFromFile(path, kind, dpi, pwdUI);
        // https://github.com/sumatrapdfreader/sumatrapdf/issues/2212
        // if failed to open with EngineMupdf, will also try to open
        // with my engine
        if (engine) {
            return engine;
        }
    }
#if 0
    if (kind == kindFileTxt) {
        engine = CreateEngineTxtFromFile(path);
        return engine;
    }
#endif

    if (kind == kindFileEpub) {
        engine = CreateEngineEpubFromFile(path);
        return engine;
    }
    if (kind == kindFileFb2 || kind == kindFileFb2z) {
        engine = CreateEngineFb2FromFile(path);
        return engine;
    }
    if (kind == kindFileMobi) {
        engine = CreateEngineMobiFromFile(path);
        return engine;
    }
    if (kind == kindFilePalmDoc) {
        engine = CreateEnginePdbFromFile(path);
        return engine;
    }
    if (kind == kindFileHTML) {
        engine = CreateEnginePdbFromFile(path);
        return engine;
    }
    return nullptr;
}

EngineBase* CreateEngineFromFile(const char* path, PasswordUI* pwdUI, bool enableChmEngine) {
    CrashIf(!path);

    // try to open with the engine guess from file name
    // if that fails, try to guess the file type based on content
    Kind kind = GuessFileTypeFromName(path);
    EngineBase* engine = CreateEngineForKind(kind, path, pwdUI, enableChmEngine);
    if (engine) {
        return engine;
    }

    Kind newKind = GuessFileTypeFromContent(path);
    if (kind != newKind) {
        engine = CreateEngineForKind(newKind, path, pwdUI, enableChmEngine);
    }
    return engine;
}

static bool IsEngineMupdf(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    return engine->kind == kindEngineMupdf;
}

bool EngineSupportsAnnotations(EngineBase* engine) {
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    return EngineMupdfSupportsAnnotations(engine);
}

bool EngineGetAnnotations(EngineBase* engine, Vec<Annotation*>& annotsOut) {
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    EngineMupdfGetAnnotations(engine, annotsOut);
    return true;
}

bool EngineHasUnsavedAnnotations(EngineBase* engine) {
    if (!IsEngineMupdf(engine)) {
        return false;
    }
    return EngineMupdfHasUnsavedAnnotations(engine);
}

Annotation* EngineGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, Annotation* annot) {
    if (!IsEngineMupdf(engine)) {
        return nullptr;
    }
    return EngineMupdfGetAnnotationAtPos(engine, pageNo, pos, annot);
}
