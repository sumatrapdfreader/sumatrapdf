/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "base/File.h"
#include "base/Win.h"
#include "base/GuessFileType.h"
#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "ChmModel.h"
#include "MarkdownModel.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "TextSelection.h"
#include "Annotation.h"
#include "TextSearch.h"
#include "SumatraPDF.h"
#include "AIChatCommon.h"
#include "AIChatPanel.h"
#include "SelectionToolbar.h"
#include "AnnotEditToolbar.h"
#include "AnnotTextPopup.h"
#include "AnnotFilterToolbar.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "SearchAndDDE.h"
#include "RefHover.h"
#include "WindowTab.h"
#include "TableOfContents.h"
#include "StressTesting.h"
#include "ExternalViewers.h"
#include "CommandAvailability.h"
#include "HomePage.h"
#include "MainWindow.h"
#include "MainWindowCommon.h"

Vec<MainWindow*> gWindows;

bool MainWindow::HasDocsLoaded() const {
    int nTabs = TabCount();
    if (nTabs == 0) {
        // logf("HasDocsLoaded: false because nTabs == 0\n");
        return true;
    }
    for (int i = 0; i < nTabs; i++) {
        auto* tab = GetTab(i);
        if (!tab->IsAboutTab()) {
            // logf("HasDocsLoaded: true because GetTab(i) !IsAboutTab()\n");
            return true;
        }
    }
    // logf("HasDocsLoaded: false because all %d tabs are IsAboutTab()\n", nTabs);
    return false;
}

bool MainWindow::IsCurrentTabAbout() const {
    return nullptr == CurrentTab() || CurrentTab()->IsAboutTab();
}

DisplayModel* MainWindow::AsFixed() const {
    return ctrl ? ctrl->AsFixed() : nullptr;
}

ChmModel* MainWindow::AsChm() const {
    return ctrl ? ctrl->AsChm() : nullptr;
}

MarkdownModel* MainWindow::AsMarkdown() const {
    return ctrl ? ctrl->AsMarkdown() : nullptr;
}

bool MainWindow::InPresentation() const {
    return presentation != PM_DISABLED;
}

// Convert file:// / file:/// / file: URIs to a local path (+ optional #fragment).
// Returns false if uri is not a file: scheme.
bool PathFromFileUriTemp(Str uri, TempStr* pathOut, Str* fragmentOut) {
    Str rest = uri;
    if (!str::TrimPrefixI(rest, StrL("file:"))) {
        return false;
    }
    // file://host/path or file:///path → drop authority (// or ///)
    if (str::TrimPrefix(rest, StrL("//"))) {
        // empty host: next char is / of absolute path
        if (rest && rest.s[0] == '/') {
            // Windows drive path: /C:/foo → C:/foo
            if (rest.len >= 3 && rest.s[1] && rest.s[2] == ':') {
                rest = Str(rest.s + 1, rest.len - 1);
            }
        }
    }
    TempStr path = str::DupTemp(rest);
    Str pathStr = path;
    Str frag = str::SliceFromChar(pathStr, '#');
    if (frag) {
        pathStr = Str(pathStr.s, (int)(frag.s - pathStr.s));
        frag = Str(frag.s + 1, frag.len - 1);
    }
    path = url::DecodeTemp(pathStr);
    str::TransCharsInPlace(path, StrL("/"), StrL("\\"));
    *pathOut = path;
    if (fragmentOut) {
        *fragmentOut = frag ? str::DupTemp(frag) : Str{};
    }
    return true;
}

// return true if we can load the file based on sniffing file type from content
bool IsFileSupportedByContent(Str filePath) {
    FileType kindSniffed = GuessFileType(filePath, true);
    return IsSupportedFileType(kindSniffed, true);
}

// MuPDF encodes GoToR named destinations as "nameddest=<name>" in the link URI
// fragment, but EngineBase::GetNamedDest prepends "#nameddest=" itself -- so the
// prefix must be stripped or the lookup becomes "#nameddest=nameddest=<name>"
// and fails, leaving the remote PDF on page 1 (issue #5642).
// Strips mupdf's "nameddest=" prefix so the name can be passed to GetNamedDest.
void CleanRemoteDestNameInPlace(Str& destName) {
    str::TrimPrefixI(destName, StrL("nameddest="));
}

// normalizes case and whitespace in the string
TempStr NormalizeFuzzyTemp(Str str) {
    TempStr dup = str::DupTemp(str);
    str::ToLowerInPlace(dup);
    str::NormalizeWSInPlace(dup);
    // cf. AddTocItemToView
    return dup;
}

bool MatchFuzzy(Str s1, Str s2, bool partially) {
    if (!partially) {
        return str::Eq(s1, s2);
    }

    // only match at the start of a word (at the beginning and after a space)
    Str rest = s1;
    while (len(rest) > 0) {
        int idx = str::IndexOf(rest, s2);
        if (idx < 0) {
            break;
        }
        const char* found = rest.s + idx;
        if (found == s1.s || *(found - 1) == ' ') {
            return true;
        }
        int off = idx + 1;
        rest.s += off;
        rest.len -= off;
    }
    return false;
}

bool HasOpenedDocuments(MainWindow* win) {
    for (WindowTab* t : win->Tabs()) {
        if (!t->IsAboutTab()) {
            return true;
        }
    }
    return false;
}

BuildMenuCtx* NewBuildMenuCtx(WindowTab* tab, Point pt) {
    auto* ctx = new AppCommandCtx;
    if (tab && tab->win) {
        *ctx = NewAppCommandCtx(tab->win, pt);
    } else if (tab) {
        ctx->tab = tab;
    }
    return ctx;
}

void DeleteBuildMenuCtx(BuildMenuCtx* ctx) {
    delete ctx;
}

// True if `win` is still in gWindows (the object has not been deleted).
// Does not look at isBeingClosed: CloseWindow sets that flag first and then
// pumps messages (save-annotations dialog, ShowWindow), using this to detect
// whether the window was destroyed during that pumping. Folding isBeingClosed
// in here would make CloseWindow abort immediately after setting the flag.
bool IsMainWindowValid(MainWindow* win) {
    return win && VecContains(gWindows, win);
}

// True if `win` still exists and CloseWindow has not started. Use this for
// deferred work (load finish, timers, find/print threads, UI updates) that
// must not touch a window that is tearing down.
bool IsMainWindowValidAndNotClosing(MainWindow* win) {
    return IsMainWindowValid(win) && !win->isBeingClosed;
}

bool IsWindowTabValid(WindowTab* tab) {
    return FindMainWindowByTab(tab) != nullptr;
}

void LinkHandler::GoToPage(int pageNo, bool addNavPoint) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return;
    }
    win->ctrl->GoToPage(pageNo, addNavPoint);
}

bool LinkHandler::GoToNextPage() {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return false;
    }
    return win->ctrl->GoToNextPage();
}

bool LinkHandler::GoToPrevPage(bool toBottom) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return false;
    }
    return win->ctrl->GoToPrevPage(toBottom);
}

void LinkHandler::ScrollTo(int pageNo, RectF rect, float zoom) {
    ReportIf(!win || !win->ctrl || win->linkHandler != this);
    if (!win || !win->ctrl || !win->IsDocLoaded()) {
        return;
    }
    win->ctrl->ScrollTo(pageNo, rect, zoom);
    ShowLinkDestHighlight(win, pageNo, rect);
}

void LinkHandler::LaunchURL(Str uri) {
    if (len(uri) == 0) {
        /* ignore missing URLs */;
        return;
    }

    TempStr path = str::DupTemp(uri);
    int colon = str::IndexOfChar(path, ':');
    int hash = str::IndexOfChar(path, '#');
    if (colon < 0 || (hash >= 0 && colon > hash)) {
        // treat relative URIs as file paths (without fragment identifier)
        if (hash >= 0) {
            path.len = hash;
        }
        str::TransCharsInPlace(path, StrL("/"), StrL("\\"));
        path = url::DecodeTemp(path);
        // LaunchFile will reject unsupported file types
        this->LaunchFile(path, nullptr);
        return;
    }

    // file://... → open as a local document (or explorer if unsupported)
    TempStr filePath;
    Str fragment;
    if (PathFromFileUriTemp(uri, &filePath, &fragment)) {
        if (len(fragment) > 0) {
            // Carry destination name for LaunchFile scroll-to (named dest / page)
            PageDestinationFile dest(filePath, fragment);
            this->LaunchFile(filePath, &dest);
        } else {
            this->LaunchFile(filePath, nullptr);
        }
        return;
    }

    // LaunchBrowser will reject unsupported URI schemes
    SumatraLaunchBrowser(path);
}

// finds the first ToC entry that (partially) matches a given normalized name
// (ignoring case and whitespace differences)
TocItem* LinkHandler::FindTocItem(TocItem* item, Str name, bool partially) {
    for (; item; item = item->next) {
        if (item->title) {
            TempStr fuzTitle = NormalizeFuzzyTemp(item->title);
            if (MatchFuzzy(fuzTitle, name, partially)) {
                return item;
            }
        }
        TocItem* found = FindTocItem(item->child, name, partially);
        if (found) {
            return found;
        }
    }
    return nullptr;
}
