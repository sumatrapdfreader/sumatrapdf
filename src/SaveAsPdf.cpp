/* Copyright 2020 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/Log.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/TreeModel.h"

#include "resource.h"
#include "Translations.h"

#include "EngineBase.h"

#include "SaveAsPdf.h"

struct PdfMerger {
    fz_context* ctx = nullptr;
    pdf_document* doc_des = nullptr;
};

void SaveVirutalAsPdf(TocItem* root, char* dstPath) {
    // TODO: implement me
}
