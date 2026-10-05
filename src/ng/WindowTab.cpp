/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#include "base/FileWatcher.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "AppSettings.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "Translations.h"
#include "ReadAloud.h"
#include "WindowTab.h"

WindowTab::WindowTab(MainWindow* win) {
    this->win = win;
}

void WindowTab::SetFilePath(Str path) {
    type = Type::Document;
    bool changed = filePath && !path::IsSame(filePath, path);
    if (changed && IsOpenCachePath(filePath)) {
        file::Delete(filePath);
    }
    if (changed) {
        str::FreePtr(&pendingFindText);
    }
    str::ReplaceWithCopy(&filePath, path);
}

void WindowTab::SetDisplayName(Str name) {
    str::ReplaceWithCopy(&displayName, name);
}

bool WindowTab::IsAboutTab() const {
    ReportIf(type == WindowTab::Type::None);
    return type == WindowTab::Type::About;
}

bool WindowTab::IsFavoritesTab() const {
    ReportIf(type == WindowTab::Type::None);
    return type == WindowTab::Type::Favorites;
}

// About or Favorites: no document controller
bool WindowTab::IsNonDocumentTab() const {
    return IsAboutTab() || IsFavoritesTab();
}

WindowTab::~WindowTab() {
    logf("~WindowTab: 0x%p, dm: 0x%p\n", this, AsFixed());
    ReadAloudForgetTab(this);
    // Drop MainWindow pointers into this tab / its controller before we free
    // them.
    if (win) {
        if (win->ctrl == ctrl) {
            win->ctrl = nullptr;
        }
        if (win->currentTabTemp == this) {
            win->currentTabTemp = nullptr;
        }
    }
    FileWatcherUnsubscribe(watcher);
    watcher = nullptr;
    delete selectionOnPage;
    // waits for in-flight renders off the UI thread; deletes on the UI thread
    DeleteControllerAsync(ctrl);
    ctrl = nullptr;
    if (IsOpenCachePath(filePath)) {
        file::Delete(filePath);
    }
    str::Free(filePath);
    filePath = {};
    str::Free(displayName);
    displayName = {};
    str::Free(frameTitle);
    frameTitle = {};
    str::Free(loadErrorReason);
    loadErrorReason = {};
    str::Free(pendingFindText);
    pendingFindText = {};
    str::Free(readAloudText);
    readAloudText = {};
    if (readAloudHighlight) {
        ReadAloudHighlightFree(readAloudHighlight);
        delete readAloudHighlight;
        readAloudHighlight = nullptr;
    }
}

bool WindowTab::IsDocLoaded() const {
    return ctrl != nullptr;
}

DisplayModel* WindowTab::AsFixed() const {
    return ctrl ? ctrl->AsFixed() : nullptr;
}

ChmModel* WindowTab::AsChm() const {
    return ctrl ? ctrl->AsChm() : nullptr;
}

MarkdownModel* WindowTab::AsMarkdown() const {
    return ctrl ? ctrl->AsMarkdown() : nullptr;
}

Kind WindowTab::GetEngineType() const {
    if (ctrl && ctrl->AsFixed()) {
        return ctrl->AsFixed()->GetEngine()->kind;
    }
    return nullptr;
}

// only if AsFixed()
EngineBase* WindowTab::GetEngine() const {
    if (ctrl && ctrl->AsFixed()) {
        return ctrl->AsFixed()->GetEngine();
    }
    return nullptr;
}

Str WindowTab::GetTabTitle() const {
    if (displayName) {
        return displayName;
    }
    if (len(filePath) == 0) {
        if (IsAboutTab()) {
            return StrL("Home");
        }
        if (IsFavoritesTab()) {
            // same label as Favorites menu / sidebar header
            return Tr("Favorites");
        }
        return StrL("");
    }
    TempStr embeddedFileName = ParseEmbeddedPdfName(filePath).fileName;
    if (embeddedFileName) {
        return embeddedFileName;
    }
    if (gSettings->fullPathInTitle) {
        return filePath;
    }
    return path::GetBaseNameTemp(filePath);
}

// the zoom ToggleZoom() would switch to. Split out so the command palette can
// name it without repeating (and drifting from) the cycle
float WindowTab::NextToggleZoom() const {
    float currZoom = ctrl ? ctrl->GetZoomVirtual() : kInvalidZoom;
    if (kZoomFitPage == currZoom) {
        return kZoomFitWidth;
    }
    if (kZoomFitWidth == currZoom) {
        return kZoomFitHeight;
    }
    if (kZoomFitHeight == currZoom) {
        return kZoomFitContent;
    }
    if (kZoomFitContent == currZoom) {
        return kZoomFitVisible;
    }
    if (kZoomFitVisible == currZoom) {
        return kZoomShrinkToFit;
    }
    return kZoomFitPage;
}

// ng: orig also stops read-aloud auto-scroll here (step 14)
void WindowTab::MoveDocBy(int dx, int dy) const {
    if (!ctrl) {
        return;
    }
    DisplayModel* dm = ctrl->AsFixed();
    ReportIf(!dm);
    if (!dm) {
        return;
    }
    if (0 != dx) {
        dm->ScrollXBy(dx);
    }
    if (0 != dy) {
        dm->ScrollYBy(dy, false);
    }

    if (win && !win->readAloudScrollFromCode) {
        ReadAloudOnUserViewChanged(win);
    }
}

void WindowTab::ToggleZoom() const {
    if (!IsDocLoaded()) {
        return;
    }
    ctrl->SetZoomVirtual(NextToggleZoom(), nullptr);
}

// ng: body copied from orig ExternalViewers.cpp, which is step 10.
// CouldBePDFDoc() is true for everything the mupdf engine renders -- epub, mobi,
// fb2, xps, svg -- which is what "open in Acrobat" wants but not what the
// PDF-only commands (Encrypt PDF, Show PDF Info, ...) want.
bool IsPdfDoc(WindowTab* tab) {
    if (!tab || !tab->ctrl) {
        // same permissive answer as CouldBePDFDoc for a document that failed to load
        return true;
    }
    if (tab->GetEngineType() != kindEngineMupdf) {
        return false;
    }
    DisplayModel* dm = tab->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    return !engine || EngineMupdfIsPdf(engine);
}
