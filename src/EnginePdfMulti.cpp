/* Copyright 2019 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma warning(disable : 4611) // interaction between '_setjmp' and C++ object destruction is non-portable

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/Archive.h"
#include "utils/ScopedWin.h"
#include "utils/FileUtil.h"
#include "utils/HtmlParserLookup.h"
#include "utils/HtmlPullParser.h"
#include "utils/TrivialHtmlParser.h"
#include "utils/WinUtil.h"
#include "utils/ZipUtil.h"
#include "utils/Log.h"

#include "AppColors.h"
#include "wingui/TreeModel.h"
#include "EngineBase.h"
#include "EngineFzUtil.h"
#include "EnginePdf.h"

Kind kindEnginePdfMulti = "enginePdfMulti";

namespace EnginePdfMulti {

bool IsSupportedFile(const WCHAR* fileName, bool sniff = false) {
    return false;
}

BaseEngine* CreateFromFile(const WCHAR* fileName, PasswordUI* pwdUI = nullptr) {
    return nullptr;
}

BaseEngine* CreateFromStream(IStream* stream, PasswordUI* pwdUI = nullptr) {
    return nullptr;
}

} // namespace EnginePdfMulti
