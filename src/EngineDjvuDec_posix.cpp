/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"

#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"

void SafeReleaseDjvuDecStream(IStream*) {}

EngineBase* CloneDjvuDecStream(IStream*) {
    return nullptr;
}

bool LoadDjvuDecStreamData(IStream*, IStream**, Str&) {
    return false;
}

Str GetDjvuDecFileData(IStream*, Str filePath) {
    return file::ReadFile(filePath);
}

bool SaveDjvuDecStreamAs(IStream*, Str) {
    return false;
}
