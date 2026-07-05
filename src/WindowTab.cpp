/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/FileWatcher.h"
#include "base/GuessFileType.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "ChmModel.h"
#include "DisplayModel.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Selection.h"
#include "ReadAloudHighlight.h"
#include "Translations.h"
#include "EditAnnotations.h"


WindowTab::WindowTab(MainWindow* win) {
    this->win = win;
}

void WindowTab::SetFilePath(Str path) {
    type = Type::Document;
    str::ReplaceWithCopy(&filePath, path);
}

void WindowTab::SetDisplayName(Str name) {
    str::ReplaceWithCopy(&displayName, name);
}

bool WindowTab::IsAboutTab() const {
    ReportIf(type == WindowTab::Type::None);
    return type == WindowTab::Type::About;
}

WindowTab::~WindowTab() {
    logf("~WindowTab: 0x%p, dm: 0x%p\n", this, AsFixed());
    if (hwndPDFInfo) {
        DestroyWindow(hwndPDFInfo);
        hwndPDFInfo = nullptr;
    }
    if (hwndPDFOutline) {
        DestroyWindow(hwndPDFOutline);
        hwndPDFOutline = nullptr;
    }
    CloseAndDeleteEditAnnotationsWindow(this);
    FileWatcherUnsubscribe(watcher);
    if (AsChm()) {
        AsChm()->RemoveParentHwnd();
    }
    delete selectionOnPage;
    // technically we only need to clear ctrl == gMostRecentlyOpenedDoc
    // but gMostRecentlyOpenedDoc is only for dde commands
    // so doesn't need to be kept for long
    gMostRecentlyOpenedDoc = nullptr;
    delete ctrl;
    str::Free(filePath);
    filePath = {};
    str::Free(displayName);
    displayName = {};
    str::Free(frameTitle);
    frameTitle = {};
    str::Free(readAloudText);
    readAloudText = {};
    if (readAloudHighlight) {
        ReadAloudHighlightFree(readAloudHighlight);
        delete readAloudHighlight;
    }
    str::Free(claudeSessionId);
    claudeSessionId = {};
    if (claudeProcess) {
        TerminateProcess(claudeProcess, 0);
        CloseHandle(claudeProcess);
    }
    str::Free(grokSessionId);
    grokSessionId = {};
    if (grokProcess) {
        TerminateProcess(grokProcess, 0);
        CloseHandle(grokProcess);
    }
    str::Free(codexSessionId);
    codexSessionId = {};
    if (codexProcess) {
        TerminateProcess(codexProcess, 0);
        CloseHandle(codexProcess);
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

Kind WindowTab::GetEngineType() const {
    if (ctrl && ctrl->AsFixed()) {
        return ctrl->AsFixed()->GetEngine()->kind;
    }
    return nullptr;
}

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
    if (!filePath) {
        if (IsAboutTab()) {
            return StrL("Home");
        }
        return StrL("");
    }
    TempStr embeddedFileName = ParseEmbeddedPdfName(filePath).fileName;
    if (embeddedFileName) {
        return embeddedFileName;
    }
    if (gGlobalPrefs->fullPathInTitle) {
        return filePath;
    }
    return path::GetBaseNameTemp(filePath);
}

void WindowTab::MoveDocBy(int dx, int dy) const {
    if (!ctrl) {
        return;
    }
    DisplayModel* dm = ctrl->AsFixed();
    ReportIf(!dm);
    if (!dm) {
        return;
    }
    ReportIf(win->linkOnLastButtonDown);
    if (win->linkOnLastButtonDown) {
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
    ReportIf(!ctrl);
    if (!IsDocLoaded()) {
        return;
    }
    // TODO: maybe move to DocController?
    float newZoom = kZoomFitPage;
    float currZoom = ctrl->GetZoomVirtual();
    if (kZoomFitPage == currZoom) {
        newZoom = kZoomFitWidth;
    } else if (kZoomFitWidth == currZoom) {
        newZoom = kZoomFitContent;
    } else if (kZoomFitContent == currZoom) {
        newZoom = kZoomShrinkToFit;
    }
    ctrl->SetZoomVirtual(newZoom, nullptr);
}

// https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
#if 0
LinkSaver::LinkSaver(WindowTab* tab, HWND parentHwnd, const WCHAR* fileName) {
    this->tab = tab;
    this->parentHwnd = parentHwnd;
    this->fileName = fileName;
}
#endif

bool SaveDataToFile(HWND hwndParent, Str fileName, Str data) {
    if (!CanAccessDisk()) {
        return false;
    }

    // ReportIf(fileName && str::SliceFromChar(fileName, '/'));

    OPENFILENAME ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwndParent;

    WCHAR dstFileName[MAX_PATH] = {};
    if (fileName) {
        str::BufSet(dstFileName, dimof(dstFileName), fileName);
    }
    ofn.lpstrFile = dstFileName;
    ofn.nMaxFile = dimof(dstFileName);

    // Prepare the file filters (use \1 instead of \0 so that the
    // double-zero terminated string isn't cut by the string handling
    // methods too early on)
    TempStr fileFilterA = fmt("%s\1*.*\1", _TRA("All files"));
    TempWStr fileFilter = ToWStrTemp(fileFilterA);
    wstr::TransCharsInPlace(fileFilter, WStrL(L"\1"), WStrL(L"\0"));
    ofn.lpstrFilter = fileFilter.s;

    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    bool ok = GetSaveFileNameW(&ofn);
    if (!ok) {
        return false;
    }
    TempStr path = ToUtf8Temp(dstFileName);
    ok = file::WriteFile(path, data);
    // https://github.com/sumatrapdfreader/sumatrapdf/issues/1336
#if 0
    if (ok && tab && IsUntrustedFile(tab->filePath, gPluginURL)) {
        file::SetZoneIdentifier(dstFileName);
    }
#endif
    return ok;
}
