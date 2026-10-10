/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// `-dbg-control <pipe>`: the automation channel orig drives its scripted tests
// with. The wire protocol (length-prefixed packets of typed arguments over a
// named pipe), the command numbering and the request loop are orig's, so the
// same client talks to both.
//
// ng: orig answers 100 commands, most of them through helpers in
// SumatraTest.cpp / the win32 layout probes, neither of which this port has
// (the matrix drops SumatraTest.cpp). Ported here are the commands whose
// answer this port can give - which is what finally calls the `*ResultTemp`
// hooks earlier steps added and left without a caller. The numbering is orig's
// so the rest can be filled in later; an unimplemented one answers
// "NOTPORTED <n>" instead of silently doing nothing.

#include "base/Base.h"
#include "base/Pixmap.h"
#include "base/UITask.h"
#include "base/File.h"
#include "base/GuessFileType.h"
#if OS_WIN
#include "base/Win.h"
#elif !OS_WASM
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <errno.h>
#endif
#include "base/CrashHandler.h"

#include "gui/UIModels.h"
#include "ProgressUpdateUI.h"

#include "Settings.h"
#include "DisplayMode.h"
#include "DocumentLayout.h"
#include "DocController.h"
#include "MarkdownModel.h"
#include "gui/BrowserView.h"
#include "DocProperties.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "Commands.h"
#include "CommandPalette.h"
#include "CommandAvailability.h"
#include "Menu.h"
#include "AppSettings.h"
#include "Flags.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "HomePage.h"
#include "TextSelection.h"
#include "Selection.h"
#include "SelectionHandlers.h"
#include "TextSearch.h"
#include "FileHistory.h"
#include "Favorites.h"
#include "PagePosition.h"
#include "SelectionTranslate.h"
#include "SearchAndDDE.h"
#include "PdfSync.h"
#include "FindBar.h"
#include "FindWindow.h"
#include "LinkFollow.h"
#include "SelectTextKeyboard.h"
#include "Notifications.h"
#include "AIChatCommon.h"
#include "EutlTrust.h"
#include "PdfTools.h"
#include "Tabs.h"
#include "Toolbar.h"
#include "gui/AppShell.h"
#include "gui/ToolWindowPlat.h"
#include "gui/ToolWindow.h"
#include "gui/NativeFileDlg.h"
#include "gui/NativeMsgBox.h"
#include "gui/DocCanvas.h"
#include "gui/Sidebar.h"
#if OS_WIN
#include "gui/Dpi.h"
#include "gui/PlatformFont.h"
#endif
#include "SumatraDialogs.h"
#include "DocumentProperties.h"
#include "NavFilesInFolder.h"
#include "EngineAll.h"
#include "PdfCad.h"
#include <mupdf/pdf.h>
#include "base/ByteReaderWriter.h"
#include "PdfCreator.h"
#include "ImageSaveCropResize.h"
#include "ImageReader.h"
extern "C" {
#include <mupdf/fitz.h>
}
#include "Annotation.h"
#include "AnnotEditToolbar.h"
#include "AnnotPlacement.h"
#include "AnnotFilterToolbar.h"
#include "SelectionToolbar.h"
#include "RefHover.h"
#include "PerfLog.h"
#include "ReadAloud.h"
#include "ReadingAutoScroll.h"
#include "ReadingBar.h"
#include "gui/OleDragDrop.h"
#include "gui/NativeCursors.h"
#include "SumatraControl.h"

#include "SumatraLog.h"

// A BGRA8 heap pixmap is readable. A GDI DIB is readable only after a copy.
static Pixmap* EnsureReadablePixmap(Pixmap* p) {
    if (!p) {
        return nullptr;
    }
    if (p->data && p->format == PixmapFormat::BGRA8) {
        return p;
    }
#if OS_WIN
    return PixmapCopyAs32bppDIB(p);
#else
    return nullptr;
#endif
}

static void AppendLayoutRect(str::Builder& out, Str name, bool visible, Rect rect) {
    out.Append(
        fmt("item name=%s visible=%d rect=%d,%d,%d,%d\n", name, visible ? 1 : 0, rect.x, rect.y, rect.dx, rect.dy));
}

// ng: the channel needs a listener thread, which wasm does not have
#if !OS_WASM

// action: "add" | "goto" | "goto-fav" | "next" | "prev" | "page"
static TempStr FavoriteNavResultTemp(Str action, int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    if (str::EqI(action, StrL("add"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        // ng: orig has a silent variant that skips the name dialog;
        // ApplyAddFavorite is that path here
        TempStr label = win->ctrl->GetPageLabeTemp(pageNo);
        ApplyAddFavorite(win, win->ctrl->GetFilePath(), pageNo, label, {});
    } else if (str::EqI(action, StrL("goto"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        win->ctrl->GoToPage(pageNo, true);
    } else if (str::EqI(action, StrL("goto-fav"))) {
        if (!win->ctrl->ValidPageNo(pageNo)) {
            return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
        }
        FileState* fs = FileHistoryFindByPath(win->ctrl->GetFilePath());
        Favorite* fav = nullptr;
        if (fs && fs->favorites) {
            for (Favorite* f : *fs->favorites) {
                if (ParseStoredPagePos(f->pageNo).pageNo == pageNo) {
                    fav = f;
                    break;
                }
            }
        }
        if (!fav) {
            return finish(fmt("ERROR no-fav page=%d", pageNo), 1);
        }
        JumpToFavorite(win, fav);
    } else if (str::EqI(action, StrL("next"))) {
        GoToNextFavorite(win, true);
    } else if (str::EqI(action, StrL("prev"))) {
        GoToNextFavorite(win, false);
    } else if (str::EqI(action, StrL("page"))) {
        // report only
    } else if (str::EqI(action, StrL("menu"))) {
        return finish(FavoritesMenuIdsTemp(win), 0);
    } else {
        return finish(fmt("ERROR unknown-action action=%s", action), 1);
    }

    int cur = win->ctrl->CurrentPageNo();
    int y = -1;
    DisplayModel* dm = win->AsFixed();
    if (dm) {
        ScrollState ss = dm->GetScrollState();
        y = (int)ss.y;
        cur = ss.page;
    }
    return finish(fmt("OK page=%d y=%d", cur, y), 0);
}

// action: "get" | "r2l" | "presentation" | "fullscreen"
// Reports the current page layout and whether presentation / windowed
// fullscreen is on. presentation/fullscreen toggle that mode first.
static TempStr DisplayModeResultTemp(Str action, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    bool reportR2L = str::EqI(action, StrL("r2l"));
    if (str::EqI(action, StrL("zoom-real"))) {
        return finish(fmt("OK zoomReal=%g", win->ctrl->GetZoomVirtual(true)), 0);
    }
    if (len(action) == 0 || str::EqI(action, StrL("get")) || reportR2L) {
        // report only
    } else if (str::EqI(action, StrL("presentation"))) {
        // ng: orig calls ToggleFullScreen() directly; the port routes both
        // through their commands
        ExecuteCmd(win, CmdTogglePresentationMode);
    } else if (str::EqI(action, StrL("fullscreen"))) {
        ExecuteCmd(win, CmdToggleFullscreen);
    } else {
        return finish(fmt("ERROR unknown-action action=%s", action), 1);
    }

    if (reportR2L) {
        DisplayModel* dm = win->AsFixed();
        if (!dm) {
            return finish(StrL("ERROR not-fixed-page"), 1);
        }
        AppCommandCtx ctx = NewAppCommandCtx(win);
        bool available =
            GetCommandVisibility(CmdToggleMangaMode, ctx, CommandSurface::Palette) == CommandVisibility::Show;
        return finish(fmt("OK r2l=%d available=%d", dm->GetDisplayR2L() ? 1 : 0, available ? 1 : 0), 0);
    }

    Str mode = DisplayModeToString(win->ctrl->GetDisplayMode());
    Str zoomLabel;
    ZoomToString(&zoomLabel, win->ctrl->GetZoomVirtual(false), nullptr);
    TempStr res = fmt("OK mode=%s presentation=%d fullscreen=%d zoom=%s", mode, win->InPresentation() ? 1 : 0,
                      win->isFullScreen ? 1 : 0, zoomLabel);
    str::Free(zoomLabel);
    return finish(res, 0);
}

// document order, the target-th (1-based) outline item that has a destination
static IPageDestination* NthDestInToc(TocItem* item, int target, int& counter) {
    for (; item; item = item->next) {
        if (item->dest) {
            counter++;
            if (counter == target) {
                return item->dest;
            }
        }
        IPageDestination* d = NthDestInToc(item->child, target, counter);
        if (d) {
            return d;
        }
    }
    return nullptr;
}

void GoToTocItem(MainWindow*, TocItem*);

static TocItem* NthTocItemWithDest(TocItem* item, int target, int& counter) {
    for (; item; item = item->next) {
        if (item->dest) {
            counter++;
            if (counter == target) {
                return item;
            }
        }
        TocItem* found = NthTocItemWithDest(item->child, target, counter);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

// destNo > 0 starts navigation to that markdown TOC item. destNo == 0 reports
// whether the webview has reached minScrollY. tests/issue-5842.ts.
static TempStr MarkdownTocNavigateResultTemp(int destNo, int minScrollY, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    MarkdownModel* mm = win->ctrl ? win->ctrl->AsMarkdown() : nullptr;
    if (!mm || !mm->docView) {
        return finish(StrL("NOTREADY no-markdown-webview"), 2);
    }

    if (destNo > 0) {
        TocTree* toc = mm->GetToc();
        int counter = 0;
        TocItem* item = toc && toc->root ? NthTocItemWithDest(toc->root, destNo, counter) : nullptr;
        if (!item) {
            // headings are filled in on a background thread; the files-only
            // stub TOC is installed first, so dest 3 may not exist yet
            return finish(fmt("NOTREADY no-dest destNo=%d", destNo), 2);
        }
        GoToTocItem(win, item);
        return finish(fmt("NAVIGATING dest=%d name=%s", destNo, item->dest->GetName()), 0);
    }

    Point pos = BrowserViewGetScrollPos(mm->docView);
    if (pos.y < minScrollY) {
        return finish(fmt("NOTREADY scrollY=%d min=%d", pos.y, minScrollY), 2);
    }
    return finish(fmt("OK scrollX=%d scrollY=%d", pos.x, pos.y), 0);
}

// href is what the browser reports. follow == false only lists tabs.
// tests/issue-5924.ts.
static TempStr MarkdownFollowLinkResultTemp(Str href, bool follow, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }

    int navigate = -1;
    if (follow) {
        MarkdownModel* mm = win->ctrl ? win->ctrl->AsMarkdown() : nullptr;
        if (!mm) {
            return finish(StrL("NOTREADY no-markdown"), 2);
        }
        if (len(href) == 0) {
            return finish(StrL("ERROR no-href"), 1);
        }
        navigate = mm->OnBeforeNavigate(href, false) ? 1 : 0;
    }
    out.Append(fmt("OK navigate=%d\n", navigate));
    for (int i = 0; i < len(gWindows); i++) {
        MainWindow* w = gWindows[i];
        for (WindowTab* tab : w->Tabs()) {
            int isCurrent = tab == w->CurrentTab() ? 1 : 0;
            int pageNo = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
            out.Append(fmt("tab win=%d current=%d pageNo=%d file=%s\n", i, isCurrent, pageNo, tab->filePath));
        }
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// first [start, end) glyph range of `word` on a page
static bool FindWordGlyphRange(EngineBase* engine, int pageNo, Str word, int* startOut, int* endOut) {
    if (!engine || len(word) == 0 || !startOut || !endOut) {
        return false;
    }
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen);
    if (len(text) == 0) {
        return false;
    }
    int wordLen = Utf8CodepointCount(word);
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (str::Eq(Utf8SliceByCodepoints(text, i, wordLen), word)) {
            *startOut = i;
            *endOut = i + wordLen;
            return true;
        }
    }
    return false;
}

// tests/issue-find-match-select.ts: GoToFindMatch must scroll to the match
// and keep it as the current find result when the typed text differs in case.
static TempStr GoToFindMatchResultTemp(Str word, Str typed, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word) || str::IsEmptyOrWhiteSpace(typed)) {
        return fail(StrL("ERROR missing word or typed"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    int pageNo = 0;
    int startGlyph = 0, endGlyph = 0;
    for (int p = 1; p <= engine->PageCount(); p++) {
        if (FindWordGlyphRange(engine, p, word, &startGlyph, &endGlyph)) {
            pageNo = p;
            break;
        }
    }
    if (pageNo == 0) {
        return fail(StrL("ERROR word-not-found"));
    }

    dm->textSearch->SetText(typed);
    win->ctrl->GoToPage(1, false);
    DeleteOldSelectionInfo(win, true);
    GoToFindMatch(win, pageNo, startGlyph, pageNo, endGlyph);

    TextSearch* ts = dm->textSearch;
    int curPage = ts->startPage;
    int curStart = ts->startGlyph;
    int curEnd = ts->endGlyph;

    TempStr matched;
    Rect* coords = nullptr;
    int pageTextLen = 0;
    Str pageTxt = engine->GetTextForPage(pageNo, &pageTextLen, &coords);
    if (pageTxt && coords && curPage == pageNo && curStart >= 0 && curEnd <= pageTextLen && curStart < curEnd) {
        matched = Utf8SliceByCodepoints(pageTxt, curStart, curEnd - curStart);
    }

    bool visible = false;
    if (coords && curPage == pageNo && curStart >= 0 && curEnd <= pageTextLen && curStart < curEnd) {
        Rect pr = coords[curStart];
        for (int i = curStart + 1; i < curEnd; i++) {
            pr = pr.Union(coords[i]);
        }
        Rect sr = dm->CvtToScreen(pageNo, ToRectF(pr));
        Rect vp = Rect(Point(), dm->viewPort.Size());
        visible = !vp.Intersect(sr).IsEmpty();
    }

    bool hasResult = len(ts->result) > 0;
    bool matchOk = (curPage == pageNo) && (curStart == startGlyph) && (curEnd == endGlyph) && str::Eq(matched, word);
    bool ok = matchOk && visible && hasResult;
    if (ok) {
        out.Append(fmt("OK match=%s page=%d visible=1 highlighted=1\n", matched, pageNo));
    } else {
        out.Append(fmt("FAIL expected=%s match=%s page=%d visible=%d highlighted=%d\n", word,
                       matched ? matched : StrL("(none)"), pageNo, visible ? 1 : 0, hasResult ? 1 : 0));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// orig's TocNavigateResultTemp: follow one outline dest and report the page
static TempStr TocNavigateResultTemp(int destNo, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return fail(StrL("NOTREADY not-fixed"), 2);
    }
    EngineBase* engine = dm->GetEngine();
    TocTree* toc = engine ? engine->GetToc() : nullptr;
    if (!toc || !toc->root) {
        return fail(StrL("ERROR no-toc"), 1);
    }
    int counter = 0;
    IPageDestination* dest = NthDestInToc(toc->root, destNo, counter);
    if (!dest) {
        return fail(fmt("ERROR no-dest destNo=%d", destNo), 1);
    }
    int expectPage = PageDestGetPageNo(dest);
    if (expectPage <= 0 && dest->loc.chapter >= 1) {
        Location loc = win->ctrl->ResolveDest(dest);
        expectPage = win->ctrl->PageNoFromLocation(loc);
    }
    if (expectPage <= 0) {
        return fail(fmt("ERROR bad-dest-page destNo=%d page=%d", destNo, expectPage), 1);
    }

    // scroll away from page 1 so a leftover offset would land on the wrong page
    if (dm->PageCount() >= 2 && expectPage != 1) {
        dm->GoToPage(1, 0, false);
        dm->ScrollYBy(dm->viewPort.dy / 3, false);
    }

    win->ctrl->HandleLink(dest, win->linkHandler);

    int landed = dm->CurrentPageNo();
    bool ok = landed == expectPage;
    out.Append(fmt("%s dest=%d expect=%d landed=%d\n", ok ? StrL("OK") : StrL("FAIL"), destNo, expectPage, landed));
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// Select the text of one page so a test can turn it into a highlight.
static TempStr SeedTextSelectionResultTemp(int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm || !dm->textSelection) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    WindowTab* tab = win->CurrentTab();
    if (!tab) {
        return fail(StrL("ERROR no-tab"), 1);
    }
    if (!dm->ValidPageNo(pageNo)) {
        return fail(fmt("ERROR invalid-page pageNo=%d pageCount=%d", pageNo, dm->PageCount()), 1);
    }

    int textLen = 0;
    dm->GetEngine()->GetTextForPage(pageNo, &textLen);
    if (textLen < 2) {
        return fail(fmt("ERROR no-text pageNo=%d", pageNo), 1);
    }

    DeleteOldSelectionInfo(win, true);
    dm->textSelection->StartAt(pageNo, 0);
    dm->textSelection->SelectUpTo(pageNo, textLen - 1);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;
    if (!tab->selectionOnPage) {
        return fail(fmt("ERROR empty-selection pageNo=%d", pageNo), 1);
    }

    int first = (*tab->selectionOnPage)[0].pageNo;
    int last = VecLast(*tab->selectionOnPage).pageNo;
    int quads = 0;
    for (SelectionOnPage& sel : *tab->selectionOnPage) {
        if (sel.HasQuad()) {
            quads++;
        }
    }
    out.Append(fmt("OK parts=%d quads=%d first=%d last=%d pageCount=%d\n", len(*tab->selectionOnPage), quads, first,
                   last, dm->PageCount()));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    AppShellInvalidate(win);
    return ToStrTemp(out);
}

// Boxes the current page actually declares (issue #814). Optional int arg is pageNo.
static TempStr PageBoxesResultTemp(int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded() || !win->ctrl) {
        return finish(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("ERROR not-fixed-page"), 1);
    }
    if (pageNo < 1) {
        pageNo = win->ctrl->CurrentPageNo();
    }
    if (!win->ctrl->ValidPageNo(pageNo)) {
        return finish(fmt("ERROR bad-page page=%d", pageNo), 1);
    }
    Vec<PdfPageBox> boxes;
    engine->GetPdfPageBoxes(pageNo, boxes);
    str::Builder line;
    line.Append(fmt("OK page=%d show=%d", pageNo, win->showPageBoxes ? 1 : 0));
    for (const PdfPageBox& box : boxes) {
        line.Append(fmt(" %s=%.2f,%.2f,%.2f,%.2f", Str(PdfPageBoxName(box.kind)), box.rect.x, box.rect.y, box.rect.dx,
                        box.rect.dy));
    }
    return finish(ToStrTemp(line), 0);
}

static bool FindWordCenter(EngineBase* engine, int pageNo, Str word, double* xOut, double* yOut) {
    if (!engine || len(word) == 0 || !xOut || !yOut) {
        return false;
    }
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (len(text) == 0) {
        return false;
    }
    int wordLen = Utf8CodepointCount(word);
    if (wordLen <= 0) {
        return false;
    }
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (!str::Eq(Utf8SliceByCodepoints(text, i, wordLen), word)) {
            continue;
        }
        int mid = i + (wordLen / 2);
        int midByte = Utf8CodepointToByteIndex(text, mid);
        for (; mid < textLen && !coords[mid].x && !coords[mid].dx; mid++) {
            int nextByte = midByte;
            if (Utf8CodepointNext(text, nextByte) == '\n') {
                return false;
            }
            midByte = nextByte;
        }
        if (mid >= textLen) {
            return false;
        }
        *xOut = coords[mid].x + (coords[mid].dx / 2.0);
        *yOut = coords[mid].y + (coords[mid].dy / 2.0);
        return true;
    }
    return false;
}

// Opening the context menu over text must not move an existing selection.
static TempStr ContextMenuSelectionResultTemp(Str word1, Str word2, Str cursorWord, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word1) || str::IsEmptyOrWhiteSpace(word2) || str::IsEmptyOrWhiteSpace(cursorWord)) {
        return fail(StrL("ERROR missing word1, word2 or cursorWord"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    double x1 = 0, y1 = 0, x2 = 0, y2 = 0, xc = 0, yc = 0;
    if (!FindWordCenter(engine, pageNo, word1, &x1, &y1)) {
        return fail(StrL("ERROR word1-not-found"));
    }
    if (!FindWordCenter(engine, pageNo, word2, &x2, &y2)) {
        return fail(StrL("ERROR word2-not-found"));
    }
    if (!FindWordCenter(engine, pageNo, cursorWord, &xc, &yc)) {
        return fail(StrL("ERROR cursorWord-not-found"));
    }

    dm->textSelection->StartAt(pageNo, x1, y1);
    dm->textSelection->SelectUpTo(pageNo, x2, y2);
    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;

    bool isTextOnly = false;
    TempStr original = GetSelectedTextTemp(tab, StrL(" "), isTextOnly);
    if (len(original) == 0) {
        return fail(StrL("ERROR empty-selection"));
    }
    original = str::DupTemp(original);

    Point screenPt = dm->CvtToScreen(pageNo, PointF((float)xc, (float)yc));
    ReadAloudCanReadFromCursor(dm, screenPt);

    TempStr after = GetSelectedTextTemp(tab, StrL(" "), isTextOnly);
    bool ok = str::Eq(original, after);
    if (ok) {
        out.Append(fmt("OK selected=%s\n", original));
    } else {
        out.Append(fmt("FAIL original=%s after=%s\n", original, after));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// a spot on pageNo with no text under it, in canvas pixels
static Point FindEmptySpotOnPage(MainWindow* win, DisplayModel* dm, int pageNo) {
    Rect client = Rect(0, 0, win->canvasRc.dx, win->canvasRc.dy);
    constexpr int kStep = 8;
    for (int y = client.y + kStep; y < client.y + client.dy; y += kStep) {
        for (int x = client.x + kStep; x < client.x + client.dx; x += kStep) {
            Point pt{x, y};
            if (dm->GetPageNoByPoint(pt) != pageNo) {
                continue;
            }
            if (dm->IsOverText(pt)) {
                continue;
            }
            if (dm->GetElementAtPos(pt, nullptr)) {
                continue;
            }
            return pt;
        }
    }
    return Point{};
}

// tests/issue-5881.ts: a click on empty page drops the text selection
static TempStr ClickClearsSelectionResultTemp(Str word, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word)) {
        return fail(StrL("ERROR missing word"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    const int pageNo = 1;
    double wx = 0, wy = 0;
    if (!FindWordCenter(dm->GetEngine(), pageNo, word, &wx, &wy)) {
        return fail(StrL("ERROR word-not-found"));
    }

    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win, true);
    dm->textSelection->StartAt(pageNo, wx, wy);
    dm->textSelection->SelectUpTo(pageNo, wx, wy);
    dm->textSelection->SelectWordAt(pageNo, wx, wy);
    tab->selectionOnPage = SelectionOnPage::FromTextSelect(&dm->textSelection->result);
    win->showSelection = tab->selectionOnPage != nullptr;

    bool isTextOnly = false;
    TempStr selected = str::DupTemp(GetSelectedTextTemp(tab, StrL(" "), isTextOnly));
    if (len(selected) == 0) {
        return fail(StrL("ERROR empty-selection"));
    }

    Point pt = FindEmptySpotOnPage(win, dm, pageNo);
    if (pt.IsEmpty()) {
        return fail(StrL("ERROR no-empty-spot"));
    }
    DocCanvasClick(win, pt.x, pt.y);

    TempStr after = GetSelectedTextTemp(tab, StrL(" "), isTextOnly);
    bool cleared = (len(after) == 0) && !win->showSelection;
    if (cleared) {
        out.Append(fmt("OK selected=%s cleared at %d,%d\n", selected, pt.x, pt.y));
    } else {
        out.Append(fmt("FAIL selected=%s still=%s showSelection=%d\n", selected, after, (int)win->showSelection));
    }
    if (exitCodeOut) {
        *exitCodeOut = cleared ? 0 : 1;
    }
    return ToStrTemp(out);
}

// inverse search: page point -> source file. tests/security-ghsa-jf4v-rw66-j4w2.ts.
static TempStr InverseSearchResultTemp(Str pdfPath, int pageNo, int x, int y) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        Synchronizer* sync = nullptr;
        int err = Synchronizer::Create(pdfPath, engine, &sync);
        if (err != PDFSYNCERR_SUCCESS || !sync) {
            out.Append(fmt("ERROR sync-create-failed err=%d\n", err));
        } else {
            Str srcfilepath;
            int line = 0, col = 0;
            Point pt(x, y);
            int ret = sync->DocToSource(pageNo, pt, srcfilepath, &line, &col);
            if (ret != PDFSYNCERR_SUCCESS) {
                out.Append(fmt("ERROR doctosource-failed err=%d\n", ret));
            } else {
                out.Append(fmt("ret=%d srcfile=%s line=%d col=%d\n", ret, srcfilepath, line, col));
            }
            str::Free(srcfilepath);
            delete sync;
        }
        SafeEngineRelease(&engine);
    }
    return ToStrTemp(out);
}

// search limited to a page range. tests/issue-5694.ts.
static TempStr FindPageRangeResultTemp(Str pdfPath, Str needle, int first, int last, Str spec, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
        return finish(1);
    }
    auto* ts = new TextSearch(engine);
    ts->SetDirection(TextSearch::Direction::Forward);
    ts->SetMatchCase(false);
    Vec<bool> allowed;
    if (spec) {
        if (!ParseFindPageRange(spec, engine->PageCount(), allowed)) {
            VecReset(allowed);
        }
    } else if (first > 0 || last > 0) {
        int lo = first > 0 ? first : 1;
        int hi = last > 0 ? last : ts->nPages;
        if (lo > hi) {
            std::swap(lo, hi);
        }
        VecResize(allowed, ts->nPages);
        for (int page = 1; page <= ts->nPages; page++) {
            allowed[page - 1] = page >= lo && page <= hi;
        }
    }
    ts->SetAllowedPages(allowed);
    int n = 0;
    Vec<TextSel>* sel = ts->FindFirst(ts->RestrictFirst(), needle);
    while (sel && len(*sel) > 0) {
        out.Append(fmt("page=%d\n", (*sel)[0].pageNo));
        n++;
        sel = ts->FindNext();
    }
    if (n == 0) {
        out.Append(fmt("NOTFOUND needle=%s first=%d last=%d\n", needle, first, last));
    }
    delete ts;
    SafeEngineRelease(&engine);
    return finish(0);
}

class TestPasswordUI : public PasswordUI {
    Str password;
    bool triedPassword = false;

  public:
    explicit TestPasswordUI(Str password) : password(password) {}

    Str GetPassword(Str /*path*/, u8* /*fileDigest*/, u8 /*decryptionKeyOut*/[32], bool* saveKey) override {
        *saveKey = false;
        if (triedPassword || len(password) == 0) {
            return {};
        }
        triedPassword = true;
        return str::Dup(password);
    }
};

// case-insensitive search. tests/issue-933.ts.
static TempStr SearchResultTemp(Str pdfPath, Str needle, Str password) {
    str::Builder out;
    TestPasswordUI pwdUI(password);
    EngineBase* engine = CreateEngineFromFile(pdfPath, password ? &pwdUI : nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        auto* ts = new TextSearch(engine);
        ts->SetDirection(TextSearch::Direction::Forward);
        ts->SetMatchCase(false);
        Vec<TextSel>* sel = ts->FindFirst(1, needle);
        if (sel && len(*sel) > 0) {
            out.Append(fmt("FOUND needle=%s page=%d\n", needle, (*sel)[0].pageNo));
        } else {
            out.Append(fmt("NOTFOUND needle=%s\n", needle));
        }
        delete ts;
        SafeEngineRelease(&engine);
    }
    return ToStrTemp(out);
}

// the destNo-th outline destination as "page=P zoom=Z". zoom 0 retains the
// current zoom. tests/issue-5537.ts.
static TempStr DestResultTemp(Str pdfPath, int destNo) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        TocTree* toc = engine->GetToc();
        IPageDestination* dest = nullptr;
        if (toc && toc->root) {
            int counter = 0;
            dest = NthDestInToc(toc->root, destNo, counter);
        }
        if (dest) {
            out.Append(fmt("dest=%d page=%d zoom=%g\n", destNo, PageDestGetPageNo(dest), dest->GetZoom()));
        } else {
            out.Append(fmt("dest=%d NODEST\n", destNo));
        }
        SafeEngineRelease(&engine);
    }
    return ToStrTemp(out);
}

// named dest, including the "nameddest=" prefix a remote link carries.
// tests/issue-5642.ts.
static TempStr NamedDestResultTemp(Str pdfPath, Str destName) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        out.Append(fmt("ERROR engine-create-failed pdf=%s\n", pdfPath));
    } else {
        Str name = destName;
        CleanRemoteDestNameInPlace(name);
        IPageDestination* dest = engine->GetNamedDest(name);
        if (dest) {
            out.Append(fmt("name=%s page=%d\n", destName, PageDestGetPageNo(dest)));
        } else {
            out.Append(fmt("name=%s NOTFOUND\n", destName));
        }
        SafeEngineRelease(&engine);
    }
    return ToStrTemp(out);
}

// stamp an image onto page 1 and count red pixels. tests/issue-1744.ts.
static TempStr ImageInsertResultTemp(Str pdfPath, Str imagePath, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", pdfPath));
    }
    if (!EngineSupportsAnnotations(engine)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR annots-not-supported\n"));
    }
    Str data = file::ReadFile(imagePath);
    Pixmap* image = PixmapFromData(data);
    str::Free(data);
    if (!image) {
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR image-load-failed path=%s\n", imagePath));
    }
    if (!engine->BenchLoadPage(1)) {
        FreePixmap(image);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR page-load-failed\n"));
    }

    AnnotCreateArgs args{AnnotationType::Stamp};
    args.stampImage = image;
    Annotation* annot = EngineMupdfCreateAnnotation(engine, 1, PointF{72.f, 100.f}, &args);
    if (!annot) {
        TempStr msg = fmt("ERROR stamp-create-failed fmt=%d %dx%d\n", (int)image->format, image->width, image->height);
        FreePixmap(image);
        SafeEngineRelease(&engine);
        return fail(msg);
    }
    FreePixmap(image);

    Vec<Annotation*> annots;
    EngineGetAnnotations(engine, annots);
    int nAnnots = len(annots);
    RectF ar = GetRect(annot);
    out.Append(fmt("annot=%s rect=%g,%g,%g,%g\n", AnnotationReadableNameTemp(Type(annot)), ar.x, ar.y, ar.dx, ar.dy));

    RenderPageArgs rargs(1, 1.f, 0, nullptr, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(rargs);
    if (!bmp || !bmp->data) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* rgb = EnsureReadablePixmap(bmp);
    if (!rgb || !rgb->data) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-convert-failed fmt=%d\n", (int)bmp->format));
    }
    int bpp = PixmapBytesPerPixel(rgb->format);
    int red = 0;
    int nonWhite = 0;
    if (bpp >= 3) {
        for (int y = 0; y < rgb->height; y++) {
            const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
            for (int x = 0; x < rgb->width; x++) {
                const u8* px = row + ((size_t)x * bpp);
                int r, g, b;
                if (rgb->format == PixmapFormat::RGBA8) {
                    r = px[0];
                    g = px[1];
                    b = px[2];
                } else {
                    b = px[0];
                    g = px[1];
                    r = px[2];
                }
                if (r < 250 || g < 250 || b < 250) {
                    nonWhite++;
                }
                if (r > 180 && g < 80 && b < 80) {
                    red++;
                }
            }
        }
    }
    out.Append(fmt("annots=%d red=%d nonwhite=%d size=%dx%d\n", nAnnots, red, nonWhite, rgb->width, rgb->height));
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = (nAnnots >= 1 && red > 50) ? 0 : 1;
    }
    if (nAnnots < 1 || red <= 50) {
        out.Append(StrL("ERROR stamp-not-visible\n"));
    }
    return ToStrTemp(out);
}

static void AppendTocItems(str::Builder& out, TocItem* item, int depth = 0) {
    for (; item; item = item->next) {
        if (item->title) {
            for (int i = 0; i < depth; i++) {
                out.Append(StrL("  "));
            }
            out.Append(fmt("%s|page=%d\n", item->title, item->pageNo));
        }
        AppendTocItems(out, item->child, depth + 1);
    }
}

// one line per TOC entry: "title|page=N". tests/issue-1201.ts.
static TempStr GetTocResultTemp(Str path, int* exitCodeOut) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR engine-create-failed path=%s\n", path));
    } else {
        TocTree* toc = engine->GetToc();
        if (!toc || !toc->root || !toc->root->child) {
            if (exitCodeOut) {
                *exitCodeOut = 1;
            }
            out.Append(StrL("ERROR no-toc\n"));
        } else {
            if (exitCodeOut) {
                *exitCodeOut = 0;
            }
            AppendTocItems(out, toc->root->child);
        }
        SafeEngineRelease(&engine);
    }
    return ToStrTemp(out);
}

// render one page and count red / non-white pixels. tests/issue-3415.ts.
static TempStr PageRenderColorsResultTemp(Str path, int* exitCodeOut, int pageNo) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = nullptr;
    bool ownEngine = true;
    if (len(gWindows) > 0 && gWindows[0]) {
        WindowTab* tab = gWindows[0]->CurrentTab();
        if (tab && tab->filePath && str::EqI(tab->filePath, path)) {
            DisplayModel* dm = tab->AsFixed();
            engine = dm ? dm->GetEngine() : nullptr;
            ownEngine = false;
        }
    }
    if (!engine) {
        engine = CreateEngineFromFile(path, nullptr, false);
    }
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    if (pageNo < 1) {
        pageNo = 1;
    }
    auto release = [&]() {
        if (ownEngine) {
            SafeEngineRelease(&engine);
        }
    };
    if (pageNo > engine->PageCount()) {
        int nPages = engine->PageCount();
        release();
        return fail(fmt("ERROR bad-page page=%d pages=%d\n", pageNo, nPages));
    }
    if (!engine->BenchLoadPage(pageNo)) {
        release();
        return fail(StrL("ERROR page-load-failed\n"));
    }

    RenderPageArgs rargs(pageNo, 1.f, 0, nullptr, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(rargs);
    if (!bmp || !bmp->data) {
        FreePixmap(bmp);
        int nPages = engine->PageCount();
        release();
        return fail(fmt("ERROR render-failed page=%d pages=%d\n", pageNo, nPages));
    }
    Pixmap* rgb = bmp;
    if (bmp->format != PixmapFormat::BGRA8 && bmp->format != PixmapFormat::BGR8 && bmp->format != PixmapFormat::RGBA8) {
        rgb = EnsureReadablePixmap(bmp);
    }
    if (!rgb || !rgb->data) {
        FreePixmap(bmp);
        release();
        return fail(fmt("ERROR pixmap-convert-failed fmt=%d\n", (int)bmp->format));
    }
    int bpp = PixmapBytesPerPixel(rgb->format);
    int red = 0;
    int blue = 0;
    int nonWhite = 0;
    int darkLeft = 0;
    int leftX = (rgb->width * 45) / 100;
    int rMin = 255, rMax = 0, gMin = 255, gMax = 0, bMin = 255, bMax = 0;
    if (bpp >= 3) {
        for (int y = 0; y < rgb->height; y++) {
            const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
            for (int x = 0; x < rgb->width; x++) {
                const u8* px = row + ((size_t)x * bpp);
                int r, g, b;
                if (rgb->format == PixmapFormat::RGBA8) {
                    r = px[0];
                    g = px[1];
                    b = px[2];
                } else {
                    b = px[0];
                    g = px[1];
                    r = px[2];
                }
                if (r < 250 || g < 250 || b < 250) {
                    nonWhite++;
                }
                if (r > 180 && g < 80 && b < 80) {
                    red++;
                }
                if (b > 180 && r < 80 && g < 80) {
                    blue++;
                }
                if (x < leftX && r < 80 && g < 80 && b < 80) {
                    darkLeft++;
                }
                if (r < rMin) {
                    rMin = r;
                }
                if (r > rMax) {
                    rMax = r;
                }
                if (g < gMin) {
                    gMin = g;
                }
                if (g > gMax) {
                    gMax = g;
                }
                if (b < bMin) {
                    bMin = b;
                }
                if (b > bMax) {
                    bMax = b;
                }
            }
        }
    }
    int spread = (rMax - rMin) + (gMax - gMin) + (bMax - bMin);
    out.Append(fmt("red=%d nonwhite=%d size=%dx%d pages=%d page=%d blue=%d spread=%d darkLeft=%d\n", red, nonWhite,
                   rgb->width, rgb->height, engine->PageCount(), pageNo, blue, spread, darkLeft));
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    if (ownEngine) {
        SafeEngineRelease(&engine);
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Color histogram of a page rendered with the CAD enhancement forced on.
// tests/issue-5937.ts.
static TempStr CadEnhanceColorsResultTemp(Str path, int pageNo, int zoomPercent, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    SetEngineeringDrawingEnhanceMode(StrL("on"));
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    if (pageNo < 1 || pageNo > engine->PageCount()) {
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR bad-page page=%d\n", pageNo));
    }
    if (!EngineMupdfCadEnhanceActive(engine)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR cad-enhance-not-active\n"));
    }

    float zoom = (float)zoomPercent / 100.f;
    RenderPageArgs args(pageNo, zoom, 0);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* rgb = EnsureReadablePixmap(bmp);
    if (!rgb) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR pixmap-convert-failed\n"));
    }

    int counts[256] = {};
    for (int y = 0; y < rgb->height; y++) {
        const u8* row = rgb->data + ((size_t)y * (size_t)rgb->stride);
        for (int x = 0; x < rgb->width; x++) {
            const u8* px = row + ((size_t)x * 4);
            if (px[0] == px[1] && px[1] == px[2]) {
                counts[px[0]]++;
            }
        }
    }
    out.Append(fmt("size=%dx%d\n", rgb->width, rgb->height));
    if (engine->HasErrors()) {
        out.Append(fmt("errors=%s\n", engine->GetErrorsTextTemp()));
    }
    for (int i = 0; i < 256; i++) {
        if (counts[i] >= 64) {
            out.Append(fmt("gray=%d count=%d\n", i, counts[i]));
        }
    }
    if (rgb != bmp) {
        FreePixmap(rgb);
    }
    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// tests/issue-5938.ts: follow an outline dest and report the zoom on both sides
static TempStr DestZoomNavResultTemp(int destNo, int startZoomPerc, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code = 1) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win || !win->IsDocLoaded()) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return fail(StrL("NOTREADY not-fixed"), 2);
    }
    IPageDestination* dest = nullptr;
    if (destNo > 0) {
        EngineBase* engine = dm->GetEngine();
        TocTree* toc = engine ? engine->GetToc() : nullptr;
        if (!toc || !toc->root) {
            return fail(StrL("ERROR no-toc"));
        }
        int counter = 0;
        dest = NthDestInToc(toc->root, destNo, counter);
        if (!dest) {
            return fail(fmt("ERROR no-dest destNo=%d", destNo));
        }
    }

    if (startZoomPerc > 0) {
        dm->SetZoomVirtual((float)startZoomPerc, nullptr);
    }
    float zoomBefore = dm->GetZoomVirtual();
    if (dest) {
        win->ctrl->HandleLink(dest, win->linkHandler);
    }
    float zoomAfter = dm->GetZoomVirtual();

    out.Append(fmt("OK dest=%d destZoom=%g page=%d landed=%d zoomBefore=%g zoomAfter=%g ignore=%d\n", destNo,
                   dest ? dest->GetZoom() : 0.f, dest ? PageDestGetPageNo(dest) : 0, dm->CurrentPageNo(), zoomBefore,
                   zoomAfter, gSettings->ignoreDestinationZoom ? 1 : 0));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// tests/rect-selection-drag.ts: a press in a rectangle over text grabs the rect
static TempStr RectSelectionDragResultTemp(Str word, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> Str {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word)) {
        return fail(StrL("ERROR missing word"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    const int pageNo = 1;
    double wx = 0, wy = 0;
    if (!FindWordCenter(dm->GetEngine(), pageNo, word, &wx, &wy)) {
        return fail(StrL("ERROR word-not-found"));
    }
    Point center = dm->CvtToScreen(pageNo, PointF((float)wx, (float)wy));

    WindowTab* tab = win->CurrentTab();
    DeleteOldSelectionInfo(win, true);
    int half = 40;
    Rect rc(center.x - half, center.y - (half / 2), half * 2, half);
    tab->selectionOnPage = SelectionOnPage::FromRectangle(dm, rc);
    win->showSelection = tab->selectionOnPage != nullptr;
    if (!win->showSelection) {
        return fail(StrL("ERROR no-rect-selection"));
    }
    if (!IsRectangularSelection(win)) {
        return fail(StrL("ERROR not-rectangular"));
    }
    if (!dm->IsOverText(center)) {
        return fail(StrL("ERROR press-point-not-over-text"));
    }

    DocCanvasMouseDown(win, center.x, center.y);
    SelectionDragEdge edge = win->selectionDragEdge;
    bool dragging = (win->mouseAction == MouseAction::Selecting) && (edge != SelectionDragEdge::None);
    bool textDrag = win->textDragPending;
    DocCanvasMouseUp(win, center.x, center.y);

    bool ok = dragging && !textDrag;
    if (ok) {
        out.Append(fmt("OK moving rect selection, edge=%d\n", (int)edge));
    } else {
        out.Append(
            fmt("FAIL edge=%d mouseAction=%d textDragPending=%d\n", (int)edge, (int)win->mouseAction, (int)textDrag));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

static TempStr ResolveUnsavedChangesResultTemp(Str action, Str path, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code = 1) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    if (str::Eq(action, StrL("save-as"))) {
        WindowTab* tab = gWindows[0]->CurrentTab();
        EngineBase* engine = tab ? tab->GetEngine() : nullptr;
        if (len(path) == 0 || !tab || !engine) {
            return fail(StrL("ERROR save-as needs a path and a document"));
        }
        if (EngineHasUnsavedAnnotations(engine) && !EngineMupdfSaveUpdated(engine, path, {})) {
            return fail(fmt("ERROR save-as '%s' failed", path));
        }
        if (exitCodeOut) {
            *exitCodeOut = 0;
        }
        out.Append(StrL("OK tabs=1\n"));
        return ToStrTemp(out);
    }

    bool discard = str::Eq(action, StrL("discard"));
    bool save = str::Eq(action, StrL("save"));
    if (!discard && !save) {
        return fail(fmt("ERROR unknown action '%s'", action));
    }
    int nTabs = 0;
    for (MainWindow* win : gWindows) {
        for (WindowTab* tab : win->Tabs()) {
            EngineBase* engine = tab ? tab->GetEngine() : nullptr;
            if (engine && EngineHasUnsavedAnnotations(engine)) {
                if (discard) {
                    tab->askedToSaveAnnotations = true;
                } else {
                    tab->ignoreNextAutoReload = true;
                    if (!EngineMupdfSaveUpdated(engine, {}, {})) {
                        return fail(fmt("ERROR save of '%s' failed", tab->filePath));
                    }
                }
            }
            nTabs++;
        }
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    out.Append(fmt("OK tabs=%d\n", nTabs));
    return ToStrTemp(out);
}

static TempStr ToggleFormButtonResultTemp(int pageNo, int idx, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code = 1) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    if (!dm->ValidPageNo(pageNo)) {
        return fail(fmt("ERROR invalid-page pageNo=%d pageCount=%d", pageNo, dm->PageCount()));
    }

    Vec<Annotation*> widgets;
    EngineMupdfGetPageWidgets(dm->GetEngine(), pageNo, widgets);
    Annotation* button = nullptr;
    int nButtons = 0;
    for (Annotation* w : widgets) {
        int wt = GetWidgetType(w);
        if (wt != PDF_WIDGET_TYPE_CHECKBOX && wt != PDF_WIDGET_TYPE_RADIOBUTTON) {
            continue;
        }
        if (nButtons == idx) {
            button = w;
        }
        nButtons++;
    }
    if (!button) {
        return fail(fmt("ERROR no-button idx=%d buttons=%d", idx, nButtons));
    }

    TempStr before = str::DupTemp(GetWidgetValue(button));
    bool toggled = ToggleFormButton(button);
    TempStr after = str::DupTemp(GetWidgetValue(button));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    out.Append(fmt("OK toggled=%d before='%s' after='%s' buttons=%d\n", (int)toggled, before, after, nButtons));
    return ToStrTemp(out);
}

static TempStr PageCommentsResultTemp(Str path, int pageNo, int* exitCodeOut) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR engine-create-failed path=%s\n", path));
        return ToStrTemp(out);
    }

    if (!engine->BenchLoadPage(pageNo)) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR page-load-failed page=%d\n", pageNo));
        SafeEngineRelease(&engine);
        return ToStrTemp(out);
    }

    int nComments = 0;
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (!el || !el->Is(kindPageElementComment)) {
            continue;
        }
        Str value = el->GetValue();
        TempStr flat = str::ReplaceTemp(value, StrL("\n"), StrL("|"));
        nComments++;
        out.Append(fmt("comment=%s\n", flat));
    }
    if (nComments == 0) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR no-comments page=%d\n", pageNo));
    } else if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    SafeEngineRelease(&engine);
    return ToStrTemp(out);
}

// SHA-1 thumbprints of CurrentUser\MY certs that can sign.
static TempStr ListSigningCertsResultTemp(int* exitCodeOut) {
    StrVec thumbs;
    StrVec labels;
    ListWindowsSigningCertificates(thumbs, labels);
    str::Builder out;
    out.Append(fmt("n=%d\n", len(thumbs)));
    for (int i = 0; i < len(thumbs); i++) {
        out.Append(fmt("thumb=%s\nlabel=%s\n", thumbs[i], labels[i]));
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// The dest is a copy so the signature can be written incrementally.
static TempStr SignDocumentResultTemp(Str pdfPath, Str destPath, Str thumbprint, Str certPath, Str certPassword,
                                      Str imagePath, int appearanceFlags, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    if (len(thumbprint) == 0 && len(certPath) == 0) {
        return fail(StrL("ERROR need thumbprint or certPath\n"));
    }
    if (!file::Exists(pdfPath)) {
        return fail(fmt("ERROR pdf-missing path=%s\n", pdfPath));
    }
    if (!file::Copy(destPath, pdfPath, false)) {
        return fail(fmt("ERROR copy-failed dest=%s\n", destPath));
    }

    EngineBase* engine = CreateEngineFromFile(destPath, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", destPath));
    }

    PdfSignArgs args;
    args.certThumbprint = thumbprint;
    args.certPath = certPath;
    args.certPassword = certPassword;
    args.imagePath = imagePath;
    args.appearanceFlags = appearanceFlags;
    args.pageNo = 1;
    StrVec fieldNames;
    Vec<int> fieldPages;
    EngineMupdfGetUnsignedSignatureFields(engine, fieldNames, fieldPages);
    if (len(fieldNames) > 0) {
        args.fieldName = fieldNames[0];
        args.pageNo = fieldPages[0];
    }

    Str err;
    bool ok = EngineMupdfSignDocument(engine, args, &err);
    if (!ok) {
        TempStr msg = fmt("ERROR sign-failed %s\n", err ? err : StrL("(no message)"));
        str::Free(err);
        SafeEngineRelease(&engine);
        return fail(msg);
    }
    str::Free(err);

    if (!EngineMupdfSaveUpdated(engine, destPath, {})) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR save-failed\n"));
    }
    SafeEngineRelease(&engine);
    out.Append(StrL("ok=1\n"));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

static TempStr DocumentSignaturesResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    EutlRegisterLookup();
    Props props;
    engine->GetProperties(props);
    Str sigs = GetPropValueTemp(props, DocProp::Signatures);
    if (len(sigs) == 0) {
        return finish(StrL("ERROR no-signatures"), 1);
    }
    return finish(str::DupTemp(sigs), 0);
}

static TempStr DocumentFontListResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    TempStr fonts = engine->GetPropertyTemp(DocProp::FontList);
    if (len(fonts) == 0) {
        return finish(StrL("ERROR no-fonts"), 1);
    }
    return finish(fmt("OK fonts=%s", fonts), 0);
}

static TempStr DocumentPropertiesResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](Str result, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return str::DupTemp(result);
    };
    if (len(gWindows) == 0) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-fixed-document"), 2);
    }
    Props props;
    engine->GetProperties(props);
    str::Builder out;
    out.Append(StrL("OK"));
    int n = len(props);
    for (int i = 0; i < n; i++) {
        TempStr name = PropNameTemp(props[i].prop);
        if (len(name) == 0) {
            continue;
        }
        out.Append(StrL("\n"));
        out.Append(name);
        out.Append(StrL("="));
        out.Append(props[i].val);
    }
    // what Save As offers, and the sniffed type Properties shows
    out.Append(fmt("\ndefaultExt=%s", engine->defaultExt));
    FileType ft = GuessFileTypeFromFile(engine->FilePath());
    out.Append(fmt("\nfileTypeExt=%s", GetExtForFileTypeTemp(ft)));
    return finish(ToStrTemp(out), 0);
}

enum class ControlCmd : u16 {
    Ping = 1,
    Quit = 2,
    TestSynctex = 10,
    TestSearch = 11,
    TestDest = 12,
    TestNamedDest = 13,
    TestChm = 14,
    TestSelectionTranslate = 15,
    TestTripleClickLineSelect = 16,
    TestContextMenuSelection = 17,
    TestGoToFindMatch = 18,
    // IDs 19-21 unused (reserved on the -dbg-control wire protocol; do not renumber).
    // Assign new test commands starting at 23.
    TestInverseSearch = 22,
    TestImageResizeArrowKey = 23,
    TestFindResultPageColumnClip = 24,
    TestFileKind = 25,
    TestScrollToLink = 26,
    TestI18nErrorString = 27,
    TestPageInfoOverlay = 28,
    TestGetToc = 29,
    TestPageLinks = 30,
    TestWindowStateDuringLoad = 31,
    TestTocNavigate = 32,
    TestMarkdownTocNavigate = 33,
    TestFavoriteNav = 34,
    TestToolbarButtons = 35,
    TestKeyboardLinkFollow = 36,
    TestFindResultsOrder = 37,
    TestClickClearsSelection = 38,
    TestRectSelectionDrag = 39,
    TestSelectTextKeyboard = 40,
    TestAIChat = 41,
    TestAIChatReplay = 42,
    TestMarkdownFollowLink = 43,
    TestHomeListRows = 44,
    TestPageComments = 45,
    TestAdvSettingsRows = 46,
    TestDestZoomNav = 47,
    TestAnnotEditorLayout = 48,
    TestDisplayMode = 49,
    TestSidebarLayout = 50,
    TestCadEnhanceColors = 51,
    TestFindPageRange = 52,
    TestDocumentFontList = 53,
    WaitRenderIdle = 54,
    SetNotificationsEnabled = 55,
    TestHomeSelection = 56,
    TestImageRenderEdges = 57,
    TestInsertImage = 58,
    TestRenderPageColors = 59,
    TestListSigningCerts = 60,
    TestSignDocument = 61,
    TestGetPolicies = 62,
    TestPageBoxes = 63,
    TestDocumentSignatures = 64,
    TestCommandPalette = 65,
    TestFindHistory = 66,
    TestImageResizeEdges = 67,
    TestLinkDestHighlight = 68,
    TestConvertToImages = 69,
    TestLayout = 70,
    TestDpi = 71,
    TestSelectionVars = 72,
    TestSelectionToolbar = 73,
    TestMarkupAnnots = 74,
    TestCmykImageSave = 75,
    TestContextMenuPoint = 76,
    TestFindWindowContents = 77,
    TestFindUiState = 78,
    TestRenderViewPrint = 79,
    TestReadAloudPlaybackBar = 80,
    TestRotatedTextMouseDrag = 81,
    TestChapterInfo = 82,
    TestGoToLocation = 83,
    TestTocSidebarNav = 84,
    TestSelectionSurvivesRenumber = 85,
    TestConvertToPdf = 86,
    TestInvokeCommand = 87,
    TestCurrentTab = 88,
    TestCommandVisibility = 89,
    TestExtractPages = 90,
    TestAnnotFilter = 91,
    TestCanvasFlags = 92,
    CrashMe = 93,
    TestDocumentProperties = 94,
    TestHiddenTabGoToPage = 95,
    TestSaveSelectionAsImage = 96,
    TestReadingAutoScroll = 97,
    TestReadingBar = 98,
    TestSeedTextSelection = 99,
    TestTtsEngineCrash = 100,
    TestTtsPumpOnSpeak = 107,
    TestRenderSelections = 108,
    TestToggleFormButton = 109,
    ResolveUnsavedChanges = 110,
    // orig's. 105 is TestSaveFileAs.
    TestImageOrientation = 106,
    // ng: not one of orig's; the performance snapshot cmd/port-perf.ts reads.
    // Orig's 101 / 102 are StartPerfLog / StopPerfLog, which this port answers
    // at 119 / 120.
    TestPerfStats = 101,
    // ng: not one of orig's; the state of the six canvas overlays
    TestOverlayState = 102,
    // orig's. The session restore the tests wait on.
    WaitSessionRestored = 103,
    TestNavFiles = 104,
    TestRefHover = 111,
    // orig's. Page widths and bookmark targets.
    TestPageInfo = 112,
    // orig's. The thumbnail pane: hwnd, highlighted page, cell rects.
    TestSidebarThumbnails = 113,
    // orig's. Maximized, and the non-client strips WM_NCPAINT would fill.
    TestFrameNcStrips = 114,
    TestMergePdf = 115,
    // orig's. A wheel while CloseWindow is in progress.
    TestWheelWhileClosing = 116,
    TestMainMenu = 117,
    // ng: shows the "no longer the default app" bar for the given extensions
    // (".pdf,.epub"); the real check needs an installation and a UserChoice
    TestDefaultAppNotif = 118,
    // ng: orig's 101 / 102, which the port had given to its own test commands
    StartPerfLog = 119,
    StopPerfLog = 120,
    // ng: not orig's; gpui input with modifiers / hover / a held button, and
    // one line of shell state to check it against
    TestInput = 121,
    TestUiState = 122,
    // ng: Windows; exercises the OLE drag source / drop target in-process
    TestOleDragDrop = 123,
    // ng: Windows; the system file dialogs: off / on / state / cancel / accept
    TestNativeFileDlg = 124,
    // ng: not one of orig's; the tool windows (gui/ToolWindow.h) and input
    // into one of them
    TestToolWindow = 125,
    // ng: Windows; the system message boxes: on / off / state / answer <id>
    TestNativeMsgBox = 126,
    // ng: the page context menu at a canvas point, same rows as TestMainMenu
    TestContextMenuAt = 127,
    // ng: not one of orig's; answers the open save-path dialog, which orig
    // does not have (it uses GetSaveFileNameW, which no script can drive).
    // 103 is WaitSessionRestored.
    TestSavePathDialog = 128,
    // ng: DDE execute grammar ([GotoPageWord], [Open], ...). Orig receives
    // these as WM_COPYDATA; macOS has no such message.
    TestDdeExecute = 129,
};

enum class ControlArgType : u16 {
    End = 0,
    Int32 = 1,
    Bytes = 2,
    String = 3,
    List = 4,
};

struct ControlArg {
    ControlArgType type = ControlArgType::End;
    i32 intVal = 0;
    u8* bytes = nullptr;
    u32 bytesLen = 0;
    Str str;
    Vec<ControlArg*>* list = nullptr;
};

static void DeleteControlArg(ControlArg* arg) {
    if (!arg) {
        return;
    }
    free(arg->bytes);
    str::FreePtr(&arg->str);
    if (arg->list) {
        for (ControlArg* el : *arg->list) {
            DeleteControlArg(el);
        }
        delete arg->list;
    }
    delete arg;
}

enum class RenderIdleState : u8 {
    NotReady = 0,
    Busy = 1,
    Idle = 2,
};

// ng: orig's manual-reset event (CreateEventW), from the portable primitives
struct DoneEvent {
    Mutex mutex;
    ConditionVariable cond;
    bool isSet = false;

    void Set() {
        mutex.Lock();
        isSet = true;
        mutex.Unlock();
        cond.WakeAll();
    }
    void Reset() {
        mutex.Lock();
        isSet = false;
        mutex.Unlock();
    }
    void Wait() {
        mutex.Lock();
        while (!isSet) {
            cond.Wait(&mutex);
        }
        mutex.Unlock();
    }
};

struct ControlRequest {
    u16 cmd = 0;
    u16 reqId = 0;
    Vec<ControlArg*> args;
    str::Builder results;
    DoneEvent done;
    RenderIdleState idleState = RenderIdleState::NotReady;
    char idleInfo[320]{};
};

static void DeleteControlRequest(ControlRequest* req) {
    if (!req) {
        return;
    }
    for (ControlArg* arg : req->args) {
        DeleteControlArg(arg);
    }
    delete req;
}

struct PacketReader {
    const u8* data = nullptr;
    size_t size = 0;
    size_t pos = 0;

    bool ReadU16(u16& v) {
        if (pos + 2 > size) {
            return false;
        }
        v = (u16)(data[pos] | (data[pos + 1] << 8));
        pos += 2;
        return true;
    }

    bool ReadU32(u32& v) {
        if (pos + 4 > size) {
            return false;
        }
        v = (u32)data[pos] | ((u32)data[pos + 1] << 8) | ((u32)data[pos + 2] << 16) | ((u32)data[pos + 3] << 24);
        pos += 4;
        return true;
    }

    bool ReadBytes(u8* dst, size_t n) {
        if (pos + n > size) {
            return false;
        }
        memcpy(dst, data + pos, n);
        pos += n;
        return true;
    }
};

static void AppendU16(str::Builder& s, u16 v) {
    u8 buf[2] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

static void AppendU32(str::Builder& s, u32 v) {
    u8 buf[4] = {(u8)(v & 0xff), (u8)((v >> 8) & 0xff), (u8)((v >> 16) & 0xff), (u8)((v >> 24) & 0xff)};
    s.Append(Str((char*)buf, (int)sizeof(buf)));
}

static void AppendArgEnd(str::Builder& s) {
    AppendU16(s, (u16)ControlArgType::End);
}

static void AppendArgInt(str::Builder& s, i32 v) {
    AppendU16(s, (u16)ControlArgType::Int32);
    AppendU32(s, (u32)v);
}

static void AppendArgString(str::Builder& s, Str str) {
    if (len(str) == 0) {
        str = StrL("");
    }
    size_t n = (size_t)str.len;
    AppendU16(s, (u16)ControlArgType::String);
    AppendU32(s, (u32)n);
    s.Append(str);
    s.AppendChar(0);
}

static bool ParseArg(PacketReader& r, ControlArg** argOut);

static bool ParseArgList(PacketReader& r, Vec<ControlArg*>* args, bool explicitCount, u16 count = 0) {
    for (u16 i = 0; !explicitCount || i < count; i++) {
        ControlArg* arg = nullptr;
        if (!ParseArg(r, &arg)) {
            return false;
        }
        if (!arg) {
            return !explicitCount;
        }
        VecAppend(*args, arg);
    }
    return true;
}

static bool ParseArg(PacketReader& r, ControlArg** argOut) {
    u16 typeRaw = 0;
    if (!r.ReadU16(typeRaw)) {
        return false;
    }
    ControlArgType type = (ControlArgType)typeRaw;
    if (type == ControlArgType::End) {
        *argOut = nullptr;
        return true;
    }

    ControlArg* arg = new ControlArg();
    arg->type = type;
    if (type == ControlArgType::Int32) {
        u32 v = 0;
        if (!r.ReadU32(v)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->intVal = (i32)v;
        *argOut = arg;
        return true;
    }
    if (type == ControlArgType::Bytes || type == ControlArgType::String) {
        u32 n = 0;
        if (!r.ReadU32(n)) {
            DeleteControlArg(arg);
            return false;
        }
        u32 extra = (type == ControlArgType::String) ? 1 : 0;
        u8* data = AllocArray<u8>((int)(n + 1));
        if (!r.ReadBytes(data, (size_t)n + extra)) {
            free(data);
            DeleteControlArg(arg);
            return false;
        }
        if (type == ControlArgType::String) {
            arg->str = str::Dup(Str((char*)data, (int)n));
            free(data);
        } else {
            arg->bytes = data;
            arg->bytesLen = n;
        }
        *argOut = arg;
        return true;
    }
    if (type == ControlArgType::List) {
        u16 n = 0;
        if (!r.ReadU16(n)) {
            DeleteControlArg(arg);
            return false;
        }
        arg->list = new Vec<ControlArg*>();
        if (!ParseArgList(r, arg->list, true, n)) {
            DeleteControlArg(arg);
            return false;
        }
        *argOut = arg;
        return true;
    }
    DeleteControlArg(arg);
    return false;
}

static ControlArg* ArgAt(ControlRequest* req, size_t idx, ControlArgType type) {
    if (idx >= (size_t)len(req->args)) {
        return nullptr;
    }
    ControlArg* arg = req->args[(int)idx];
    return arg->type == type ? arg : nullptr;
}

static Str StringArg(ControlRequest* req, size_t idx) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::String);
    return arg ? arg->str : Str();
}

static bool IntArg(ControlRequest* req, size_t idx, i32& valOut) {
    ControlArg* arg = ArgAt(req, idx, ControlArgType::Int32);
    if (!arg) {
        return false;
    }
    valOut = arg->intVal;
    return true;
}

static void AppendError(ControlRequest* req, Str msg) {
    req->results.Reset();
    AppendArgInt(req->results, -1);
    AppendArgString(req->results, msg);
    AppendArgEnd(req->results);
}

static void AppendTestResult(ControlRequest* req, int exitCode, Str result) {
    AppendArgInt(req->results, exitCode);
    AppendArgString(req->results, result);
    AppendArgEnd(req->results);
}

static MainWindow* FirstWindow() {
    return len(gWindows) > 0 ? gWindows[0] : nullptr;
}

static Str MarkupTypeName(AnnotationType tp) {
    switch (tp) {
        case AnnotationType::Highlight:
            return StrL("Highlight");
        case AnnotationType::Underline:
            return StrL("Underline");
        case AnnotationType::Squiggly:
            return StrL("Squiggly");
        case AnnotationType::StrikeOut:
            return StrL("StrikeOut");
        case AnnotationType::Square:
            return StrL("Square");
        case AnnotationType::Circle:
            return StrL("Circle");
        case AnnotationType::Polygon:
            return StrL("Polygon");
        case AnnotationType::PolyLine:
            return StrL("PolyLine");
        case AnnotationType::Ink:
            return StrL("Ink");
        case AnnotationType::Stamp:
            return StrL("Stamp");
        case AnnotationType::Redact:
            return StrL("Redact");
        case AnnotationType::FileAttachment:
            return StrL("FileAttachment");
        case AnnotationType::FreeText:
            return StrL("FreeText");
        default:
            return StrL("other");
    }
}

float CanvasScale(MainWindow* win);
TempStr RefHoverResultTemp(Str action, int x, int y, int* exitCodeOut);

// CvtToScreen is canvas space. Tests post the rect at the frame, and ToDoc
// subtracts the canvas origin, so the reported point has to include it.
static Rect FrameScreenRect(MainWindow* win, Rect r) {
    float s = CanvasScale(win);
    if (s <= 0.f) {
        s = 1.f;
    }
    r.x += (int)((float)win->canvasRc.x / s + 0.5f);
    r.y += (int)((float)win->canvasRc.y / s + 0.5f);
    return r;
}

// the inverse: a visibility probe hands back the frame point FrameScreenRect reported
static Point FramePointToCanvas(MainWindow* win, Point pt) {
    float s = CanvasScale(win);
    if (s <= 0.f) {
        s = 1.f;
    }
    pt.x -= (int)((float)win->canvasRc.x / s + 0.5f);
    pt.y -= (int)((float)win->canvasRc.y / s + 0.5f);
    return pt;
}

// Screen rects and undo state of the annotations the tests edit.
static TempStr MarkupAnnotsResultTemp(Str action, int x, int y, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0 || !gWindows[0]) {
        return finish(StrL("NOTREADY no-window\n"), 2);
    }
    MainWindow* win = gWindows[0];
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineBase* engine = dm ? dm->GetEngine() : nullptr;
    if (!engine) {
        return finish(StrL("NOTREADY no-engine\n"), 2);
    }
    if (str::Eq(action, StrL("erase-ink"))) {
        // the test's point is a frame client point; the eraser wants canvas space
        float s = CanvasScale(win);
        if (s <= 0.f) {
            s = 1.f;
        }
        Point pt((int)(((float)x - (float)win->canvasRc.x) / s), (int)(((float)y - (float)win->canvasRc.y) / s));
        AnnotationPlacementEraseAt(win, pt);
    }
    if (str::Eq(action, StrL("finish-ink"))) {
        FinishInkAnnotationPlacement(win);
    }
    if (str::Eq(action, StrL("cancel-ink"))) {
        CancelAnnotationPlacement(win);
    }
    if (str::Eq(action, StrL("open-embedded"))) {
        int pageNo = dm->CurrentPageNo();
        Vec<IPageElement*> els = engine->GetElements(pageNo);
        IPageDestination* dest = nullptr;
        for (IPageElement* el : els) {
            if (!el || !el->Is(kindPageElementDest)) {
                continue;
            }
            IPageDestination* d = el->AsLink();
            if (d && d->GetKind() == kindDestinationLaunchEmbedded) {
                dest = d;
                break;
            }
        }
        if (!dest) {
            return finish(StrL("ERROR no-embedded-dest\n"), 1);
        }
        win->ctrl->HandleLink(dest, win->linkHandler);
        return finish(StrL("OK\n"), 0);
    }
    Vec<Annotation*> annots;
    EngineMupdfGetLoadedAnnotations(engine, annots);
    int n = 0;
    for (Annotation* a : annots) {
        AnnotationType tp = Type(a);
        bool isMarkup = tp == AnnotationType::Highlight || tp == AnnotationType::Underline ||
                        tp == AnnotationType::Squiggly || tp == AnnotationType::StrikeOut;
        bool isShape = tp == AnnotationType::Square || tp == AnnotationType::Circle || tp == AnnotationType::Polygon ||
                       tp == AnnotationType::PolyLine || tp == AnnotationType::Ink;
        bool isStamp = tp == AnnotationType::Stamp;
        bool isRedact = tp == AnnotationType::Redact;
        bool isFileAttachment = tp == AnnotationType::FileAttachment;
        bool isFreeText = tp == AnnotationType::FreeText;
        if (!isMarkup && !isShape && !isStamp && !isRedact && !isFileAttachment && !isFreeText) {
            continue;
        }
        Str typeName = MarkupTypeName(tp);
        if (isRedact) {
            Vec<RectF> quads = GetQuadPointsAsRect(a);
            RectF r = GetRect(a);
            Rect screen = FrameScreenRect(win, dm->CvtToScreen(PageNo(a), r));
            out.Append(fmt("type=%s page=%d quads=%d rect=%g,%g,%g,%g screen=%d,%d,%d,%d\n", typeName, PageNo(a),
                           len(quads), r.x, r.y, r.dx, r.dy, screen.x, screen.y, screen.dx, screen.dy));
            for (int i = 0; i < len(quads); i++) {
                RectF qr = quads[i];
                out.Append(fmt("rect=%g,%g,%g,%g\n", qr.x, qr.y, qr.dx, qr.dy));
            }
            n++;
            continue;
        }
        if (isShape || isStamp || isFileAttachment || isFreeText) {
            RectF r = GetRect(a);
            Rect screen = FrameScreenRect(win, dm->CvtToScreen(PageNo(a), r));
            out.Append(fmt("type=%s page=%d rect=%g,%g,%g,%g screen=%d,%d,%d,%d\n", typeName, PageNo(a), r.x, r.y, r.dx,
                           r.dy, screen.x, screen.y, screen.dx, screen.dy));
            if (tp == AnnotationType::PolyLine || tp == AnnotationType::Polygon) {
                Vec<PointF> pts = GetVertices(a);
                bool closed = len(pts) > 2 && pts[0] == VecLast(pts);
                out.Append(fmt("polyline vertices=%d closed=%d pts=", len(pts), closed ? 1 : 0));
                for (int i = 0; i < len(pts); i++) {
                    out.Append(fmt(i == 0 ? "%g,%g" : ";%g,%g", pts[i].x, pts[i].y));
                }
                out.Append(StrL("\n"));
            }
            if (tp == AnnotationType::Ink) {
                Vec<int> strokeCounts;
                Vec<PointF> points;
                GetInkList(a, strokeCounts, points);
                out.Append(fmt("ink strokes=%d points=%d opacity=%d width=%d\n", len(strokeCounts), len(points),
                               Opacity(a), BorderWidth(a)));
                if (len(points) > 0) {
                    float x0 = points[0].x;
                    float y0 = points[0].y;
                    float x1 = x0;
                    float y1 = y0;
                    for (PointF p : points) {
                        if (p.x < x0) {
                            x0 = p.x;
                        }
                        if (p.y < y0) {
                            y0 = p.y;
                        }
                        if (p.x > x1) {
                            x1 = p.x;
                        }
                        if (p.y > y1) {
                            y1 = p.y;
                        }
                    }
                    out.Append(fmt("inkRect=%g,%g,%g,%g\n", x0, y0, x1 - x0, y1 - y0));
                }
            }
            n++;
            continue;
        }
        Vec<RectF> quads = GetQuadPointsAsRect(a);
        out.Append(fmt("type=%s page=%d quads=%d\n", typeName, PageNo(a), len(quads)));
        out.Append(StrL("color="));
        SerializePdfColor(GetColor(a), out);
        out.Append(StrL("\n"));
        for (int i = 0; i < len(quads); i++) {
            RectF r = quads[i];
            out.Append(fmt("rect=%g,%g,%g,%g\n", r.x, r.y, r.dx, r.dy));
            Rect screen = FrameScreenRect(win, dm->CvtToScreen(PageNo(a), r));
            out.Append(fmt("screen=%d,%d,%d,%d\n", screen.x, screen.y, screen.dx, screen.dy));
        }
        n++;
    }
    out.Append(fmt("n=%d\n", n));
    out.Append(fmt("annotations=%d\n", len(annots)));
    {
        Str pageText = engine->GetTextForPage(1);
        str::Builder collapsed;
        bool space = false;
        for (int i = 0; i < len(pageText); i++) {
            char c = pageText.s[i];
            if (c == '\r' || c == '\n' || c == '\t') {
                if (!space && len(collapsed) > 0) {
                    collapsed.AppendChar(' ');
                    space = true;
                }
                continue;
            }
            collapsed.AppendChar(c);
            space = false;
        }
        out.Append(fmt("page1text=%s\n", ToStrTemp(collapsed)));
    }
    int canUndo = EngineMupdfCanUndo(engine) ? 1 : 0;
    int canRedo = EngineMupdfCanRedo(engine) ? 1 : 0;
    int modified = EngineHasUnsavedAnnotations(engine) ? 1 : 0;
    out.Append(fmt("undo canUndo=%d canRedo=%d modified=%d\n", canUndo, canRedo, modified));
    bool selectedHover = tab->selectedAnnotation && tab->selectedAnnotation == win->annotationUnderCursor;
    out.Append(fmt("state selected=%d hover=%d editToolbar=%d notification=%d selectedHover=%d\n",
                   tab->selectedAnnotation ? 1 : 0, win->annotationUnderCursor ? 1 : 0,
                   win->pdfAnnotationsToolbarEnabled ? 1 : 0, 0, selectedHover ? 1 : 0));
    out.Append(AnnotEditToolbarStateTemp(win));
    out.Append(AnnotFilterToolbarStateTemp(win));
    out.Append(AnnotationHoverOverlayStateTemp(win));
    out.Append(AnnotationPlacementStateTemp(win));
    TempStr FreeTextInPlaceEditStateTemp(MainWindow * win);
    out.Append(FreeTextInPlaceEditStateTemp(win));
    TempStr AnnotColorPopupStateTemp(MainWindow * win);
    out.Append(AnnotColorPopupStateTemp(win));
    return finish({}, 0);
}

TempStr HomeSelectionForWindowTemp(int* exitCodeOut, int winIdx);

// Current chapter/page and chapter table state of the first window's doc.
static TempStr ChapterInfoResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    if (!win->IsDocLoaded() || !win->ctrl) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DocController* ctrl = win->ctrl;
    Location cur = ctrl->CurrentLocation();
    int laidOut = 0;
    DisplayModel* dm = ctrl->AsFixed();
    if (dm && dm->GetEngine()) {
        laidOut = dm->GetEngine()->ChaptersLaidOut();
    }
    bool chapterUi = ShowChapterUi(ctrl);
    out.Append(
        fmt("OK chapter=%d page=%d chapterCount=%d chapterPageCount=%d pageCount=%d hasChapters=%d "
            "laidOut=%d chapterUi=%d\n",
            cur.chapter, cur.page, ctrl->ChapterCount(), ctrl->ChapterPageCount(cur.chapter), ctrl->PageCount(),
            ctrl->HasChapters() ? 1 : 0, laidOut, chapterUi ? 1 : 0));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Navigate to {chapter, page} (clamped) and report where it landed.
static TempStr GoToLocationResultTemp(int chapter, int page, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    if (!win->IsDocLoaded() || !win->ctrl) {
        return fail(StrL("NOTREADY no-doc"), 2);
    }
    DocController* ctrl = win->ctrl;
    Location want = ctrl->ClampLocation({chapter, page});
    ctrl->GoToLocation(want, true);
    Location got = ctrl->CurrentLocation();
    out.Append(fmt("OK chapter=%d page=%d\n", got.chapter, got.page));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// One line per link on a page: kind, page, rects, value.
static TempStr PageLinksResultTemp(Str path, int pageNo, int* exitCodeOut) {
    str::Builder out;
    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR engine-create-failed path=%s\n", path));
        return ToStrTemp(out);
    }
    if (!engine->BenchLoadPage(pageNo)) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR page-load-failed page=%d\n", pageNo));
        SafeEngineRelease(&engine);
        return ToStrTemp(out);
    }

    int nLinks = 0;
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (!el || !el->Is(kindPageElementDest)) {
            continue;
        }
        IPageDestination* dest = el->AsLink();
        if (!dest) {
            continue;
        }
        nLinks++;
        Str value = dest->GetValue();
        TempStr valueShown = str::ReplaceTemp(value, StrL("\r\n"), StrL("|"));
        valueShown = str::ReplaceTemp(valueShown, StrL("\n"), StrL("|"));
        RectF src = el->GetRect();
        RectF destRc = dest->GetRect();
        out.Append(fmt("kind=%s page=%d src=%g,%g,%g,%g dest=%g,%g,%g,%g value=%s\n", Str(dest->GetKind()),
                       PageDestGetPageNo(dest), src.x, src.y, src.dx, src.dy, destRc.x, destRc.y, destRc.dx, destRc.dy,
                       valueShown));
    }
    if (nLinks == 0) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(fmt("ERROR no-links page=%d\n", pageNo));
    } else if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    SafeEngineRelease(&engine);
    return ToStrTemp(out);
}

// clipKind values match SumatraTest.cpp ImageRenderEdgesResultTemp
constexpr int kClipSelection = 1;
constexpr int kClipRightHalfTile = 2;
constexpr int kClipFullPageTile = 3;

// Render an image page and report dest size plus the RGB of the left and right
// edge pixels. clipKind 1 is the copy-selection rect, 2 a right-half tile
// after a full-page render, 3 the page/pixel round trip.
static TempStr ImageRenderEdgesResultTemp(Str path, int zoomPercent, int clipKind, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    RectF box = engine->PageMediabox(1);
    float zoom = (float)zoomPercent / 100.f;
    RectF clip;
    RectF* pageRect = nullptr;
    if (clipKind == kClipSelection) {
        // same half-pixel pull-back CvtFromScreen applies to a pixel-aligned
        // selection of the whole image
        clip = RectF(-0.499f, -0.499f, box.dx, box.dy);
        pageRect = &clip;
    }
    if (clipKind == kClipRightHalfTile) {
        // full render first so mupdf caches the whole decoded image, then a
        // tile of the right half
        RenderPageArgs full(1, zoom, 0, nullptr, RenderTarget::Export);
        FreePixmap(engine->RenderPage(full));
        clip = RectF(box.dx / 2, 0, box.dx / 2, box.dy);
        pageRect = &clip;
    }
    Rect tile;
    if (clipKind == kClipFullPageTile) {
        tile = engine->Transform(box, 1, zoom, 0).Round();
        clip = engine->Transform(ToRectF(tile), 1, zoom, 0, true);
        pageRect = &clip;
    }
    RenderPageArgs args(1, zoom, 0, pageRect, RenderTarget::Export);
    Pixmap* bmp = engine->RenderPage(args);
    if (!bmp) {
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR render-failed box=%gx%g zoom=%g\n", box.dx, box.dy, zoom));
    }
    if (bmp->width < 2 || bmp->height < 1 || !bmp->data) {
        TempStr msg = fmt("ERROR pixmap-too-small bmp=%dx%d fmt=%d box=%gx%g\n", bmp->width, bmp->height,
                          (int)bmp->format, box.dx, box.dy);
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(msg);
    }
    int bpp = PixmapBytesPerPixel(bmp->format);
    if (bpp < 3) {
        FreePixmap(bmp);
        SafeEngineRelease(&engine);
        return fail(fmt("ERROR pixmap-fmt=%d\n", (int)bmp->format));
    }

    auto pixel = [&](int x, int y, int* r, int* g, int* b) {
        const u8* px = bmp->data + ((size_t)y * (size_t)bmp->stride) + ((size_t)x * bpp);
        if (bmp->format == PixmapFormat::RGBA8) {
            *r = px[0];
            *g = px[1];
            *b = px[2];
        } else {
            *b = px[0];
            *g = px[1];
            *r = px[2];
        }
    };
    int lr, lg, lb, rr, rg, rb;
    pixel(0, bmp->height / 2, &lr, &lg, &lb);
    pixel(bmp->width - 1, bmp->height / 2, &rr, &rg, &rb);
    out.Append(fmt("size=%dx%d left=%d,%d,%d right=%d,%d,%d", bmp->width, bmp->height, lr, lg, lb, rr, rg, rb));
    if (clipKind == kClipFullPageTile) {
        out.Append(fmt(" tile=%dx%d", tile.dx, tile.dy));
    }
    out.Append(StrL("\n"));

    FreePixmap(bmp);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Sidebar column vs canvas, in frame coordinates. Hidden panels report x=-1,
// matching orig's invisible child windows.
static TempStr SidebarLayoutResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    if (!win) {
        return finish(StrL("NOTREADY no-window"), 2);
    }
    bool pref = gSettings && gSettings->sidebarOnRight;
    bool topVis = win->uiState.tocVisible;
    bool bottomVis = win->uiState.favVisible;
    WindowTab* tab = win->CurrentTab();
    Str topView = tab ? SidebarContentToStr(tab->sidebarContent) : StrL("none");
    Str bottomView = SidebarContentToStr(win->sidebarBottomContent);
    int topX = -1;
    int bottomX = -1;
    int canvasX = win->canvasRc.x;
    if (topVis || bottomVis) {
        int sideX = pref ? canvasX + win->canvasRc.dx + kSplitterDx : AppShellFrameBorder(win);
        if (topVis) {
            topX = sideX;
        }
        if (bottomVis) {
            bottomX = sideX;
        }
    }
    return finish(fmt("OK pref=%d topVis=%d bottomVis=%d topX=%d bottomX=%d canvasX=%d topView=%s bottomView=%s",
                      pref ? 1 : 0, topVis ? 1 : 0, bottomVis ? 1 : 0, topX, bottomX, canvasX, topView, bottomView),
                  0);
}

static int CountNonWhitePixels(Pixmap* bmp) {
    if (!bmp || !bmp->data) {
        return 0;
    }
    int bpp = PixmapBytesPerPixel(bmp->format);
    if (bpp < 3) {
        return 0;
    }
    int n = 0;
    for (int y = 0; y < bmp->height; y++) {
        const u8* row = bmp->data + ((size_t)y * (size_t)bmp->stride);
        for (int x = 0; x < bmp->width; x++) {
            const u8* px = row + ((size_t)x * bpp);
            int r, g, b;
            if (bmp->format == PixmapFormat::RGBA8) {
                r = px[0];
                g = px[1];
                b = px[2];
            } else {
                b = px[0];
                g = px[1];
                r = px[2];
            }
            if (r < 250 || g < 250 || b < 250) {
                n++;
            }
        }
    }
    return n;
}

// View vs print ink. A print-only OCG must still paint for print (issue #6101).
static TempStr PageRenderViewPrintResultTemp(Str path, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&out, exitCodeOut](Str msg) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        return ToStrTemp(out);
    };

    EngineBase* engine = CreateEngineFromFile(path, nullptr, false);
    if (!engine) {
        return fail(fmt("ERROR engine-create-failed path=%s\n", path));
    }
    if (!engine->BenchLoadPage(1)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR page-load-failed\n"));
    }

    RenderPageArgs viewArgs(1, 1.f, 0, nullptr, RenderTarget::View);
    RenderPageArgs printArgs(1, 1.f, 0, nullptr, RenderTarget::Print);
    Pixmap* viewRaw = engine->RenderPage(viewArgs);
    Pixmap* printRaw = engine->RenderPage(printArgs);
    if (!viewRaw || !viewRaw->data || !printRaw || !printRaw->data) {
        FreePixmap(viewRaw);
        FreePixmap(printRaw);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR render-failed\n"));
    }
    Pixmap* view = EnsureReadablePixmap(viewRaw);
    Pixmap* print = EnsureReadablePixmap(printRaw);
    if (!view || !view->data || !print || !print->data) {
        if (view != viewRaw) {
            FreePixmap(view);
        }
        if (print != printRaw) {
            FreePixmap(print);
        }
        FreePixmap(viewRaw);
        FreePixmap(printRaw);
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR pixmap-convert-failed\n"));
    }

    int viewN = CountNonWhitePixels(view);
    int printN = CountNonWhitePixels(print);
    out.Append(fmt("viewNonwhite=%d printNonwhite=%d viewSize=%dx%d printSize=%dx%d\n", viewN, printN, view->width,
                   view->height, print->width, print->height));
    if (view != viewRaw) {
        FreePixmap(view);
    }
    if (print != printRaw) {
        FreePixmap(print);
    }
    FreePixmap(viewRaw);
    FreePixmap(printRaw);
    SafeEngineRelease(&engine);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

static int JpegSofComponents(Str jpeg) {
    ByteReader r(jpeg);
    int n = len(jpeg);
    if (n < 4 || r.UInt8(0) != 0xFF || r.UInt8(1) != 0xD8) {
        return 0;
    }
    int i = 2;
    while (i + 9 < n) {
        if (r.UInt8(i) != 0xFF) {
            return 0;
        }
        u8 marker = r.UInt8(i + 1);
        if (marker == 0xDA || marker == 0xD9) {
            return 0;
        }
        if (marker >= 0xD0 && marker <= 0xD7) {
            i += 2;
            continue;
        }
        if (marker == 0x01) {
            i += 2;
            continue;
        }
        u16 seglen = r.UInt16BE(i + 2);
        if (seglen < 2) {
            return 0;
        }
        if (marker >= 0xC0 && marker <= 0xC3) {
            return r.UInt8(i + 9);
        }
        i += 2 + (int)seglen;
    }
    return 0;
}

static u16 TiffPhotometric(Str tiff) {
    ByteReader r(tiff);
    int n = len(tiff);
    if (n < 8 || r.UInt8(0) != 'I' || r.UInt8(1) != 'I') {
        return 0;
    }
    u32 ifd = r.UInt32LE(4);
    if (ifd + 2 > (u32)n) {
        return 0;
    }
    u16 count = r.UInt16LE((int)ifd);
    for (u16 i = 0; i < count; i++) {
        int e = (int)ifd + 2 + ((int)i * 12);
        if (e + 12 > n) {
            break;
        }
        if (r.UInt16LE(e) == 262) {
            return r.UInt16LE(e + 8);
        }
    }
    return 0;
}

// A DeviceCMYK JPEG saved as JPEG must stay CMYK and not invert. Saved as
// TIFF it must stay CMYK rather than become RGB.
static TempStr CmykImageSaveResultTemp(Str jpegPath, Str tiffPath, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        if (!str::EndsWith(msg, StrL("\n"))) {
            out.AppendChar('\n');
        }
        return ToStrTemp(out);
    };
    if (len(jpegPath) == 0 || len(tiffPath) == 0) {
        return fail(StrL("ERROR bad-args"));
    }

    PdfCreator pdf;
    if (!pdf.ctx || !pdf.doc) {
        return fail(StrL("ERROR pdf-create"));
    }
    fz_context* ctx = pdf.ctx;
    fz_pixmap* pix = nullptr;
    fz_buffer* jpegBuf = nullptr;
    Str jpegSrc;
    bool built = false;
    fz_var(pix);
    fz_var(jpegBuf);

    fz_try(ctx) {
        pix = fz_new_pixmap(ctx, fz_device_cmyk(ctx), 32, 32, nullptr, 0);
        fz_clear_pixmap(ctx, pix);
        for (int y = 0; y < 32; y++) {
            u8* p = pix->samples + ((size_t)y * (size_t)pix->stride);
            for (int x = 0; x < 32; x++) {
                p[0] = 200;
                p[1] = 10;
                p[2] = 20;
                p[3] = 30;
                p += 4;
            }
        }
        // Adobe polarity so a standalone JPEG embeds with the usual invert.
        jpegBuf = fz_new_buffer_from_pixmap_as_jpeg(ctx, pix, fz_default_color_params, 95, 1);
        unsigned char* data = nullptr;
        size_t n = fz_buffer_storage(ctx, jpegBuf, &data);
        if (data && n > 0 && n <= (size_t)INT_MAX) {
            jpegSrc = Str((char*)data, (int)n);
        }
    }
    fz_catch(ctx) {
        fz_report_error(ctx);
        jpegSrc = {};
    }
    if (jpegSrc) {
        built = pdf.AddPageFromImageData(jpegSrc, 72.f);
    }
    fz_drop_buffer(ctx, jpegBuf);
    fz_drop_pixmap(ctx, pix);
    if (!built) {
        return fail(StrL("ERROR embed-cmyk-jpeg"));
    }

    TempStr pdfPath = GetTempFilePathTemp(StrL("cmyksave"));
    if (len(pdfPath) == 0 || !pdf.SaveToFile(pdfPath)) {
        return fail(StrL("ERROR save-pdf"));
    }

    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        file::Delete(pdfPath);
        return fail(StrL("ERROR engine-create-failed"));
    }
    if (!engine->BenchLoadPage(1)) {
        SafeEngineRelease(&engine);
        file::Delete(pdfPath);
        return fail(StrL("ERROR page-load-failed"));
    }
    Vec<IPageElement*> els = engine->GetElements(1);
    IPageElement* imgEl = nullptr;
    for (IPageElement* el : els) {
        if (el && el->Is(kindPageElementImage)) {
            imgEl = el;
            break;
        }
    }
    if (!imgEl) {
        SafeEngineRelease(&engine);
        file::Delete(pdfPath);
        return fail(StrL("ERROR no-image-element"));
    }
    Str jpeg = engine->GetImageDataForPageElement(imgEl);
    SafeEngineRelease(&engine);
    file::Delete(pdfPath);
    if (len(jpeg) == 0) {
        return fail(StrL("ERROR no-image-data"));
    }
    bool wroteJpeg = file::WriteFile(jpegPath, jpeg);
    bool wroteTiff = TrySaveOriginalAsCmykTiff(jpeg, tiffPath);
    int nComp = JpegSofComponents(jpeg);
    int cw = 0, ch = 0, cstride = 0;
    Vec<u8> cmyk;
    bool decoded = DecodeJpegToCmyk(jpeg, cw, ch, cstride, cmyk);
    int jc = 0, jm = 0, jy = 0, jk = 0;
    if (decoded && cstride >= 4 && len(cmyk) >= 4) {
        jc = cmyk[0];
        jm = cmyk[1];
        jy = cmyk[2];
        jk = cmyk[3];
    }
    Str tiff = file::ReadFile(tiffPath);
    u16 photo = TiffPhotometric(tiff);
    str::Free(tiff);
    str::Free(jpeg);

    out.Append(fmt("jpeg_ok=%d jpeg_n=%d jpeg_c=%d jpeg_m=%d jpeg_y=%d jpeg_k=%d tiff_ok=%d tiff_photo=%d\n",
                   (int)wroteJpeg, nComp, jc, jm, jy, jk, (int)wroteTiff, (int)photo));
    if (exitCodeOut) {
        *exitCodeOut = (wroteJpeg && wroteTiff && nComp == 4 && photo == 5) ? 0 : 1;
    }
    return ToStrTemp(out);
}

// A blank strip of pages 1 and 2 as one selection image, and how many of its
// pixels are white. tests/render-selections-8bpp.ts.
static TempStr RenderSelectionsResultTemp(int* exitCodeOut) {
    auto fail = [&](Str msg, int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return msg;
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm || dm->PageCount() < 2) {
        return fail(StrL("NOTREADY need-two-pages"), 2);
    }

    Vec<SelectionOnPage> sels;
    RectF r(72, 300, 200, 40);
    VecAppend(sels, SelectionOnPage(1, &r, nullptr));
    VecAppend(sels, SelectionOnPage(2, &r, nullptr));
    Pixmap* px = RenderSelectionsAsPixmap(dm, sels);
    if (!px || !px->data) {
        FreePixmap(px);
        return fail(StrL("ERROR no-bitmap"), 1);
    }
    int white = 0;
    int total = px->width * px->height;
    if (px->format == PixmapFormat::BGRA8) {
        for (int y = 0; y < px->height; y++) {
            const u32* row = (const u32*)(px->data + ((size_t)y * px->stride));
            for (int x = 0; x < px->width; x++) {
                if ((row[x] & 0xffffff) == 0xffffff) {
                    white++;
                }
            }
        }
    }
    TempStr res = fmt("OK w=%d h=%d format=%d white=%d total=%d", px->width, px->height, (int)px->format, white, total);
    FreePixmap(px);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return res;
}

// GoToPage on a background tab so UpdateScrollbars sees a non-current dm.
static TempStr HiddenTabGoToPageResultTemp(int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"), 2);
    }
    MainWindow* win = gWindows[0];
    if (!win) {
        return fail(StrL("NOTREADY no-window"), 2);
    }

    DisplayModel* dm = nullptr;
    for (WindowTab* tab : win->Tabs()) {
        if (tab && tab != win->CurrentTab() && tab->AsFixed()) {
            dm = tab->AsFixed();
            break;
        }
    }
    if (!dm) {
        return fail(StrL("NOTREADY no-hidden-doc"), 2);
    }

    dm->GoToPage(dm->CurrentPageNo(), false);
    out.Append(StrL("OK\n"));
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Each page's width, and where the top-level bookmarks point.
// tests/sidebar-thumbnails.ts gives every page a unique width.
static TempStr PageInfoResultTemp(int* exitCodeOut) {
    MainWindow* win = len(gWindows) > 0 ? gWindows[0] : nullptr;
    WindowTab* tab = win ? win->CurrentTab() : nullptr;
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    if (!dm) {
        if (exitCodeOut) {
            *exitCodeOut = 2;
        }
        return str::DupTemp(StrL("NOTREADY no-document"));
    }
    str::Builder out;
    EngineBase* engine = dm->GetEngine();
    out.Append(fmt("pages=%d widths=", engine->PageCount()));
    for (int i = 1; i <= engine->PageCount(); i++) {
        out.Append(fmt(i == 1 ? "%d" : ",%d", (int)lroundf(engine->PageMediabox(i).dx)));
    }
    out.Append(StrL(" toc="));
    TocTree* toc = engine->GetToc();
    for (TocItem* it = toc && toc->root ? toc->root->child : nullptr; it; it = it->next) {
        out.Append(fmt("%s:%d;", it->title, it->pageNo));
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return fmt("OK %s", ToStrTemp(out));
}

static TempStr PixmapRgbHexTemp(Pixmap* px, int x, int y) {
    int bpp = PixmapBytesPerPixel(px->format);
    u8* p = px->data + ((size_t)y * (size_t)px->stride) + ((size_t)x * (size_t)bpp);
    // BGRA in memory
    return fmt("%02x%02x%02x", (int)p[2], (int)p[1], (int)p[0]);
}

// Corners of the image Copy Image / Save Image would hand out, after the
// page's transform (issue #6214).
static TempStr ImageOrientationResultTemp(Str pdfPath, int pageNo, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        out.Append(msg);
        out.AppendChar('\n');
        return ToStrTemp(out);
    };
    EngineBase* engine = CreateEngineFromFile(pdfPath, nullptr, false);
    if (!engine) {
        return fail(StrL("ERROR engine-create-failed"));
    }
    if (!engine->BenchLoadPage(pageNo)) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR page-load-failed"));
    }
    IPageElement* imgEl = nullptr;
    Vec<IPageElement*> els = engine->GetElements(pageNo);
    for (IPageElement* el : els) {
        if (el && el->Is(kindPageElementImage)) {
            imgEl = el;
            break;
        }
    }
    if (!imgEl) {
        SafeEngineRelease(&engine);
        return fail(StrL("ERROR no-image-element"));
    }
    Pixmap* px = nullptr;
#if OS_WIN
    RenderedBitmap* bmp = engine->GetImageForPageElement(imgEl);
    SafeEngineRelease(&engine);
    if (!bmp) {
        return fail(StrL("ERROR no-image"));
    }
    px = PixmapToBgra(PixmapFromRenderedBitmap(bmp));
#else
    px = EngineMupdfPageImagePixmap(engine, imgEl);
    SafeEngineRelease(&engine);
    if (!px) {
        return fail(StrL("ERROR no-image"));
    }
#endif
    if (!px || !px->data) {
        FreePixmap(px);
        return fail(StrL("ERROR no-pixmap"));
    }
    int w = px->width;
    int h = px->height;
    out.Append(fmt("w=%d h=%d tl=%s tr=%s bl=%s br=%s\n", w, h, PixmapRgbHexTemp(px, 0, 0),
                   PixmapRgbHexTemp(px, w - 1, 0), PixmapRgbHexTemp(px, 0, h - 1), PixmapRgbHexTemp(px, w - 1, h - 1)));
    FreePixmap(px);
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

// Reload a 2-page file, then a 1-page file, and report the page-info tip.
// LoadDocument refreshes it (issue #2252).
static TempStr PageInfoOverlayResultTemp(Str pathTwoPages, Str pathOnePage, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (len(pathTwoPages) == 0 || len(pathOnePage) == 0) {
        return fail(StrL("ERROR missing-paths"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    if (!win) {
        return fail(StrL("NOTREADY no-window"));
    }

    LoadDocument(win, pathTwoPages, LoadPrefs::DontSave, LoadReuse::CurrentTab);
    if (!win->IsDocLoaded() || !win->ctrl || win->ctrl->PageCount() != 2) {
        return fail(StrL("ERROR two-page-load"));
    }

    if (!win->pageInfoWanted) {
        win->pageInfoWanted = true;
        ShowPageInfoIfWanted(win);
    }
    NotificationWnd* wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (!wnd) {
        return fail(StrL("ERROR no-overlay"));
    }
    TempStr msg = NotificationGetMessageTemp(wnd);
    if (!str::Contains(msg, StrL("/ 2"))) {
        out.Append(fmt("FAIL before-reload msg=%s\n", msg));
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }

    LoadDocument(win, pathOnePage, LoadPrefs::DontSave, LoadReuse::CurrentTab);
    if (!win->IsDocLoaded() || !win->ctrl || win->ctrl->PageCount() != 1) {
        return fail(StrL("ERROR one-page-load"));
    }
    wnd = GetNotificationForGroup(win, kNotifPageInfo);
    if (!wnd) {
        return fail(StrL("ERROR overlay-gone"));
    }
    msg = NotificationGetMessageTemp(wnd);
    bool ok = str::Contains(msg, StrL("/ 1")) && !str::Contains(msg, StrL("/ 2"));
    if (ok) {
        out.Append(fmt("OK msg=%s\n", msg));
    } else {
        out.Append(fmt("FAIL after-reload msg=%s\n", msg));
    }
    if (exitCodeOut) {
        *exitCodeOut = ok ? 0 : 1;
    }
    return ToStrTemp(out);
}

// orig posts WM_CLOSE. There is no frame wndproc, so post the same close.
static void TestPostCloseWindow(MainWindow* win) {
    CloseWindowIfCan(win, true);
}

// Expand SelectionHandlers placeholders against the current tab's selection.
static TempStr SelectionVarsResultTemp(Str pattern, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](Str msg, int code) -> TempStr {
        out.Append(msg);
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };
    if (len(gWindows) == 0 || !gWindows[0]) {
        return finish(StrL("NOTREADY no-window\n"), 2);
    }
    WindowTab* tab = gWindows[0]->CurrentTab();
    bool isTextOnly = false;
    TempStr sel = tab ? GetSelectedTextTemp(tab, StrL("\n"), isTextOnly) : TempStr{};
    if (len(sel) == 0) {
        sel = StrL("");
    }
    if (str::IsEmptyOrWhiteSpace(pattern)) {
        pattern = StrL("${selectionPosition}");
    }
    TempStr expanded = ExpandSelectionVarsTemp(pattern, sel, false, 0, nullptr, tab);
    out.Append(StrL("pattern="));
    out.Append(pattern);
    out.AppendChar('\n');
    out.Append(StrL("expanded="));
    out.Append(expanded);
    out.AppendChar('\n');
    if (tab && tab->selectionOnPage) {
        out.Append(fmt("nrects=%d\n", len(*tab->selectionOnPage)));
        for (SelectionOnPage& onPage : *tab->selectionOnPage) {
            RectF r = onPage.rect;
            out.Append(fmt("rect=%g,%g,%g,%g page=%d\n", r.x, r.y, r.dx, r.dy, onPage.pageNo));
            if (onPage.HasQuad()) {
                QuadF q = onPage.quad;
                out.Append(fmt("quad=%g,%g %g,%g %g,%g %g,%g\n", q.ul.x, q.ul.y, q.ur.x, q.ur.y, q.ll.x, q.ll.y, q.lr.x,
                               q.lr.y));
            }
        }
    } else {
        out.Append(StrL("nrects=0\n"));
    }
    return finish({}, 0);
}

// The frame's non-client strips, in window coordinates. Maximized, DWM's
// overhang is outside the work area and must stay unpainted.
static TempStr FrameNcStripsResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = FirstWindow();
    Rect wr{};
    int clientX = 0;
    int clientY = 0;
    int clientDx = 0;
    int clientDy = 0;
    bool zoomed = false;
#if OS_WIN
    HWND hwnd = win ? AppShellNativeHwnd(win) : nullptr;
    if (!hwnd) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    zoomed = IsZoomed(hwnd);
    if (!zoomed) {
        wr = HwndWindowRect(hwnd);
        Rect cr = HwndClientRect(hwnd);
        Point clientScreen = HwndClientToScreen(hwnd, Point(0, 0));
        clientX = clientScreen.x - wr.x;
        clientY = clientScreen.y - wr.y;
        clientDx = cr.dx;
        clientDy = cr.dy;
    }
#else
    if (!win || !win->gpuiWin) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    wr = ToolWinNativeFrame(win->gpuiWin);
    Rect cr = ToolWinNativeContentRect(win->gpuiWin);
    if (wr.IsEmpty() || cr.IsEmpty()) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    clientX = cr.x - wr.x;
    clientY = cr.y - wr.y;
    clientDx = cr.dx;
    clientDy = cr.dy;
#endif
    Vec<Rect> strips;
    if (!zoomed) {
        int bottomNcTop = clientY + clientDy;
        int rightNcLeft = clientX + clientDx;
        if (clientY > 0) {
            VecAppend(strips, Rect{0, 0, wr.dx, clientY});
        }
        if (bottomNcTop < wr.dy) {
            VecAppend(strips, Rect{0, bottomNcTop, wr.dx, wr.dy - bottomNcTop});
        }
        if (clientX > 0) {
            VecAppend(strips, Rect{0, clientY, clientX, bottomNcTop - clientY});
        }
        if (rightNcLeft < wr.dx) {
            VecAppend(strips, Rect{rightNcLeft, clientY, wr.dx - rightNcLeft, bottomNcTop - clientY});
        }
    }
    str::Builder sb;
    sb.Append(fmt("zoomed=%d strips=%d", zoomed ? 1 : 0, len(strips)));
    for (Rect& r : strips) {
        sb.Append(fmt(" %d,%d,%d,%d", r.x, r.y, r.dx, r.dy));
    }
    return finish(0, ToStrTemp(sb));
}

// Orig's canvas DefWindowProc handed this wheel back to the frame.
static TempStr WheelWhileClosingResultTemp(int* exitCodeOut) {
    auto finish = [exitCodeOut](int code, TempStr s) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return s;
    };
    MainWindow* win = FirstWindow();
    if (!win || !win->IsDocLoaded()) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-document")));
    }
#if OS_WIN
    HWND hwnd = AppShellNativeHwnd(win);
    if (!hwnd) {
        return finish(2, str::DupTemp(StrL("NOTREADY no-window")));
    }
    win->isBeingClosed = true;
    SendMessageW(hwnd, WM_MOUSEWHEEL, MAKEWPARAM(0, -WHEEL_DELTA), 0);
    if (IsMainWindowValid(win)) {
        win->isBeingClosed = false;
    }
#endif
    return finish(0, str::DupTemp(StrL("OK")));
}

int HomeSearchFontPx(MainWindow* win);

#if OS_WIN
static int DpiFontHeight(PlatformFont* font) {
    return font ? PlatformFontLineHeight(font) : 0;
}

// orig's DpiResultTemp. The hidden window is created after DpiSet, so a
// not-yet-visible root reports that layout DPI instead of the monitor.
static TempStr DpiResultTemp(Str action, int* exitCodeOut) {
    str::Builder out;
    auto finish = [&](int code) -> TempStr {
        if (exitCodeOut) {
            *exitCodeOut = code;
        }
        return ToStrTemp(out);
    };

    if (str::Eq(action, StrL("hidden"))) {
        int prevX = dpiX;
        int prevY = dpiY;
        DpiSet(240, 240);
        HWND hwnd = CreateWindowExW(0, L"STATIC", L"DPI test", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr,
                                    GetModuleHandleW(nullptr), nullptr);
        int layoutDpi = DpiGet();
        int windowDpi = hwnd ? RoundUp(DpiGetForHwnd(hwnd), 4) : 0;
        int fontDy = DpiFontHeight(GetDefaultGuiFont());
        if (hwnd) {
            DestroyWindow(hwnd);
        }
        DpiSet(prevX, prevY);
        out.Append(fmt("layout=%d window=%d font=%d\n", layoutDpi, windowDpi, fontDy));
        return finish(layoutDpi == 240 && windowDpi == 240 && fontDy >= 24 ? 0 : 1);
    }

    if (!str::Eq(action, StrL("state")) || len(gWindows) == 0) {
        out.Append(StrL("ERROR TestDpi expects hidden or state\n"));
        return finish(1);
    }
    MainWindow* win = gWindows[0];
    int findH = FindWindowFontHeight(win);
    if (findH <= 0) {
        findH = FindBarFontHeight(win);
    }
    int findBarDy = FindBarWindowHeight(win);
    // the header's view icons and its close button share one pixel size
    int iconDy = SidebarIconDy(win);
    out.Append(
        fmt("frame=%d current=%d home=%d tocIcon=%d tocEdit=%d tocClose=%d favClose=%d aiLabel=%d aiInput=%d "
            "aiCheckbox=%d aiClose=%d find=%d findBarDy=%d\n",
            AppShellFrameDpi(win), DpiGet(), HomeSearchFontPx(win), iconDy, SidebarFilterFont(win), iconDy, iconDy, 0,
            0, 0, 0, findH, findBarDy));
    return finish(0);
}
#endif

// Mouse-drag text selection of rotated glyphs (issue #4839). Finds `word`,
// presses the first glyph and drags to the last.
static TempStr RotatedTextMouseDragResultTemp(Str word, int* exitCodeOut) {
    str::Builder out;
    auto fail = [&](Str msg) -> TempStr {
        out.Append(msg);
        out.AppendChar('\n');
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    };

    if (str::IsEmptyOrWhiteSpace(word)) {
        return fail(StrL("ERROR missing word"));
    }
    if (len(gWindows) == 0) {
        return fail(StrL("NOTREADY no-window"));
    }
    MainWindow* win = gWindows[0];
    DisplayModel* dm = win ? win->AsFixed() : nullptr;
    if (!dm) {
        return fail(StrL("NOTREADY no-doc"));
    }
    EngineBase* engine = dm->GetEngine();
    const int pageNo = 1;
    Rect* coords = nullptr;
    QuadF* quads = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords, &quads);
    if (len(text) == 0 || !coords) {
        return fail(StrL("ERROR no-page-text"));
    }
    int startGlyph = -1;
    int endGlyph = -1;
    int wordLen = Utf8CodepointCount(word);
    for (int i = 0; i <= textLen - wordLen; i++) {
        if (str::Eq(Utf8SliceByCodepoints(text, i, wordLen), word)) {
            startGlyph = i;
            endGlyph = i + wordLen;
            break;
        }
    }
    if (startGlyph < 0) {
        return fail(StrL("ERROR word-not-found"));
    }
    int first = startGlyph;
    int last = endGlyph - 1;
    for (; first < endGlyph && !coords[first].x && !coords[first].dx; first++) {
    }
    for (; last > first && !coords[last].x && !coords[last].dx; last--) {
    }
    if (first >= endGlyph) {
        return fail(StrL("ERROR empty-glyph-boxes"));
    }
    bool firstTilted = quads && quads[first].IsRotated();
    out.Append(fmt("quads=%d firstTilted=%d start=%d end=%d\n", quads ? 1 : 0, firstTilted ? 1 : 0, first, last));

    PointF p0{(float)(coords[first].x + (coords[first].dx / 2.0)), (float)(coords[first].y + (coords[first].dy / 2.0))};
    PointF p1{(float)(coords[last].x + coords[last].dx), (float)(coords[last].y + (coords[last].dy / 2.0))};
    if (quads) {
        p0 = quads[first].Center();
        // past the last glyph along its baseline so the final letter is included
        p1 = {(quads[last].ur.x + quads[last].lr.x) / 2.f, (quads[last].ur.y + quads[last].lr.y) / 2.f};
    }
    Point s0 = dm->CvtToScreen(pageNo, p0);
    Point s1 = dm->CvtToScreen(pageNo, p1);
    out.Append(fmt("screen0=%d,%d screen1=%d,%d overText0=%d overText1=%d\n", s0.x, s0.y, s1.x, s1.y,
                   dm->IsOverText(s0) ? 1 : 0, dm->IsOverText(s1) ? 1 : 0));
    if (!dm->IsOverText(s0)) {
        return fail(StrL("ERROR start-not-over-text"));
    }

    DeleteOldSelectionInfo(win, true);
    DocCanvasMouseDown(win, s0.x, s0.y);
    int actionDown = (int)win->mouseAction;
    DocCanvasMouseMove(win, s1.x, s1.y);
    DocCanvasMouseUp(win, s1.x, s1.y);
    int actionUp = (int)win->mouseAction;

    WindowTab* tab = win->CurrentTab();
    bool isTextOnly = false;
    TempStr selected = tab ? GetSelectedTextTemp(tab, StrL(" "), isTextOnly) : TempStr{};
    int nrects = (tab && tab->selectionOnPage) ? len(*tab->selectionOnPage) : 0;
    int nQuads = 0;
    int nTilted = 0;
    if (tab && tab->selectionOnPage) {
        for (SelectionOnPage& onPage : *tab->selectionOnPage) {
            if (onPage.HasQuad()) {
                nQuads++;
                if (onPage.quad.IsRotated()) {
                    nTilted++;
                }
            }
        }
    }
    out.Append(fmt("actionDown=%d actionUp=%d isTextOnly=%d nrects=%d nQuads=%d nTilted=%d selected=%s\n", actionDown,
                   actionUp, isTextOnly ? 1 : 0, nrects, nQuads, nTilted, selected));

    bool ok =
        (actionDown == (int)MouseAction::SelectingText) && isTextOnly && nTilted > 0 && str::ContainsI(selected, word);
    if (!ok) {
        if (exitCodeOut) {
            *exitCodeOut = 1;
        }
        return ToStrTemp(out);
    }
    if (exitCodeOut) {
        *exitCodeOut = 0;
    }
    return ToStrTemp(out);
}

bool FreeTextInPlaceSetText(MainWindow* win, const WCHAR* text);
bool CommandPaletteSetText(MainWindow* win, const WCHAR* text);
bool FreeTextInPlaceCommitOnChar(MainWindow* win, int ch);

static void ExecuteControlRequest(ControlRequest* req) {
    switch ((ControlCmd)req->cmd) {
        case ControlCmd::Ping:
            AppendArgString(req->results, StrL("pong"));
            AppendArgEnd(req->results);
            break;

        case ControlCmd::Quit:
            // Orig discards first. A dirty tab otherwise waits on the unsaved
            // dialog, and a test that wants the changes saved already did.
            ResolveUnsavedChangesResultTemp(StrL("discard"), {}, nullptr);
            AppendArgInt(req->results, 0);
            AppendArgEnd(req->results);
            // Same path as CmdExit. AppShellQuit() drops the tabs first, and
            // the shutdown save then writes an empty session.
            uitask::Post(MkFunc0Void(OnMenuExit), "ControlQuit");
            break;

        // A notification covers part of the document for a couple of seconds,
        // so a test that reads pixels either waits it out or turns them off.
        case ControlCmd::SetNotificationsEnabled: {
            i32 enabled = 0;
            if (!IntArg(req, 0, enabled)) {
                AppendError(req, StrL("SetNotificationsEnabled expects int enabled"));
                break;
            }
            SetNotificationsEnabled(enabled != 0);
            AppendTestResult(req, 0, enabled ? StrL("OK enabled") : StrL("OK disabled"));
            break;
        }

        case ControlCmd::TestInverseSearch: {
            i32 page = 0, x = 0, y = 0;
            Str pdf = StringArg(req, 0);
            if (len(pdf) == 0 || !IntArg(req, 1, page) || !IntArg(req, 2, x) || !IntArg(req, 3, y)) {
                AppendError(req, StrL("TestInverseSearch expects string pdf, int page, int x, int y"));
                break;
            }
            AppendTestResult(req, 0, InverseSearchResultTemp(pdf, page, x, y));
            break;
        }

        case ControlCmd::TestSearch: {
            Str pdf = StringArg(req, 0);
            Str needle = StringArg(req, 1);
            Str password = StringArg(req, 2);
            if (len(pdf) == 0 || len(needle) == 0) {
                AppendError(req, StrL("TestSearch expects string pdf, string needle, optional string password"));
                break;
            }
            if (len(password) == 0) {
                password = CliPassword();
            }
            AppendTestResult(req, 0, SearchResultTemp(pdf, needle, password));
            break;
        }

        case ControlCmd::TestFindPageRange: {
            Str pdf = StringArg(req, 0);
            Str needle = StringArg(req, 1);
            i32 first = 0;
            i32 last = 0;
            IntArg(req, 2, first);
            IntArg(req, 3, last);
            Str spec = StringArg(req, 4);
            if (len(pdf) == 0 || len(needle) == 0) {
                AppendError(req, StrL("TestFindPageRange expects string pdf, string needle [, int first, int last [, "
                                      "string spec]]"));
                break;
            }
            int exitCode = 0;
            Str res = FindPageRangeResultTemp(pdf, needle, first, last, spec, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDest: {
            i32 destNo = 0;
            Str pdf = StringArg(req, 0);
            if (len(pdf) == 0 || !IntArg(req, 1, destNo)) {
                AppendError(req, StrL("TestDest expects string pdf, int destinationNumber"));
                break;
            }
            AppendTestResult(req, 0, DestResultTemp(pdf, destNo));
            break;
        }

        case ControlCmd::TestNamedDest: {
            Str pdf = StringArg(req, 0);
            Str name = StringArg(req, 1);
            if (len(pdf) == 0 || len(name) == 0) {
                AppendError(req, StrL("TestNamedDest expects string pdf, string name"));
                break;
            }
            AppendTestResult(req, 0, NamedDestResultTemp(pdf, name));
            break;
        }

        case ControlCmd::TestInsertImage: {
            Str pdfPath = StringArg(req, 0);
            Str imagePath = StringArg(req, 1);
            if (len(pdfPath) == 0 || len(imagePath) == 0) {
                AppendError(req, StrL("TestInsertImage expects string pdfPath, string imagePath"));
                break;
            }
            int exitCode = 0;
            Str res = ImageInsertResultTemp(pdfPath, imagePath, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRenderPageColors: {
            Str path = StringArg(req, 0);
            if (len(path) == 0) {
                AppendError(req, StrL("TestRenderPageColors expects string path [, int pageNo]"));
                break;
            }
            i32 pageNo = 1;
            IntArg(req, 1, pageNo);
            int exitCode = 0;
            Str res = PageRenderColorsResultTemp(path, &exitCode, pageNo);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGetToc: {
            Str path = StringArg(req, 0);
            if (len(path) == 0) {
                AppendError(req, StrL("TestGetToc expects string path"));
                break;
            }
            int exitCode = 0;
            Str res = GetTocResultTemp(path, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFileKind: {
            Str path = StringArg(req, 0);
            Str expectedKind = StringArg(req, 1);
            if (len(path) == 0 || len(expectedKind) == 0) {
                AppendError(req, StrL("TestFileKind expects string path, string expectedKind"));
                break;
            }
            int exitCode = 0;
            Str res = FileKindResultTemp(path, expectedKind, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionTranslate: {
            i32 backend = 0;
            Str srcLang = StringArg(req, 1);
            Str dstLang = StringArg(req, 2);
            Str text = StringArg(req, 3);
            if (!IntArg(req, 0, backend) || len(text) == 0) {
                AppendError(req, StrL("TestSelectionTranslate expects int backend, strings src, dst, text"));
                break;
            }
            int exitCode = 0;
            Str res = SelectionTranslateResultTemp(backend, srcLang, dstLang, text, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFavoriteNav: {
            i32 pageNo = 0;
            IntArg(req, 1, pageNo);
            int exitCode = 0;
            Str res = FavoriteNavResultTemp(StringArg(req, 0), pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRectSelectionDrag: {
            int exitCode = 0;
            Str res = RectSelectionDragResultTemp(StringArg(req, 0), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestClickClearsSelection: {
            int exitCode = 0;
            Str res = ClickClearsSelectionResultTemp(StringArg(req, 0), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestKeyboardLinkFollow: {
            int exitCode = 0;
            Str res = KeyboardLinkFollowResultTemp(StringArg(req, 0), StringArg(req, 1), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectTextKeyboard: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            AppendTestResult(req, 0, SelectTextKeyboardResultTemp(win));
            break;
        }

        case ControlCmd::TestCadEnhanceColors: {
            Str path = StringArg(req, 0);
            i32 pageNo = 0;
            i32 zoomPercent = 100;
            if (len(path) == 0 || !IntArg(req, 1, pageNo)) {
                AppendError(req, StrL("TestCadEnhanceColors expects string path, int pageNo [, int zoomPercent]"));
                break;
            }
            IntArg(req, 2, zoomPercent);
            int exitCode = 0;
            Str res = CadEnhanceColorsResultTemp(path, pageNo, zoomPercent, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDestZoomNav: {
            i32 destNo = 0;
            i32 startZoomPerc = 0;
            if (!IntArg(req, 0, destNo)) {
                AppendError(req, StrL("TestDestZoomNav expects int destNo (1-based) [, int startZoomPerc]"));
                break;
            }
            IntArg(req, 1, startZoomPerc);
            int exitCode = 0;
            Str res = DestZoomNavResultTemp(destNo, startZoomPerc, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTocNavigate: {
            i32 destNo = 1;
            if (!IntArg(req, 0, destNo)) {
                AppendError(req, StrL("TestTocNavigate expects int destNo (1-based)"));
                break;
            }
            int exitCode = 0;
            Str res = TocNavigateResultTemp(destNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownFollowLink: {
            Str href = StringArg(req, 0);
            i32 follow = 0;
            if (!IntArg(req, 1, follow)) {
                AppendError(req, StrL("TestMarkdownFollowLink expects string href, int follow"));
                break;
            }
            int exitCode = 0;
            Str res = MarkdownFollowLinkResultTemp(href, follow != 0, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkdownTocNavigate: {
            i32 destNo = 0;
            i32 minScrollY = 1;
            if (!IntArg(req, 0, destNo) || !IntArg(req, 1, minScrollY)) {
                AppendError(req, StrL("TestMarkdownTocNavigate expects int destNo, int minScrollY"));
                break;
            }
            int exitCode = 0;
            Str res = MarkdownTocNavigateResultTemp(destNo, minScrollY, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageInfo: {
            int exitCode = 0;
            Str res = PageInfoResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRenderSelections: {
            int exitCode = 0;
            Str res = RenderSelectionsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSeedTextSelection: {
            i32 pageNo = 1;
            if (!IntArg(req, 0, pageNo)) {
                AppendError(req, StrL("TestSeedTextSelection expects int pageNo (1-based)"));
                break;
            }
            int exitCode = 0;
            Str res = SeedTextSelectionResultTemp(pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestHiddenTabGoToPage: {
            int exitCode = 0;
            Str res = HiddenTabGoToPageResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChat: {
            i32 backend = 0;
            Str filePath = StringArg(req, 1);
            Str message = StringArg(req, 2);
            if (!IntArg(req, 0, backend) || len(message) == 0) {
                AppendError(req, StrL("TestAIChat expects int backend, string file, string message"));
                break;
            }
            int exitCode = 0;
            Str res = AIChatTestResultTemp(backend, filePath, message, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAIChatReplay: {
            int exitCode = 0;
            Str res = AIChatTestReplayResultTemp(StringArg(req, 0), StringArg(req, 1), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDisplayMode: {
            int exitCode = 0;
            Str res = DisplayModeResultTemp(StringArg(req, 0), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDocumentFontList: {
            int exitCode = 0;
            Str res = DocumentFontListResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGetPolicies: {
            str::Builder out;
            out.Append(fmt("restricted=%d\n", HasPermission(Perm::RestrictedUse) ? 1 : 0));
            out.Append(fmt("internet=%d\n", HasPermission(Perm::InternetAccess) ? 1 : 0));
            out.Append(fmt("disk=%d\n", HasPermission(Perm::DiskAccess) ? 1 : 0));
            out.Append(fmt("prefs=%d\n", HasPermission(Perm::SavePreferences) ? 1 : 0));
            out.Append(fmt("registry=%d\n", HasPermission(Perm::RegistryAccess) ? 1 : 0));
            out.Append(fmt("printer=%d\n", HasPermission(Perm::PrinterAccess) ? 1 : 0));
            out.Append(fmt("copy=%d\n", HasPermission(Perm::CopySelection) ? 1 : 0));
            out.Append(fmt("fullscreen=%d\n", HasPermission(Perm::FullscreenAccess) ? 1 : 0));
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestPageBoxes: {
            i32 pageNo = 0;
            IntArg(req, 0, pageNo);
            int exitCode = 0;
            Str res = PageBoxesResultTemp(pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestContextMenuSelection: {
            Str word1 = StringArg(req, 0);
            Str word2 = StringArg(req, 1);
            Str cursorWord = StringArg(req, 2);
            if (len(word1) == 0 || len(word2) == 0 || len(cursorWord) == 0) {
                AppendError(req,
                            StrL("TestContextMenuSelection expects string word1, string word2, string cursorWord"));
                break;
            }
            int exitCode = 0;
            Str res = ContextMenuSelectionResultTemp(word1, word2, cursorWord, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::ResolveUnsavedChanges: {
            Str action = StringArg(req, 0);
            Str path = StringArg(req, 1);
            int exitCode = 0;
            Str res = ResolveUnsavedChangesResultTemp(action, path, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestToggleFormButton: {
            i32 pageNo = 1;
            i32 idx = 0;
            if (!IntArg(req, 0, pageNo) || !IntArg(req, 1, idx)) {
                AppendError(req, StrL("TestToggleFormButton expects int pageNo (1-based), int idx (0-based)"));
                break;
            }
            int exitCode = 0;
            Str res = ToggleFormButtonResultTemp(pageNo, idx, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageComments: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            if (len(path) == 0 || !IntArg(req, 1, pageNo)) {
                AppendError(req, StrL("TestPageComments expects string path, int pageNo"));
                break;
            }
            int exitCode = 0;
            Str res = PageCommentsResultTemp(path, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestListSigningCerts: {
            int exitCode = 0;
            Str res = ListSigningCertsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSignDocument: {
            Str pdfPath = StringArg(req, 0);
            Str destPath = StringArg(req, 1);
            Str thumbprint = StringArg(req, 2);
            Str certPath = StringArg(req, 3);
            Str certPassword = StringArg(req, 4);
            Str imagePath = StringArg(req, 5);
            i32 appearanceFlags = -1;
            IntArg(req, 6, appearanceFlags);
            if (len(pdfPath) == 0 || len(destPath) == 0) {
                AppendError(req, StrL("TestSignDocument expects string pdfPath, string destPath [, thumbprint] [, "
                                      "certPath] [, password] [, imagePath] [, appearanceFlags]"));
                break;
            }
            int exitCode = 0;
            Str res = SignDocumentResultTemp(pdfPath, destPath, thumbprint, certPath, certPassword, imagePath,
                                             appearanceFlags, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDocumentSignatures: {
            int exitCode = 0;
            Str res = DocumentSignaturesResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindHistory: {
            int exitCode = 0;
            Str res = FindHistoryResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestWindowStateDuringLoad: {
            int exitCode = 0;
            Str res = WindowStateDuringLoadResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultPageColumnClip: {
            int exitCode = 0;
            Str res = FindResultPageColumnClipResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGoToFindMatch: {
            Str word = StringArg(req, 0);
            Str typed = StringArg(req, 1);
            if (len(word) == 0 || len(typed) == 0) {
                AppendError(req, StrL("TestGoToFindMatch expects string word, string typed"));
                break;
            }
            int exitCode = 0;
            Str res = GoToFindMatchResultTemp(word, typed, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindResultsOrder: {
            Str term = StringArg(req, 0);
            i32 startPage = 0;
            if (len(term) == 0 || !IntArg(req, 1, startPage)) {
                AppendError(req, StrL("TestFindResultsOrder expects string term, int startPage"));
                break;
            }
            int exitCode = 0;
            Str res = FindResultsOrderResultTemp(term, startPage, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindWindowContents: {
            i32 maxRows = 0;
            IntArg(req, 0, maxRows);
            int exitCode = 0;
            Str res = FindWindowContentsResultTemp(maxRows, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestLinkDestHighlight: {
            int exitCode = 0;
            Str res = LinkDestHighlightResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFindUiState: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            Str action = StringArg(req, 0);
            if (str::Eq(action, StrL("set")) || str::Eq(action, StrL("caret")) || str::Eq(action, StrL("enter"))) {
                int exitCode = 0;
                Str res = FindEditTestTemp(win, action, StringArg(req, 1), &exitCode);
                AppendTestResult(req, exitCode, res);
                break;
            }
            if (len(action) > 0) {
                int exitCode = 0;
                Str res = FindUiStateResultTemp(action, &exitCode);
                AppendTestResult(req, exitCode, res);
                break;
            }
            AppendTestResult(req, 0, FindStateResultTemp(win));
            break;
        }

        case ControlCmd::TestConvertToImages: {
            Str tmpl = StringArg(req, 0);
            Str pages = StringArg(req, 1);
            if (len(tmpl) == 0) {
                AppendError(req, StrL("TestConvertToImages expects string template, string pages"));
                break;
            }
            // optional; 0 means the dialog's default
            i32 dpi = 0;
            IntArg(req, 2, dpi);
            int exitCode = 0;
            Str res = ConvertPagesToImagesResultTemp(tmpl, pages, dpi, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestConvertToPdf: {
            Str src = StringArg(req, 0);
            Str dst = StringArg(req, 1);
            if (len(src) == 0 || len(dst) == 0) {
                AppendError(req, StrL("TestConvertToPdf expects string srcPath, string destPath"));
                break;
            }
            int exitCode = 0;
            Str res = ConvertImageCollectionToPdfResultTemp(src, dst, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestExtractPages: {
            i32 annotsOnly = 0;
            Str dst = StringArg(req, 0);
            Str pages = StringArg(req, 1);
            IntArg(req, 2, annotsOnly);
            if (len(dst) == 0) {
                AppendError(req, StrL("TestExtractPages expects string destPath, string pages, int annotsOnly"));
                break;
            }
            int exitCode = 0;
            Str res = ExtractPdfPagesResultTemp(dst, pages, annotsOnly, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSaveSelectionAsImage: {
            i32 dpi = 0, pageNo = 0, x = 0, y = 0, dx = 0, dy = 0;
            Str dst = StringArg(req, 0);
            if (len(dst) == 0 || !IntArg(req, 1, dpi) || !IntArg(req, 2, pageNo) || !IntArg(req, 3, x) ||
                !IntArg(req, 4, y) || !IntArg(req, 5, dx) || !IntArg(req, 6, dy)) {
                AppendError(req, StrL("TestSaveSelectionAsImage expects string dest, int dpi, page, x, y, dx, dy"));
                break;
            }
            int exitCode = 0;
            Str res = SaveSelectionAsImageResultTemp(dst, dpi, pageNo, x, y, dx, dy, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMergePdf: {
            // action, arg, n: see MergePdfResultTemp()
            Str action = StringArg(req, 0);
            Str arg = StringArg(req, 1);
            i32 n = 0;
            IntArg(req, 2, n);
            int exitCode = 0;
            Str res = MergePdfResultTemp(action, arg, n, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestFrameNcStrips: {
            int exitCode = 0;
            Str res = FrameNcStripsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestWheelWhileClosing: {
            int exitCode = 0;
            Str res = WheelWhileClosingResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestCanvasFlags: {
            Str action = StringArg(req, 0);
            if (str::EqI(action, StrL("set-grid"))) {
                i32 on = 0;
                IntArg(req, 1, on);
                SetShowPageGrid(on != 0);
            } else if (str::EqI(action, StrL("reset-grid"))) {
                ResetPageGridToDefaults();
            } else if (str::EqI(action, StrL("grid-marks"))) {
                MainWindow* win = FirstWindow();
                AppendTestResult(req, 0, fmt("marks=%d", PageGridMarkPixels(win)));
                break;
            }
            AppendTestResult(req, 0, PageGridStateTemp());
            break;
        }

        case ControlCmd::TestInvokeCommand: {
            Str name = StringArg(req, 0);
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            if (str::EqI(name, StrL("WM_CLOSE"))) {
                uitask::Post(MkFunc0(TestPostCloseWindow, win), "TestWM_CLOSE");
                AppendTestResult(req, 0, StrL("OK"));
                break;
            }
            // SB_* codes, same as the canvas wndproc. There is no scrollbar HWND.
            if (str::EqI(name, StrL("WM_HSCROLL")) || str::EqI(name, StrL("WM_VSCROLL"))) {
                bool vert = str::EqI(name, StrL("WM_VSCROLL"));
                i32 code = 0;
                i32 thumb = 0;
                IntArg(req, 1, code);
                IntArg(req, 2, thumb);
                ScrollMsg sm = ScrollMsg::None;
                if (code == 4 || code == 5) {
                    sm = ScrollMsg::ThumbTrack;
                } else if (vert) {
                    switch (code) {
                        case 0:
                            sm = ScrollMsg::LineUp;
                            break;
                        case 1:
                            sm = ScrollMsg::LineDown;
                            break;
                        case 2:
                            sm = ScrollMsg::PageUp;
                            break;
                        case 3:
                            sm = ScrollMsg::PageDown;
                            break;
                        case 6:
                            sm = ScrollMsg::Top;
                            break;
                        case 7:
                            sm = ScrollMsg::Bottom;
                            break;
                    }
                } else {
                    switch (code) {
                        case 0:
                            sm = ScrollMsg::LineLeft;
                            break;
                        case 1:
                            sm = ScrollMsg::LineRight;
                            break;
                        case 2:
                            sm = ScrollMsg::PageLeft;
                            break;
                        case 3:
                            sm = ScrollMsg::PageRight;
                            break;
                        case 6:
                            sm = ScrollMsg::Left;
                            break;
                        case 7:
                            sm = ScrollMsg::Right;
                            break;
                    }
                }
                if (sm != ScrollMsg::None) {
                    if (vert && win->IsCurrentTabAbout()) {
                        HomePageOnVScroll(win, sm, thumb);
                    } else if (vert) {
                        CanvasOnVScroll(win, sm, thumb);
                    } else {
                        CanvasOnHScroll(win, sm, thumb);
                    }
                }
                AppendTestResult(req, 0, StrL("OK"));
                break;
            }
            // No new focus: the on-screen keyboard. Keep the contents editor.
            if (str::EqI(name, StrL("WM_KILLFOCUS"))) {
                if (IsEditingAnnotContents(win)) {
                    AnnotContentsKeepOnKillFocus(win);
                }
                AppendTestResult(req, 0, StrL("OK"));
                break;
            }
            // A favorite or another custom command has no name in Commands.h.
            // Tests send its id as "#123".
            int cmdId = 0;
            if (len(name) > 1 && name.s[0] == '#') {
                str::Parse(Str(name.s + 1, name.len - 1), "%d", &cmdId);
            }
            if (cmdId <= 0) {
                cmdId = GetCommandIdByName(name);
            }
            if (cmdId <= 0 && str::IndexOfChar(name, ' ') >= 0) {
                CustomCommand* custom = CreateCommandFromDefinition(name);
                if (custom) {
                    cmdId = custom->id;
                }
            }
            if (cmdId <= 0) {
                AppendError(req, StrL("TestInvokeCommand expects a command name"));
                break;
            }
            i32 x = 0;
            i32 y = 0;
            // ng: an optional fourth argument is the index of the main window
            // the command is for. (-1, -1) means no point; any other point,
            // including one above the canvas, is delivered as on Windows.
            i32 winIdx = 0;
            if (IntArg(req, 3, winIdx) && winIdx > 0 && winIdx < len(gWindows)) {
                win = gWindows[winIdx];
            }
            if (IntArg(req, 1, x) && IntArg(req, 2, y) && !(x == -1 && y == -1)) {
                ExecuteCmdAtPoint(win, cmdId, Point{x, y});
            } else {
                ExecuteCmd(win, cmdId);
            }
            AppendTestResult(req, 0, fmt("OK handled=%d", gLastCmdFellThrough ? 0 : 1));
            break;
        }

        // ng: orig reports the win32 child window rects. The shell has no
        // child windows, so these are the rects it lays the same pieces out in
        // (same names, same format, so one script can diff the two exes)
        case ControlCmd::TestLayout: {
            // ng: with the name of an open tool window, that window's layout
            if (ToolWindowFind(StringArg(req, 0))) {
                AppendTestResult(req, 0, ToolWindowTestTemp(StrL("layout"), StringArg(req, 0), Str{}, 0, 0, 0, 0));
                break;
            }
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            str::Builder out;
            out.Append(StrL("OK count=0 watching=0\n"));
            // ng: the window's screen rect and the two modes that hide the
            // chrome, so a sweep can check fullscreen without a screenshot
            Rect scr = AppShellWindowScreenRect(win);
            out.Append(fmt("window rect=%d,%d,%d,%d maximized=%d fullscreen=%d presentation=%d scale=%.4f\n", scr.x,
                           scr.y, scr.dx, scr.dy, win->isMaximized ? 1 : 0, win->isFullScreen ? 1 : 0,
                           (int)win->presentation, CanvasScale(win)));
            WindowTab* titleTab = win->CurrentTab();
            Str frameTitle = titleTab ? titleTab->frameTitle : Str{};
            out.Append(fmt("windowTitle=%s\n", frameTitle));
#if !OS_WIN && !OS_WASM
            // frame-client dips plus this origin are screen pixels, which is
            // what a drop-down's box is reported in
            Rect content = ToolWinNativeContentRect(win->gpuiWin);
            out.Append(fmt("content origin=%d,%d\n", content.x, content.y));
#endif
            Rect sysMenu;
            if (AppShellSysMenuRect(win, &sysMenu)) {
                // orig's caption tree: row 0, child 0 is the app icon. visibility 0 is Visible.
                out.Append(fmt("layout path=%s kind=%s visibility=%d rect=%d,%d,%d,%d\n", StrL("caption/0/0"),
                               StrL("captionBtn"), 0, sysMenu.x, sysMenu.y, sysMenu.dx, sysMenu.dy));
            }
            Rect frame = win->frameRc;
            Rect canvas = win->canvasRc;
            int menuDy = (win->isMenuBarVisible && !AppShellNativeMenu()) ? kMenuBarDy : 0;
            int tabsDy = TabsAreVisible(win) ? kTabBarDy : 0;
            int toolbarDy = ToolbarDy(win);
            int sidebarDx = (win->uiState.tocVisible || win->uiState.favVisible) ? win->sidebarDx : 0;
            AppendLayoutRect(out, StrL("frame"), true, frame);
            AppendLayoutRect(out, StrL("canvas"), true, canvas);
            Rect menuRc{0, 0, frame.dx, menuDy};
            Rect tabsRc{0, menuDy, frame.dx, tabsDy};
            int topDy = menuDy + tabsDy;
            // orig's tabsInTitlebar: both are in the caption
            AppShellCaptionRects(win, &menuRc, &tabsRc, &topDy);
            AppendLayoutRect(out, StrL("menu"), menuDy > 0, menuRc);
            AppendLayoutRect(out, StrL("tabs"), tabsDy > 0, tabsRc);
            int border = AppShellFrameBorder(win);
            AppendLayoutRect(out, StrL("toolbar"), toolbarDy > 0,
                             Rect{border, topDy, frame.dx - 2 * border, toolbarDy});
            WindowTab* tab = win->CurrentTab();
            // the Favorites tab fills the canvas; orig reports that panel's hwnd
            bool favAsTab = tab && tab->IsFavoritesTab();
            AppendLayoutRect(out, StrL("favoritesTab"), favAsTab, favAsTab ? canvas : Rect{});
            SidebarContent topContent = tab ? tab->sidebarContent : SidebarContent::Bookmarks;
            int sideX = border;
            if (sidebarDx > 0 && gSettings && gSettings->sidebarOnRight) {
                sideX = canvas.x + canvas.dx + kSplitterDx;
            }
            Rect sideRc{sideX, topDy, sidebarDx, canvas.dy};
            AppendLayoutRect(out, SidebarContentToStr(topContent), win->uiState.tocVisible, sideRc);
            AppendLayoutRect(out, SidebarContentToStr(win->sidebarBottomContent), win->uiState.favVisible, sideRc);
            // orig names the panes sidebarTop / sidebarBottom, whatever they show
            AppendLayoutRect(out, StrL("sidebarTop"), win->uiState.tocVisible, sideRc);
            AppendLayoutRect(out, StrL("sidebarBottom"), win->uiState.favVisible, sideRc);
            // dips per document pixel; click tests scale canvas points by this
            out.Append(fmt("canvasScale=%.3f\n", (double)CanvasScale(win)));
            // same range GetScrollInfo reads off the canvas HWND on Windows
            out.Append(fmt("scrollV pos=%d min=%d max=%d page=%d\n", win->scrollV.nPos, win->scrollV.nMin,
                           win->scrollV.nMax, win->scrollV.nPage));
            out.Append(fmt("scrollH pos=%d min=%d max=%d page=%d\n", win->scrollH.nPos, win->scrollH.nMin,
                           win->scrollH.nMax, win->scrollH.nPage));
            DisplayModel* dm = win->AsFixed();
            if (dm) {
                out.Append(
                    fmt("pages count=%d spacing=%d,%d\n", dm->PageCount(), dm->pageSpacing.dx, dm->pageSpacing.dy));
                int n = dm->PageCount() < 8 ? dm->PageCount() : 8;
                for (int pageNo = 1; pageNo <= n; pageNo++) {
                    PageInfo* pi = dm->GetPageInfo(pageNo);
                    if (!pi) {
                        continue;
                    }
                    Rect p = pi->pos;
                    // pageOnScreen is canvas space. Tests capture the frame, and
                    // the bars sit outside the viewport, so report the visible page.
                    Rect view(Point(), dm->GetViewPort().Size());
                    Rect vis = pi->pageOnScreen.Intersect(view);
                    Rect s = FrameScreenRect(win, vis.IsEmpty() ? pi->pageOnScreen : vis);
                    out.Append(fmt("page n=%d shown=%d pos=%d,%d,%d,%d screen=%d,%d,%d,%d\n", pageNo,
                                   pi->isShown ? 1 : 0, p.x, p.y, p.dx, p.dy, s.x, s.y, s.dx, s.dy));
                }
            }
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestPerfStats: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            i32 reset = 0;
            IntArg(req, 0, reset);
            TempStr frames = AppShellFrameStatsTemp(win, reset != 0);
            double wrapMs = 0;
            int wrapCount = 0;
            CanvasBmpWrapStats(wrapMs, wrapCount);
            int nEntries = 0;
            i64 cacheBytes = gRenderCache ? gRenderCache->CacheBytes(nEntries) : 0;
            AppendTestResult(
                req, 0,
                fmt("%s bmpWrapMs=%.0f bmpWrapCount=%d cacheEntries=%d/%d cacheKB=%d firstPaintMs=%.0f", frames, wrapMs,
                    wrapCount, nEntries, kMaxBitmapsCached, (int)(cacheBytes / 1024), CanvasFirstPaintMs()));
            break;
        }

        // ng: the six canvas overlays and the two tips are drawn, so there is
        // nothing to read back from the model; this reports their flags and
        // the text of the two notifications instead of a screenshot
        case ControlCmd::TestReadAloudPlaybackBar: {
            // ng: an optional "show" / "hide" (ReadAloudPlaybackBarTestTemp)
            int exitCode = 0;
            Str res = ReadAloudPlaybackBarTestTemp(StringArg(req, 0), &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestReadingAutoScroll: {
            int exitCode = 0;
            Str res = ReadingAutoScrollBarStateTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestReadingBar: {
            int exitCode = 0;
            Str res = ReadingBarStateTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestOverlayState: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            NotificationWnd* pageInfo = GetNotificationForGroup(win, kNotifPageInfo);
            NotificationWnd* cursorPos = GetNotificationForGroup(win, kNotifCursorPos);
            str::Builder out;
            out.Append(fmt("pageInfo wanted=%d shown=%d msg=%s\n", win->pageInfoWanted ? 1 : 0, pageInfo ? 1 : 0,
                           pageInfo ? NotificationGetMessageTemp(pageInfo) : StrL("")));
            out.Append(fmt("cursorPos shown=%d next=%s msg=%s\n", cursorPos ? 1 : 0, NextCursorPositionUnitName(win),
                           cursorPos ? NotificationGetMessageTemp(cursorPos) : StrL("")));
            out.Append(fmt("pageBoxes=%d images=%d transparencyGrid=%d fitContentArea=%d links=%d\n",
                           win->showPageBoxes ? 1 : 0, ShowImageOutlines() ? 1 : 0, ShowTransparencyGrid() ? 1 : 0,
                           ShowFitContentArea() ? 1 : 0, gSettings->showLinks ? 1 : 0));
            out.Append(fmt("drawn shapes=%d\n", CanvasOverlayShapesDrawn()));
            out.Append(fmt("commandPalette=%d\n", IsCommandPaletteVisible() ? 1 : 0));
            AppendTestResult(req, 0, ToStrTemp(out));
            break;
        }

        case ControlCmd::TestSidebarThumbnails: {
            int exitCode = 0;
            Str res = SidebarThumbnailsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRefHover: {
            Str action = StringArg(req, 0);
            i32 x = 0;
            i32 y = 0;
            IntArg(req, 1, x);
            IntArg(req, 2, y);
            int exitCode = 0;
            Str res = RefHoverResultTemp(action, x, y, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestNavFiles: {
            Str action = StringArg(req, 0);
            i32 idx = -1;
            IntArg(req, 1, idx);
            int exitCode = 0;
            Str res = NavFilesInFolderStateTemp(action, idx, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAnnotFilter: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            Str action = StringArg(req, 0);
            if (str::EqI(action, StrL("set"))) {
                ApplyAnnotFilterText(win, StringArg(req, 1));
            } else if (str::EqI(action, StrL("paint"))) {
                AppShellInvalidate(win);
            } else {
                // ng: not orig's: click / dblclick / key / delete / scroll
                i32 arg = -1;
                i32 mods = 0;
                IntArg(req, 1, arg);
                IntArg(req, 2, mods);
                AnnotFilterTestAction(win, action, arg, mods);
            }
            AppendTestResult(req, 0, AnnotFilterToolbarStateTemp(win));
            break;
        }

        case ControlCmd::TestDdeExecute: {
            Str cmd = StringArg(req, 0);
            bool ok = ExecuteDdeCmds(cmd);
            AppendTestResult(req, ok ? 0 : 1, ok ? StrL("1") : StrL("0"));
            break;
        }

        case ControlCmd::TestSavePathDialog: {
            Str path = StringArg(req, 0);
            if (!IsSavePathDialogVisible()) {
                AppendTestResult(req, 2, StrL("NOTREADY no-dialog"));
                break;
            }
            bool ok = TestFinishSavePathDialog(path);
            AppendTestResult(req, 0, fmt("OK done=%d path=%s", ok ? 1 : 0, path));
            break;
        }

        case ControlCmd::TestCurrentTab: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            WindowTab* tab = win->CurrentTab();
            if (!tab || len(tab->filePath) == 0) {
                AppendTestResult(req, 2, StrL("NOTREADY no-tab"));
                break;
            }
            int page = tab->ctrl ? tab->ctrl->CurrentPageNo() : 0;
            AppendTestResult(req, 0, fmt("path=%s page=%d", tab->filePath, page));
            break;
        }

        case ControlCmd::TestCommandVisibility: {
            Str name = StringArg(req, 0);
            int cmdId = GetCommandIdByName(name);
            if (cmdId <= 0) {
                AppendError(req, StrL("TestCommandVisibility expects a command name"));
                break;
            }
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            CommandSurface surface = CommandSurface::Menu;
            Str surf = StringArg(req, 1);
            if (str::EqI(surf, StrL("palette"))) {
                surface = CommandSurface::Palette;
            } else if (str::EqI(surf, StrL("toolbar"))) {
                surface = CommandSurface::Toolbar;
            }
            Point pt{};
            i32 x = 0;
            i32 y = 0;
            if (IntArg(req, 2, x) && IntArg(req, 3, y)) {
                pt = FramePointToCanvas(win, Point{x, y});
            }
            AppCommandCtx ctx = NewAppCommandCtx(win, pt);
            CommandVisibility vis = GetCommandVisibility(cmdId, ctx, surface);
            Str visName = StrL("show");
            if (vis == CommandVisibility::Hide) {
                visName = StrL("hide");
            } else if (vis == CommandVisibility::Disable) {
                visName = StrL("disable");
            }
            AppendTestResult(req, 0, fmt("cmd=%s vis=%s", name, visName));
            break;
        }

#if OS_WIN
        case ControlCmd::TestDefaultAppNotif: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            StrVec exts;
            Split(&exts, StringArg(req, 0), StrL(","), true);
            ShowDefaultAppNotification(win, exts);
            AppendTestResult(req, 0, fmt("OK exts=%d", len(exts)));
            break;
        }
#endif

        case ControlCmd::TestAdvSettingsRows: {
            Str action = StringArg(req, 0);
            i32 arg = 0;
            IntArg(req, 1, arg);
            if (len(action) == 0) {
                AppendError(req, StrL("TestAdvSettingsRows expects string action [, int rows]"));
                break;
            }
            int exitCode = 0;
            Str res = AdvSettingsRowsResultTemp(action, arg, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMainMenu: {
            MainWindow* win = FirstWindow();
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            if (str::EqI(StringArg(req, 0), StrL("history"))) {
                AppendTestResult(req, 0, FileHistoryMenuIdsTemp(win));
                break;
            }
            AppendTestResult(req, 0, MainMenuResultTemp(win));
            break;
        }

        case ControlCmd::TestCommandPalette: {
            int exitCode = 0;
            Str res = CommandPaletteStateTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestContextMenuAt: {
            MainWindow* win = FirstWindow();
            i32 x = 0;
            i32 y = 0;
            if (!win) {
                AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                break;
            }
            IntArg(req, 0, x);
            IntArg(req, 1, y);
            AppendTestResult(req, 0, ContextMenuAtPointResultTemp(win, x, y));
            break;
        }

        case ControlCmd::TestDocumentProperties: {
            int exitCode = 0;
            Str action = StringArg(req, 0);
            Str res = DocumentPropertiesResultTemp(&exitCode);
            if (str::Eq(action, StrL("buttons"))) {
                res = PropertiesDialogButtonsTemp(&exitCode);
            } else if (str::Eq(action, StrL("text"))) {
                res = PropertiesDialogTextTemp(&exitCode);
            }
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestInput: {
            MainWindow* win = FirstWindow();
            Str kind = StringArg(req, 0);
            // WM_SETTEXT: the edit is a gpui field, and the test's pointer is
            // not this process's.
            if (str::Eq(kind, StrL("text"))) {
                bool ok = false;
                if (win) {
                    WStr w = ToWStrTemp(StringArg(req, 1));
                    ok = FreeTextInPlaceSetText(win, w.s) || CommandPaletteSetText(win, w.s);
                }
                AppendTestResult(req, ok ? 0 : 1, ok ? StrL("OK") : StrL("ERR text"));
                break;
            }
            i32 a = 0, b = 0, c = 0, d = 0;
            IntArg(req, 1, a);
            IntArg(req, 2, b);
            IntArg(req, 3, c);
            IntArg(req, 4, d);
            // WM_CHAR LF commits the in-place editor before gpui sees it.
            if (str::Eq(kind, StrL("char")) && a == '\n' && FreeTextInPlaceCommitOnChar(win, a)) {
                AppendTestResult(req, 0, StrL("OK"));
                break;
            }
            AppendTestResult(req, 0, AppShellTestInput(win, kind, a, b, c, d));
            break;
        }

        case ControlCmd::TestUiState: {
            Str op = StringArg(req, 0);
            if (len(op) > 0) {
                i32 arg = 0;
                IntArg(req, 1, arg);
                if (str::Eq(op, StrL("ctxcmd"))) {
                    WindowContextMenuCommand(FirstWindow(), arg);
                    AppendTestResult(req, 0, StrL("ok"));
                    break;
                }
                // mac has no HWND to MoveWindow. x,y,dx,dy then the window index.
                if (str::Eq(op, StrL("place"))) {
                    i32 y = 0, dx = 0, dy = 0, winIdx = 0;
                    IntArg(req, 2, y);
                    IntArg(req, 3, dx);
                    IntArg(req, 4, dy);
                    IntArg(req, 5, winIdx);
                    MainWindow* placed = (winIdx >= 0 && winIdx < len(gWindows)) ? gWindows[winIdx] : nullptr;
                    if (!placed || dx <= 0 || dy <= 0) {
                        AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                        break;
                    }
                    AppShellPlaceWindow(placed, Rect{arg, y, dx, dy}, false);
                    AppendTestResult(req, 0, StrL("OK"));
                    break;
                }
                AppendTestResult(req, 0, SidebarTestToc(FirstWindow(), op, arg));
                break;
            }
            AppendTestResult(req, 0, AppShellUiStateTemp(FirstWindow()));
            break;
        }

        case ControlCmd::TestToolWindow: {
            if (str::Eq(StringArg(req, 0), StrL("sign-placement"))) {
                AppendTestResult(req, 0, SignDocumentPlacementTemp());
                break;
            }
            if (str::Eq(StringArg(req, 0), StrL("sign-checks"))) {
                AppendTestResult(req, 0, SignDocumentChecksTemp());
                break;
            }
            i32 a = 0, b = 0, c = 0, d = 0;
            IntArg(req, 3, a);
            IntArg(req, 4, b);
            IntArg(req, 5, c);
            IntArg(req, 6, d);
            Str res = ToolWindowTestTemp(StringArg(req, 0), StringArg(req, 1), StringArg(req, 2), a, b, c, d);
            AppendTestResult(req, 0, res);
            break;
        }

#if OS_WIN
        case ControlCmd::TestNativeMsgBox: {
            i32 id = 0;
            IntArg(req, 1, id);
            AppendTestResult(req, 0, NativeMsgBoxTestTemp(StringArg(req, 0), id));
            break;
        }

        case ControlCmd::TestNativeFileDlg: {
            AppendTestResult(req, 0, NativeFileDlgTestTemp(FirstWindow(), StringArg(req, 0), StringArg(req, 1)));
            break;
        }

        case ControlCmd::TestOleDragDrop: {
            i32 x = 0, y = 0;
            IntArg(req, 2, x);
            IntArg(req, 3, y);
            if (str::Eq(StringArg(req, 0), StrL("cursor"))) {
                AppendTestResult(req, 0, NativeCursorTestTemp(FirstWindow(), StringArg(req, 1)));
                break;
            }
            // ng: a fifth argument names the tool window to drop on
            AppendTestResult(
                req, 0,
                OleDragDropTestTemp(FirstWindow(), StringArg(req, 0), StringArg(req, 1), x, y, StringArg(req, 4)));
            break;
        }
#endif

        case ControlCmd::TestToolbarButtons: {
            int exitCode = 0;
            Str res = ToolbarButtonsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestMarkupAnnots: {
            Str action = StringArg(req, 0);
            i32 x = 0;
            i32 y = 0;
            if (len(action) > 0 && (!IntArg(req, 1, x) || !IntArg(req, 2, y))) {
                AppendError(req, StrL("TestMarkupAnnots expects action, x, y"));
                break;
            }
            int exitCode = 0;
            Str res = MarkupAnnotsResultTemp(action, x, y, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestAnnotEditorLayout: {
            i32 clientDy = 0;
            i32 selectItem = 0;
            i32 selectLast = 0;
            IntArg(req, 0, clientDy);
            IntArg(req, 1, selectItem);
            IntArg(req, 2, selectLast);
            int exitCode = 0;
            Str res = AnnotEditorLayoutResultTemp(clientDy, selectItem, &exitCode, selectLast);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestHomeSelection: {
            // a window index selects which frame to describe. It follows the
            // view-mode string when that is present.
            Str mode = StringArg(req, 0);
            i32 winIdx = 0;
            if (len(mode) > 0) {
                SetHomePageListView(str::EqI(mode, StrL("list")));
                if (MainWindow* win = FirstWindow()) {
                    AppShellInvalidate(win);
                }
                IntArg(req, 1, winIdx);
            } else {
                IntArg(req, 0, winIdx);
            }
            int exitCode = 0;
            Str res = HomeSelectionForWindowTemp(&exitCode, winIdx);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestHomeListRows: {
            int exitCode = 0;
            Str res = HomeListRowsResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSidebarLayout: {
            int exitCode = 0;
            Str res = SidebarLayoutResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageInfoOverlay: {
            Str pathTwo = StringArg(req, 0);
            Str pathOne = StringArg(req, 1);
            if (len(pathTwo) == 0 || len(pathOne) == 0) {
                AppendError(req, StrL("TestPageInfoOverlay expects string pathTwoPages, string pathOnePage"));
                break;
            }
            int exitCode = 0;
            Str res = PageInfoOverlayResultTemp(pathTwo, pathOne, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestPageLinks: {
            Str path = StringArg(req, 0);
            i32 pageNo = 1;
            if (len(path) == 0 || !IntArg(req, 1, pageNo)) {
                AppendError(req, StrL("TestPageLinks expects string path, int pageNo"));
                break;
            }
            int exitCode = 0;
            Str res = PageLinksResultTemp(path, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestImageRenderEdges: {
            Str path = StringArg(req, 0);
            i32 zoomPercent = 100;
            i32 clipKind = 0;
            if (len(path) == 0) {
                AppendError(req, StrL("TestImageRenderEdges expects string path [, int zoomPercent] [, int clipKind]"));
                break;
            }
            IntArg(req, 1, zoomPercent);
            IntArg(req, 2, clipKind);
            int exitCode = 0;
            Str res = ImageRenderEdgesResultTemp(path, zoomPercent, clipKind, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestImageResizeArrowKey: {
            Str imagePath = StringArg(req, 0);
            if (len(imagePath) == 0) {
                AppendError(req, StrL("TestImageResizeArrowKey expects string imagePath"));
                break;
            }
            int exitCode = 0;
            Str res = ImageResizeArrowKeyResultTemp(imagePath, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestImageResizeEdges: {
            Str imagePath = StringArg(req, 0);
            i32 newW = 0;
            i32 newH = 0;
            if (len(imagePath) == 0 || !IntArg(req, 1, newW) || !IntArg(req, 2, newH)) {
                AppendError(req, StrL("TestImageResizeEdges expects string imagePath, int newW, int newH"));
                break;
            }
            int exitCode = 0;
            Str res = ImageResizeEdgesResultTemp(imagePath, newW, newH, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionVars: {
            Str pattern = StringArg(req, 0);
            int exitCode = 0;
            Str res = SelectionVarsResultTemp(pattern, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRotatedTextMouseDrag: {
            Str word = StringArg(req, 0);
            if (len(word) == 0) {
                AppendError(req, StrL("TestRotatedTextMouseDrag expects string word"));
                break;
            }
            int exitCode = 0;
            Str res = RotatedTextMouseDragResultTemp(word, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestSelectionToolbar: {
            Str action = StringArg(req, 0);
            if (action) {
                int exitCode = 0;
                Str res = SelectionToolbarClickTemp(action, &exitCode);
                AppendTestResult(req, exitCode, res);
                break;
            }
            AppendTestResult(req, 0, SelectionToolbarLayoutDumpTemp());
            break;
        }

        case ControlCmd::TestChapterInfo: {
            int exitCode = 0;
            Str res = ChapterInfoResultTemp(&exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestGoToLocation: {
            i32 chapter = 0;
            i32 page = 0;
            if (!IntArg(req, 0, chapter) || !IntArg(req, 1, page)) {
                AppendError(req, StrL("TestGoToLocation expects int chapter, int page"));
                break;
            }
            int exitCode = 0;
            Str res = GoToLocationResultTemp(chapter, page, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestTtsEngineCrash: {
            Str action = StringArg(req, 0);
            if (str::EqI(action, StrL("crash"))) {
                str::ReplaceWithCopy(&gSettings->readAloudVoiceId, StrL("test-voice"));
                if (!TtsTestEngineCrash()) {
                    AppendTestResult(req, 1, StrL("FAIL could not start the crashing thread"));
                    break;
                }
            }
            TempStr state = fmt("crashed=%d voice='%s'", (int)TtsEngineCrashed(), gSettings->readAloudVoiceId);
            AppendTestResult(req, 0, state);
            break;
        }

        case ControlCmd::TestTtsPumpOnSpeak: {
            TtsTestPumpOnNextSpeak();
            // mac sendMessage runs at once, so the close would be sent only
            // after speak returned. Queue it, then speak, and the pump runs it.
            if (str::Eq(StringArg(req, 0), StrL("close-during-speak"))) {
                MainWindow* win = FirstWindow();
                if (!win) {
                    AppendTestResult(req, 2, StrL("NOTREADY no-window"));
                    break;
                }
                uitask::Post(MkFunc0(TestPostCloseWindow, win), "TestWM_CLOSE");
                ExecuteCmd(win, CmdReadAloudFromTopPage);
            }
            AppendTestResult(req, 0, StrL("OK"));
            break;
        }

        case ControlCmd::StartPerfLog:
            StartPerfLog();
            AppendTestResult(req, 0, StrL("OK"));
            break;

        case ControlCmd::StopPerfLog:
            StopPerfLog();
            AppendTestResult(req, 0, StrL("OK"));
            break;

        case ControlCmd::TestImageOrientation: {
            Str pdfPath = StringArg(req, 0);
            i32 pageNo = 0;
            if (len(pdfPath) == 0 || !IntArg(req, 1, pageNo) || pageNo < 1) {
                AppendError(req, StrL("TestImageOrientation expects string pdfPath, int pageNo"));
                break;
            }
            int exitCode = 0;
            Str res = ImageOrientationResultTemp(pdfPath, pageNo, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestCmykImageSave: {
            Str jpegPath = StringArg(req, 0);
            Str tiffPath = StringArg(req, 1);
            if (len(jpegPath) == 0 || len(tiffPath) == 0) {
                AppendError(req, StrL("TestCmykImageSave expects string jpegPath, string tiffPath"));
                break;
            }
            int exitCode = 0;
            Str res = CmykImageSaveResultTemp(jpegPath, tiffPath, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestRenderViewPrint: {
            Str path = StringArg(req, 0);
            if (len(path) == 0) {
                AppendError(req, StrL("TestRenderViewPrint expects string path"));
                break;
            }
            int exitCode = 0;
            Str res = PageRenderViewPrintResultTemp(path, &exitCode);
            AppendTestResult(req, exitCode, res);
            break;
        }

        case ControlCmd::TestDpi: {
#if OS_WIN
            Str action = StringArg(req, 0);
            int exitCode = 0;
            Str res = DpiResultTemp(action, &exitCode);
            AppendTestResult(req, exitCode, res);
#else
            AppendTestResult(req, 1, StrL("NOTPORTED 71"));
#endif
            break;
        }

        default:
            // ng: orig answers the rest through SumatraTest.cpp and the win32
            // layout probes; neither is in this port (step 18)
            AppendTestResult(req, 1, fmt("NOTPORTED %d", (int)req->cmd));
            break;
    }
    req->done.Set();
}

static void SnapshotRenderIdle(ControlRequest* req) {
    req->idleState = RenderIdleState::NotReady;
    req->idleInfo[0] = 0;
#if OS_WIN
    if (gIsStartup) {
        // LoadOnStartup applies -zoom after the first paint; a snapshot
        // during that window would see the default-zoom tiles as "done"
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("startup"));
        req->done.Set();
        return;
    }
#endif
    MainWindow* win = FirstWindow();
    if (!win || !win->IsDocLoaded()) {
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("no-doc"));
        req->done.Set();
        return;
    }
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        // ebook / CHM / etc.: nothing in RenderCache to wait for
        req->idleState = RenderIdleState::Idle;
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("no-fixed"));
        req->done.Set();
        return;
    }
    // A posted invalidate has not drawn yet. Paint so missing tiles get
    // requested, then drain the ones that finished during that paint.
    AppShellInvalidate(win);
#if OS_WIN
    if (HWND hwnd = MainWindowHwnd(win)) {
        UpdateWindow(hwnd);
    }
#endif
    uitask::DrainQueue();

    float zoomV = dm->GetZoomVirtual(true);
    int pageNo = dm->FirstVisiblePageNo();
    if (pageNo < 1) {
        pageNo = 1;
    }
    float zoomR = dm->GetZoomReal(pageNo);
    int res = gRenderCache ? (int)gRenderCache->GetTileRes(dm, pageNo) : 0;
    Size vp = dm->GetViewPort().Size();
    Str whyNot;
    bool busy = gRenderCache && gRenderCache->IsBusyFor(dm);
    bool ready = false;
    // LoadDocument Relayouts before the canvas has a real size; fit zoom then
    // stays unset and no page is visible. That is not idle (issue-1203).
    if (dm->zoomReal < 0.01f || dm->GetCanvasSize().IsEmpty() || vp.IsEmpty()) {
        whyNot = StrL("no-layout");
    } else {
        ready = gRenderCache && !busy && gRenderCache->VisibleTargetTilesReady(dm, &whyNot);
    }
    if (busy) {
        whyNot = StrL("rendering");
    }
    if (win->scrollAnimActive) {
        whyNot = StrL("scrolling");
        ready = false;
    }
    // The test captures as soon as this returns. Present the ready frame into
    // each swapchain buffer; one present leaves PrintWindow on the previous one.
    if (ready) {
#if OS_WIN
        if (HWND hwnd = MainWindowHwnd(win)) {
            for (int i = 0; i < 3; i++) {
                AppShellInvalidate(win);
                UpdateWindow(hwnd);
            }
        }
#else
        AppShellInvalidate(win);
#endif
    }
    int nQ = gRenderCache ? gRenderCache->requestCount : -1;
    TempStr busyInfo = gRenderCache ? gRenderCache->BusyInfoTemp(dm) : str::DupTemp(StrL(""));
    str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)),
                fmt("zoomV=%.1f zoomR=%.3f res=%d vp=%dx%d ready=%d q=%d why=%s %s", zoomV, zoomR, res, vp.dx, vp.dy,
                    ready ? 1 : 0, nQ, whyNot, busyInfo));
    req->idleState = ready ? RenderIdleState::Idle : (gRenderCache ? RenderIdleState::Busy : RenderIdleState::NotReady);
    req->done.Set();
}

// A tab is still coming in, or startup has not finished opening the session.
static bool SessionRestorePending() {
#if OS_WIN
    if (gIsStartup) {
        return true;
    }
#endif
    if (len(gWindows) == 0) {
        return true;
    }
    for (MainWindow* win : gWindows) {
        if (!win) {
            continue;
        }
        for (WindowTab* tab : win->Tabs()) {
            if (!tab) {
                continue;
            }
            if (tab->loadState == WindowTab::LoadState::Loading ||
                tab->loadState == WindowTab::LoadState::LoadedPending) {
                return true;
            }
        }
    }
    return false;
}

static void SnapshotSessionRestore(ControlRequest* req) {
    req->idleState = RenderIdleState::NotReady;
    req->idleInfo[0] = 0;
    if (SessionRestorePending()) {
#if OS_WIN
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), gIsStartup ? StrL("startup") : StrL("loading"));
#else
        str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("loading"));
#endif
        req->done.Set();
        return;
    }
    req->idleState = RenderIdleState::Idle;
    str::BufSet(Str(req->idleInfo, dimofi(req->idleInfo)), StrL("restored"));
    req->done.Set();
}

// Block on the control thread until the restored session's tabs have loaded.
static void RunWaitSessionRestored(ControlRequest* req) {
    i32 timeoutMs = 15000;
    IntArg(req, 0, timeoutMs);
    if (timeoutMs < 1) {
        timeoutMs = 1;
    }
    u64 deadline = GetTickCount64() + (u64)timeoutMs;
    for (;;) {
        req->done.Reset();
        uitask::Post(MkFunc0<ControlRequest>(SnapshotSessionRestore, req), "WaitSessionRestored");
        req->done.Wait();
        if (req->idleState == RenderIdleState::Idle) {
            AppendTestResult(req, 0, req->idleInfo[0] ? Str(req->idleInfo) : StrL("restored"));
            return;
        }
        if (GetTickCount64() >= deadline) {
            AppendTestResult(req, 1, req->idleInfo[0] ? fmt("timeout %s", Str(req->idleInfo)) : StrL("timeout"));
            return;
        }
        SleepInMs(20);
    }
}

// Block on the control thread until visible tiles are cached at target
// resolution, or until timeoutMs. Optional first int arg is the timeout.
static void RunWaitRenderIdle(ControlRequest* req) {
    i32 timeoutMs = 15000;
    IntArg(req, 0, timeoutMs);
    if (timeoutMs < 1) {
        timeoutMs = 1;
    }
    u64 deadline = GetTickCount64() + (u64)timeoutMs;
    for (;;) {
        req->done.Reset();
        uitask::Post(MkFunc0<ControlRequest>(SnapshotRenderIdle, req), "WaitRenderIdle");
        req->done.Wait();
        if (req->idleState == RenderIdleState::Idle) {
            AppendTestResult(req, 0, req->idleInfo[0] ? Str(req->idleInfo) : StrL("idle"));
            return;
        }
        if (GetTickCount64() >= deadline) {
            Str kind = req->idleState == RenderIdleState::NotReady ? StrL("timeout-notready") : StrL("timeout-busy");
            AppendTestResult(req, 1, req->idleInfo[0] ? fmt("%s %s", kind, Str(req->idleInfo)) : kind);
            return;
        }
        SleepInMs(20);
    }
}

#if OS_WIN
using ControlConn = HANDLE;
#else
// a connected unix domain socket
using ControlConn = int;
#endif

#if OS_WIN
static bool ReadExact(HANDLE h, void* data, DWORD n) {
    u8* d = (u8*)data;
    DWORD total = 0;
    while (total < n) {
        DWORD nRead = 0;
        if (!ReadFile(h, d + total, n - total, &nRead, nullptr) || nRead == 0) {
            return false;
        }
        total += nRead;
    }
    return true;
}

static bool WriteExact(HANDLE h, Str data) {
    const u8* d = (const u8*)data.s;
    int total = 0;
    while (total < data.len) {
        DWORD nWritten = 0;
        if (!WriteFile(h, d + total, (DWORD)(data.len - total), &nWritten, nullptr) || nWritten == 0) {
            return false;
        }
        total += (int)nWritten;
    }
    return true;
}

#else
static bool ReadExact(int fd, void* data, u32 n) {
    u8* d = (u8*)data;
    u32 total = 0;
    while (total < n) {
        ssize_t nRead = read(fd, d + total, n - total);
        if (nRead < 0 && errno == EINTR) {
            continue;
        }
        if (nRead <= 0) {
            return false;
        }
        total += (u32)nRead;
    }
    return true;
}

static bool WriteExact(int fd, Str data) {
    int total = 0;
    while (total < data.len) {
        ssize_t nWritten = send(fd, data.s + total, (size_t)(data.len - total), MSG_NOSIGNAL);
        if (nWritten < 0 && errno == EINTR) {
            continue;
        }
        if (nWritten <= 0) {
            return false;
        }
        total += (int)nWritten;
    }
    return true;
}
#endif

static ControlRequest* ReadControlRequest(ControlConn h) {
    u32 size = 0;
    if (!ReadExact(h, &size, sizeof(size))) {
        return nullptr;
    }
    if (size < 4 || size > 16 * 1024 * 1024) {
        return nullptr;
    }
    u8* data = AllocArray<u8>((int)size);
    if (!ReadExact(h, data, size)) {
        free(data);
        return nullptr;
    }

    PacketReader r{data, size};
    ControlRequest* req = new ControlRequest();
    if (!r.ReadU16(req->cmd) || !r.ReadU16(req->reqId) || !ParseArgList(r, &req->args, false)) {
        DeleteControlRequest(req);
        free(data);
        return nullptr;
    }
    free(data);
    return req;
}

static bool WriteControlResponse(ControlConn h, ControlRequest* req) {
    str::Builder payload;
    AppendU16(payload, req->reqId);
    payload.Append(ToStr(req->results));

    str::Builder packet;
    AppendU32(packet, (u32)len(payload));
    packet.Append(ToStr(payload));
    return WriteExact(h, ToStr(packet));
}

// One request. false closes the connection. *stop ends the listener thread
// (Quit), which otherwise blocks in ConnectNamedPipe through ASan shutdown.
static bool HandleControlRequest(ControlConn h, bool* stop) {
    *stop = false;
    ControlRequest* req = ReadControlRequest(h);
    if (!req) {
        return false;
    }
    bool isQuit = (ControlCmd)req->cmd == ControlCmd::Quit;
    if ((ControlCmd)req->cmd == ControlCmd::CrashMe) {
        log(StrL("ControlCmd::CrashMe\n"));
        CrashMe();
    }
    // WaitRenderIdle polls on this thread so the UI thread stays free to
    // paint (and thereby request the tiles we are waiting for)
    if ((ControlCmd)req->cmd == ControlCmd::WaitRenderIdle) {
        RunWaitRenderIdle(req);
    } else if ((ControlCmd)req->cmd == ControlCmd::WaitSessionRestored) {
        RunWaitSessionRestored(req);
    } else {
        uitask::Post(MkFunc0<ControlRequest>(ExecuteControlRequest, req), "SumatraControl");
        req->done.Wait();
    }
    bool ok = WriteControlResponse(h, req);
    DeleteControlRequest(req);
    if (!ok) {
        return false;
    }
    if (isQuit) {
        *stop = true;
        return false;
    }
    return true;
}

static bool ProcessControlConnection(ControlConn h) {
    for (;;) {
        bool stop = false;
        if (!HandleControlRequest(h, &stop)) {
            return stop;
        }
    }
}

#if OS_WIN
static WStr FullPipeNameOwned(Str pipeName) {
    if (str::StartsWith(pipeName, StrL(R"(\\.\pipe\)"))) {
        return ToWStr(pipeName);
    }
    TempStr fullName = str::JoinTemp(StrL(R"(\\.\pipe\)"), pipeName);
    return ToWStr(fullName);
}

struct ControlThreadArg {
    Str pipeName;
};

static void SumatraControlThread(ControlThreadArg* arg) {
    WStr pipeNameW = FullPipeNameOwned(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;

    for (;;) {
        HANDLE pipe = CreateNamedPipeW(pipeNameW.s, PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                                       1, 64 * 1024, 64 * 1024, 0, nullptr);
        if (pipe == INVALID_HANDLE_VALUE) {
            logf("CreateNamedPipeW failed for control pipe, err=%u\n", (unsigned)GetLastError());
            return;
        }
        BOOL connected = ConnectNamedPipe(pipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        bool stop = false;
        if (connected) {
            stop = ProcessControlConnection(pipe);
        }
        // DisconnectNamedPipe discards data the client hasn't read yet; wait
        // until it has, or the Quit reply is lost and the client sees EPIPE
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        if (stop) {
            return;
        }
    }
}

#else

// ng: off Windows the channel is a unix domain socket: <name> is its path, or,
// without a '/', /tmp/<name>.sock (cmd/dbg-control.ts does the same)
static TempStr ControlSocketPathTemp(Str pipeName) {
    if (str::IndexOfChar(pipeName, '/') >= 0) {
        return str::DupTemp(pipeName);
    }
    return fmt("/tmp/%s.sock", pipeName);
}

struct ControlThreadArg {
    Str pipeName;
};

static void SumatraControlThread(ControlThreadArg* arg) {
    TempStr sockPath = ControlSocketPathTemp(arg->pipeName);
    str::FreePtr(&arg->pipeName);
    delete arg;

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (len(sockPath) >= (int)sizeof(addr.sun_path)) {
        logf("control socket path is too long: '%s'\n", sockPath);
        return;
    }
    memcpy(addr.sun_path, sockPath.s, (size_t)len(sockPath));

    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listener < 0) {
        logf("socket() failed for control socket, errno=%d\n", errno);
        return;
    }
    // a stale socket file from a run that did not clean up
    unlink(addr.sun_path);
    if (bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0 || listen(listener, 4) != 0) {
        logf("bind() / listen() failed for control socket '%s', errno=%d\n", sockPath, errno);
        close(listener);
        return;
    }
    // The async client stays connected. Window messages use a second socket,
    // so the listener has to poll every connection instead of reading one
    // until it disconnects.
    constexpr int kMaxConns = 8;
    int conns[kMaxConns];
    int nConns = 0;
    bool stop = false;
    while (!stop) {
        pollfd fds[1 + kMaxConns];
        fds[0].fd = listener;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        for (int i = 0; i < nConns; i++) {
            fds[i + 1].fd = conns[i];
            fds[i + 1].events = POLLIN;
            fds[i + 1].revents = 0;
        }
        int pr = poll(fds, (nfds_t)(1 + nConns), -1);
        if (pr < 0) {
            if (errno == EINTR) {
                continue;
            }
            logf("poll() failed for control socket, errno=%d\n", errno);
            break;
        }
        if ((fds[0].revents & POLLIN) != 0) {
            int conn = accept(listener, nullptr, nullptr);
            if (conn >= 0) {
                if (nConns < kMaxConns) {
                    conns[nConns++] = conn;
                } else {
                    close(conn);
                }
            } else if (errno != EINTR) {
                logf("accept() failed for control socket, errno=%d\n", errno);
                break;
            }
        }
        for (int i = 0; i < nConns;) {
            short rev = fds[i + 1].revents;
            if ((rev & (POLLIN | POLLHUP | POLLERR | POLLNVAL)) == 0) {
                i++;
                continue;
            }
            bool quit = false;
            bool keep = (rev & POLLIN) != 0 && HandleControlRequest(conns[i], &quit);
            if (quit) {
                stop = true;
            }
            if (keep && !stop) {
                i++;
                continue;
            }
            // the client reads the reply (to Quit, too) before it sees the close.
            // fds[] still describes this slot, so the swapped connection waits
            // for the next poll.
            shutdown(conns[i], SHUT_WR);
            close(conns[i]);
            nConns--;
            conns[i] = conns[nConns];
            i++;
        }
    }
    for (int i = 0; i < nConns; i++) {
        shutdown(conns[i], SHUT_WR);
        close(conns[i]);
    }
    close(listener);
    unlink(addr.sun_path);
}
#endif

void StartSumatraControl(Str pipeName) {
    if (len(pipeName) == 0) {
        return;
    }
    logf("StartSumatraControl: pipe '%s'\n", pipeName);
#if OS_WIN
    // a script cannot answer a system message box: it gets the port's own
    // until it asks for the system's (TestNativeMsgBox on)
    NativeMsgBoxSetEnabled(false);
#endif
    auto* arg = new ControlThreadArg{str::Dup(pipeName)};
    RunAsync(MkFunc0(SumatraControlThread, arg), StrL("SumatraControl"));
}

#else

// ng: no threads, so nothing to listen on
void StartSumatraControl(Str pipeName) {
    if (len(pipeName) > 0) {
        log(StrL("-dbg-control is not supported on this platform\n"));
    }
}

#endif
