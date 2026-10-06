/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/GuessFileType.h"
#include "base/Pixmap.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "gui/UIModels.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "LitDoc.h"
#include "ReaderModel.h"

static EngineBase* CreateReaderEngine(Str path, PasswordUI* pwdUI) {
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
        return CreateEngineCbxFromFile(path, pwdUI, kind);
    }
    if (kind == FileType::Lit) {
        return CreateEngineLitFromFile(path, pwdUI);
    }
    if (IsEngineMupdfSupportedFileType(kind)) {
        return CreateEngineMupdfFromFile(path, kind, 96, pwdUI);
    }
    return nullptr;
}

ReaderModel* ReaderModel::Create(Str path, PasswordUI* pwdUI) {
    if (len(path) == 0) {
        return nullptr;
    }
    EngineBase* engine = CreateReaderEngine(path, pwdUI);
    if (!engine) {
        return nullptr;
    }
    if (engine->PageCount() < 1) {
        engine->Release();
        return nullptr;
    }

    return new ReaderModel(engine);
}

ReaderModel::~ReaderModel() {
    engine->Release();
}

int ReaderModel::PageCount() const {
    return engine->PageCount();
}

RectF ReaderModel::PageMediabox(int pageNo) const {
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        return {};
    }
    return engine->PageMediabox(pageNo);
}

float ReaderModel::FileDPI() const {
    float dpi = engine->fileDPI;
    return dpi > 0 ? dpi : 96.0f;
}

bool ReaderModel::Layout(const DocumentLayoutParams& params, DocumentLayout* layout) const {
    if (!layout) {
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

Pixmap* ReaderModel::RenderPageForPrint(int pageNo, float zoom, int rotation) const {
    if (pageNo < 1 || pageNo > engine->PageCount()) {
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
