/* Copyright 2021 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/GuessFileType.h"

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
#include "EngineCreate.h"

static bool gEnableEpubWithPdfEngine = true;

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks) {
    if (IsPdfEngineSupportedFileType(kind)) {
        return true;
    } else if (IsXpsEngineSupportedFileType(kind)) {
        return true;
    } else if (IsDjVuEngineSupportedFileType(kind)) {
        return true;
    } else if (IsImageEngineSupportedFileType(kind)) {
        return true;
    } else if (kind == kindDirectory) {
        // TODO: more complex
        return false;
    } else if (IsCbxEngineSupportedFileType(kind)) {
        return true;
    } else if (IsPsEngineSupportedFileType(kind)) {
        return true;
    }

    if (!enableEngineEbooks) {
        return false;
    }

    if (kind == kindFileEpub) {
        return true;
    } else if (kind == kindFileFb2) {
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

static EngineBase* CreateEngineForKind(Kind kind, const WCHAR* path, PasswordUI* pwdUI, bool enableChmEngine,
                                       bool enableEngineEbooks) {
    if (!kind) {
        return nullptr;
    }
    EngineBase* engine = nullptr;
    if (kind == kindFilePDF) {
        engine = CreateEnginePdfFromFile(path, pwdUI);
        return engine;
    }
    if (IsXpsEngineSupportedFileType(kind)) {
        engine = CreateXpsEngineFromFile(path);
        return engine;
    }
    if (IsDjVuEngineSupportedFileType(kind)) {
        engine = CreateDjVuEngineFromFile(path);
        return engine;
    }
    if (IsImageEngineSupportedFileType(kind)) {
        engine = CreateImageEngineFromFile(path);
        return engine;
    }
    if (kind == kindDirectory) {
        if (IsXpsDirectory(path)) {
            engine = CreateXpsEngineFromFile(path);
        }
        // TODO: in 3.1.2 we open folder of images (IsImageDirEngineSupportedFile)
        // To avoid changing behavior, we open pdfs only in ramicro build
        // this should be controlled via cmd-line flag e.g. -folder-open-pdf
        // Then we could have more options, like -folder-open-images (default)
        // -folder-open-all (show all files we support in toc)
        if (!engine) {
            engine = CreateImageDirEngineFromFile(path);
        }
        return engine;
    }

    if (IsCbxEngineSupportedFileType(kind)) {
        engine = CreateCbxEngineFromFile(path);
        return engine;
    }
    if (IsPsEngineSupportedFileType(kind)) {
        engine = CreatePsEngineFromFile(path);
        return engine;
    }
    if (enableChmEngine && (kind == kindFileChm)) {
        engine = CreateChmEngineFromFile(path);
        return engine;
    }
    if (kind == kindFileTxt) {
        engine = CreateTxtEngineFromFile(path);
        return engine;
    }

    if (gEnableEpubWithPdfEngine) {
        if (kind == kindFileEpub || kind == kindFileFb2) {
            engine = CreateEnginePdfFromFile(path, pwdUI);
        }
    }

    if (!enableEngineEbooks) {
        return engine;
    }

    if (kind == kindFileEpub) {
        engine = CreateEpubEngineFromFile(path);
        return engine;
    }
    if (kind == kindFileFb2) {
        engine = CreateFb2EngineFromFile(path);
        return engine;
    }
    if (kind == kindFileMobi) {
        engine = CreateMobiEngineFromFile(path);
        return engine;
    }
    if (kind == kindFilePalmDoc) {
        engine = CreatePdbEngineFromFile(path);
        return engine;
    }
    if (kind == kindFileHTML) {
        engine = CreatePdbEngineFromFile(path);
        return engine;
    }
    return nullptr;
}

EngineBase* CreateEngine(const WCHAR* path, PasswordUI* pwdUI, bool enableChmEngine, bool enableEngineEbooks) {
    CrashIf(!path);

    // try to open with the engine guess from file name
    // if that fails, try to guess the file type based on content
    Kind kind = GuessFileTypeFromName(path);
    EngineBase* engine = CreateEngineForKind(kind, path, pwdUI, enableChmEngine, enableEngineEbooks);
    if (engine) {
        return engine;
    }

    Kind newKind = GuessFileTypeFromContent(path);
    if (kind != newKind) {
        engine = CreateEngineForKind(newKind, path, pwdUI, enableChmEngine, enableEngineEbooks);
    }
    return engine;
}

static bool IsEnginePdf(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    return engine->kind == kindEnginePdf;
}

bool EngineSupportsAnnotations(EngineBase* engine) {
    return IsEnginePdf(engine);
}

bool EngineGetAnnotations(EngineBase* engine, Vec<Annotation*>* annotsOut) {
    if (!IsEnginePdf(engine)) {
        return false;
    }
    EnginePdfGetAnnotations(engine, annotsOut);
    return true;
}

bool EngineHasUnsavedAnnotations(EngineBase* engine) {
    if (!IsEnginePdf(engine)) {
        return false;
    }
    return EnginePdfHasUnsavedAnnotations(engine);
}

// caller must delete
Annotation* EngineGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, AnnotationType* allowedAnnots) {
    if (!IsEnginePdf(engine)) {
        return nullptr;
    }
    return EnginePdfGetAnnotationAtPos(engine, pageNo, pos, allowedAnnots);
}
