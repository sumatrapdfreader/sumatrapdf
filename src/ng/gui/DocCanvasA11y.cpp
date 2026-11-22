/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// ng: orig's src/uia/DocumentProvider.cpp and PageProvider.cpp. The canvas is
// an accessibility document (UI Automation, AT-SPI, NSAccessibility) whose text
// is the text of all pages, with a part per page. Both are read when a screen
// reader asks, not per frame.

#include "gui/GpuiBridge.h"
#include "base/File.h"

#include "gui/UIModels.h"
#include "Settings.h"
#include "DisplayMode.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"
#include "MainWindow.h"
#include "gui/DocCanvas.h"

// one per window with a document canvas
struct DocA11y {
    MainWindow* win = nullptr;
    // what the text is for; when any of it changes, the text is rebuilt
    DisplayModel* dm = nullptr;
    EngineBase* engine = nullptr;
    int layoutGen = 0;
    int pageCount = 0;
    u64 textRevision = 0;
    // the canvas in window coordinates, written by layout
    gp::Bounds canvasBounds;

    bool built = false;
    // pages joined by '\n'
    str::Builder text;
    // where page n's text starts in text, and its length, at [n - 1]
    Vec<int> pageStart;
    Vec<int> pageLen;
};

static Vec<DocA11y*> gDocA11ys;

static DocA11y* FindDocA11y(MainWindow* win) {
    for (DocA11y* a : gDocA11ys) {
        if (a->win == win) {
            return a;
        }
    }
    return nullptr;
}

// the callbacks run when a screen reader asks, maybe after the window, or the
// document the last frame showed, is gone
static DocA11y* LiveDocA11y(void* data) {
    for (DocA11y* a : gDocA11ys) {
        if (a == data) {
            bool live = IsMainWindowValid(a->win) && a->win->AsFixed() == a->dm;
            return live ? a : nullptr;
        }
    }
    return nullptr;
}

static void ResetText(DocA11y* a) {
    a->built = false;
    a->text.Reset();
    VecReset(a->pageStart);
    VecReset(a->pageLen);
}

static void BuildText(DocA11y* a) {
    if (a->built) {
        return;
    }
    for (int pageNo = 1; pageNo <= a->pageCount; pageNo++) {
        if (pageNo > 1) {
            a->text.AppendChar('\n');
        }
        Str s = a->engine->GetTextForPage(pageNo);
        VecAppend(a->pageStart, len(a->text));
        VecAppend(a->pageLen, len(s));
        a->text.Append(s);
    }
    a->built = true;
}

// (page, glyph) -> UTF-8 offset in the joined text
static int TextOffset(DocA11y* a, int pageNo, int glyph) {
    if (pageNo < 1) {
        return 0;
    }
    if (pageNo > a->pageCount) {
        return len(a->text);
    }
    int start = a->pageStart[pageNo - 1];
    Str page(a->text.els + start, a->pageLen[pageNo - 1]);
    return start + Utf8CodepointToByteIndex(page, std::max(glyph, 0));
}

static gp::Str DocA11yText(void* data) {
    DocA11y* a = LiveDocA11y(data);
    if (!a) {
        return {};
    }
    BuildText(a);
    return ToGpui(ToStr(a->text));
}

static bool HasTextSelection(DisplayModel* dm) {
    return dm->textSelection && len(dm->textSelection->result) > 0;
}

// orig's DocumentProvider::GetSelection: dm->textSelection
static bool DocA11ySelection(void* data, int* start, int* end) {
    DocA11y* a = LiveDocA11y(data);
    if (!a || !HasTextSelection(a->dm)) {
        return false;
    }
    BuildText(a);
    int fromPage, fromGlyph, toPage, toGlyph;
    a->dm->textSelection->GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);
    *start = TextOffset(a, fromPage, fromGlyph);
    *end = TextOffset(a, toPage, toGlyph);
    return true;
}

// changes whenever the selection does; 0 is no selection
static u64 SelectionRevision(DisplayModel* dm) {
    if (!HasTextSelection(dm)) {
        return 0;
    }
    TextSelection* sel = dm->textSelection;
    int v[4] = {sel->startPage, sel->startGlyph, sel->endPage, sel->endGlyph};
    return MurmurHash2(v, (int)sizeof(v)) | 1;
}

static int DocA11yPageCount(void* data) {
    DocA11y* a = LiveDocA11y(data);
    return a ? a->pageCount : 0;
}

// orig's PageProvider: "Page N"
static gp::Str DocA11yPageName(void* data, int part) {
    if (!LiveDocA11y(data)) {
        return {};
    }
    return ToGpui(fmt("Page %d", part + 1));
}

static gp::Str DocA11yPageText(void* data, int part) {
    DocA11y* a = LiveDocA11y(data);
    if (!a) {
        return {};
    }
    return ToGpui(a->engine->GetTextForPage(part + 1));
}

// the page's pageOnScreen, from canvas pixels to window dips
static bool DocA11yPageBounds(void* data, int part, gp::Bounds* out) {
    DocA11y* a = LiveDocA11y(data);
    const PageInfo* page = a ? a->dm->GetPageInfo(part + 1) : nullptr;
    if (!page) {
        return false;
    }
    float k = CanvasScale(a->win);
    Rect r = page->pageOnScreen;
    *out = {a->canvasBounds.x + (float)r.x * k, a->canvasBounds.y + (float)r.y * k, (float)r.dx * k, (float)r.dy * k};
    return true;
}

static bool DocA11yPageRange(void* data, int part, int* start, int* end) {
    DocA11y* a = LiveDocA11y(data);
    if (!a) {
        return false;
    }
    BuildText(a);
    if (part < 0 || part >= len(a->pageStart)) {
        return false;
    }
    *start = a->pageStart[part];
    *end = *start + a->pageLen[part];
    return true;
}

// per frame: cheap, the text is only built by DocA11yText
void DocCanvasA11y(MainWindow* win, gp::Ctx* cx, gp::El* canvas) {
    DisplayModel* dm = win->AsFixed();
    if (!dm) {
        return;
    }
    DocA11y* a = FindDocA11y(win);
    if (!a) {
        a = new DocA11y();
        a->win = win;
        VecAppend(gDocA11ys, a);
    }

    EngineBase* engine = dm->GetEngine();
    int gen = engine->LayoutGeneration();
    int pageCount = dm->PageCount();
    bool same = a->dm == dm && a->engine == engine && a->layoutGen == gen && a->pageCount == pageCount;
    if (!same) {
        a->dm = dm;
        a->engine = engine;
        a->layoutGen = gen;
        a->pageCount = pageCount;
        a->textRevision++;
        ResetText(a);
    }

    gp::AccessibilityDocumentSource src;
    src.text = &DocA11yText;
    src.selection = &DocA11ySelection;
    src.data = a;
    src.textRevision = a->textRevision;
    src.selectionRevision = SelectionRevision(dm);
    src.partCount = &DocA11yPageCount;
    src.partName = &DocA11yPageName;
    src.partText = &DocA11yPageText;
    src.partBounds = &DocA11yPageBounds;
    src.partRange = &DocA11yPageRange;
    canvas->BoundsOut(&a->canvasBounds);
    // orig: the file name, AutomationId "Document"
    Str name = path::GetBaseNameTemp(engine->FilePath());
    canvas->AccessibilityId(GStrL("Document"))->AriaLabel(GpuiDup(cx->a, name))->AccessibilityDocument(src);
}

void DocCanvasA11yFree(MainWindow* win) {
    DocA11y* a = FindDocA11y(win);
    if (!a) {
        return;
    }
    VecRemove(gDocA11ys, a);
    delete a;
}
