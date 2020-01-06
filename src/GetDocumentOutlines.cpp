/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
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

#include "GetDocumentOutlines.h"

/* This is for debugging: gets document outlines for known document types */

bool GetDocumentOutlines(const WCHAR* path, Vec<OutlineEntry*>& entries) {
    UNUSED(path);
    UNUSED(entries);
    return false;
}
