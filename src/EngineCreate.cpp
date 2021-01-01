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
#include "EngineMupdf.h"
#include "EngineCreate.h"

// still working on EngineMupdf, so this is an easy way
// to enable / disable it
static bool gEnableMupdfEngine = true;

bool IsSupportedFileType(Kind kind, bool enableEngineEbooks) {
    if (IsPdfEngineSupportedFileType(kind)) {
        return true;
    } else if (kind == kindFileVbkm) {
        return true;
    } else if (IsXpsEngineSupportedFileType(kind)) {
        return true;
    } else if (IsDjVuEngineSupportedFileType(kind)) {
        return true;
    } else if (IsImageEngineSupportedFileType(kind)) {
        return true;
    } else if (kind == kindFileDir) {
        // TODO: more complex
        return false;
    } else if (IsCbxEngineSupportedFileType(kind)) {
        return true;
    } else if (IsPsEngineSupportedFileType(kind)) {
        return true;
    } else if (gEnableMupdfEngine && IsMupdfEngineSupportedFileType(kind)) {
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
    } else if (kind == kindFileVbkm) {
        engine = CreateEngineMultiFromFile(path, pwdUI);
    } else if (IsXpsEngineSupportedFileType(kind)) {
        engine = CreateXpsEngineFromFile(path);
    } else if (IsDjVuEngineSupportedFileType(kind)) {
        engine = CreateDjVuEngineFromFile(path);
    } else if (IsImageEngineSupportedFileType(kind)) {
        engine = CreateImageEngineFromFile(path);
    } else if (kind == kindFileDir) {
        if (IsXpsDirectory(path)) {
            engine = CreateXpsEngineFromFile(path);
        }
        // in ra-micro builds, prioritize opening folders as multiple PDFs
        // for 'Open Folder' functionality
        // TODO: in 3.1.2 we open folder of images (IsImageDirEngineSupportedFile)
        // To avoid changing behavior, we open pdfs only in ramicro build
        // this should be controlled via cmd-line flag e.g. -folder-open-pdf
        // Then we could have more options, like -folder-open-images (default)
        // -folder-open-all (show all files we support in toc)
        if (!engine && gIsRaMicroBuild) {
            engine = CreateEngineMultiFromFile(path, pwdUI);
        }
        if (!engine) {
            engine = CreateImageDirEngineFromFile(path);
        }
    } else if (IsCbxEngineSupportedFileType(kind)) {
        engine = CreateCbxEngineFromFile(path);
    } else if (IsPsEngineSupportedFileType(kind)) {
        engine = CreatePsEngineFromFile(path);
    } else if (enableChmEngine && (kind == kindFileChm)) {
        engine = CreateChmEngineFromFile(path);
    } else if (gEnableMupdfEngine && kind == kindFileEpub) {
        engine = CreateEngineMupdfFromFile(path);
    }

    if (!enableEngineEbooks) {
        return engine;
    }

    if (kind == kindFileEpub) {
        engine = CreateEpubEngineFromFile(path);
    } else if (kind == kindFileFb2) {
        engine = CreateFb2EngineFromFile(path);
    } else if (kind == kindFileMobi) {
        engine = CreateMobiEngineFromFile(path);
    } else if (kind == kindFilePalmDoc) {
        engine = CreatePdbEngineFromFile(path);
    } else if (kind == kindFileHTML) {
        engine = CreatePdbEngineFromFile(path);
    } else if (kind == kindFileTxt) {
        engine = CreateTxtEngineFromFile(path);
    }
    return engine;
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

bool EngineSupportsAnnotations(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    Kind kind = engine->kind;
    if (kind == kindEnginePdf) {
        return true;
    }
    return false;
}

bool EngineGetAnnotations(EngineBase* engine, Vec<Annotation*>* annotsOut) {
    if (!engine) {
        return false;
    }
    Kind kind = engine->kind;
    if (kind != kindEnginePdf) {
        return false;
    }
    EnginePdfGetAnnotations(engine, annotsOut);
    return true;
}

bool EngineHasUnsavedAnnotations(EngineBase* engine) {
    if (!engine) {
        return false;
    }
    Kind kind = engine->kind;
    if (kind != kindEnginePdf) {
        return false;
    }
    return EnginePdfHasUnsavedAnnotations(engine);
}

Annotation* EngineGetAnnotationAtPos(EngineBase* engine, int pageNo, PointF pos, AnnotationType* allowedAnnots) {
    if (!engine) {
        return nullptr;
    }
    Kind kind = engine->kind;
    if (kind != kindEnginePdf) {
        return nullptr;
    }
    return EnginePdfGetAnnotationAtPos(engine, pageNo, pos, allowedAnnots);
}
