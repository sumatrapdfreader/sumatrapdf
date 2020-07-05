/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/GuessFileType.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"
#include "utils/Log.h"
#include "utils/LogDbg.h"

#include "AppColors.h"
#include "wingui/TreeModel.h"

#include "Annotation.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EngineMupdf.h"

bool IsMupdfEngineSupportedFileType(Kind kind) {
    if (kind == kindFileEpub) {
        return true;
    }
    return false;
}

EngineBase* CreateEngineMupdfFromFile(const WCHAR* path) {
    UNUSED(path);
    return nullptr;
}

EngineBase* CreateEngineMupdfFromStream(IStream* stream) {
    UNUSED(stream);
    return nullptr;
}
