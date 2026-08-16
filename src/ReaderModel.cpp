/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "TreeModel.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "ReaderModel.h"

static EngineBase* CreateReaderEngine(Str path) {
    if (IsEngineImageDirSupportedFile(path)) {
        return CreateEngineImageDirFromFile(path);
    }

    FileType kind = GuessFileTypeFromName(path);
    if (IsEngineDjVuSupportedFileType(kind)) {
        return CreateEngineDjvuDecFromFile(path);
    }
    if (IsEngineImageSupportedFileType(kind)) {
        return CreateEngineImageFromFile(path);
    }
    if (IsEngineCbxSupportedFileType(kind)) {
        return CreateEngineCbxFromFile(path, nullptr, kind);
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, nullptr);
    }
    return nullptr;
}

ReaderModel* ReaderModel::Create(Str path) {
    if (!path) {
        return nullptr;
    }
    EngineBase* engine = CreateReaderEngine(path);
    if (!engine) {
        return nullptr;
    }
    if (engine->PageCount() < 1) {
        engine->Release();
        return nullptr;
    }

    auto* model = new ReaderModel();
    model->engine = engine;
    return model;
}

ReaderModel::~ReaderModel() {
    if (engine) {
        engine->Release();
        engine = nullptr;
    }
}

Str ReaderModel::FilePath() const {
    return engine ? engine->FilePath() : Str{};
}

int ReaderModel::PageCount() const {
    return engine ? engine->PageCount() : 0;
}

RectF ReaderModel::PageMediabox(int pageNo) const {
    if (!engine || pageNo < 1 || pageNo > engine->PageCount()) {
        return {};
    }
    return engine->PageMediabox(pageNo);
}

float ReaderModel::FileDPI() const {
    if (!engine) {
        return 96.0f;
    }
    float dpi = engine->GetFileDPI();
    return dpi > 0 ? dpi : 96.0f;
}

bool ReaderModel::Layout(const DocumentLayoutParams& params, DocumentLayout* layout) const {
    if (!engine || !layout) {
        return false;
    }
    int pageCount = engine->PageCount();
    if (pageCount < 1) {
        return false;
    }

    layout->Reset(pageCount);
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        layout->SetPageMediaBox(pageNo, engine->PageMediabox(pageNo));
    }
    layout->Relayout(params);
    return true;
}

Pixmap* ReaderModel::RenderPage(int pageNo, float zoom, int rotation) const {
    if (!engine || pageNo < 1 || pageNo > engine->PageCount()) {
        return nullptr;
    }
    if (zoom <= 0) {
        zoom = 1.0f;
    }
    RenderPageArgs args(pageNo, zoom, rotation);
    return engine->RenderPage(args);
}

Pixmap* ReaderModel::RenderPageForPrint(int pageNo, float zoom, int rotation) const {
    if (!engine || pageNo < 1 || pageNo > engine->PageCount()) {
        return nullptr;
    }
    if (zoom <= 0) {
        zoom = 1.0f;
    }
    RenderPageArgs args(pageNo, zoom, rotation, nullptr, RenderTarget::Print);
    return engine->RenderPage(args);
}

EngineBase* ReaderModel::GetEngine() const {
    return engine;
}
