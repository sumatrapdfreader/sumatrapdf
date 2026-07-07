/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"

#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"

void SafeReleaseDjvuDecStream(IStream* stream) {
    if (stream) {
        stream->Release();
    }
}

EngineBase* CloneDjvuDecStream(IStream* stream) {
    return stream ? CreateEngineDjvuDecFromStream(stream) : nullptr;
}

bool LoadDjvuDecStreamData(IStream* stm, IStream** streamOut, Str& fileData) {
    fileData = GetDataFromStream(stm, nullptr);
    if (len(fileData) == 0) {
        return false;
    }
    *streamOut = stm;
    stm->AddRef();
    return true;
}

Str GetDjvuDecFileData(IStream* stream, Str filePath) {
    return GetStreamOrFileData(stream, filePath);
}

bool SaveDjvuDecStreamAs(IStream* stream, Str dstPath) {
    if (!stream) {
        return false;
    }
    Str d = GetDataFromStream(stream, nullptr);
    bool ok = len(d) > 0 && file::WriteFile(dstPath, d);
    str::Free(d);
    return ok;
}
