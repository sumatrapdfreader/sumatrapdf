/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"

#include "utils/Log.h"
#include "TextToSpeech.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "Selection.h"
#include "SumatraPDF.h"
#include "ReadAloudHighlight.h"

struct ReadAloudRawByte {
    char c = 0;
    ReadAloudByteLoc loc{};
};

static bool IsReadAloudLowerAscii(char c) {
    return c >= 'a' && c <= 'z';
}

static bool IsReadAloudLineBreak(char c) {
    return c == '\r' || c == '\n';
}

static bool IsReadAloudHorizontalSpace(char c) {
    return c == ' ' || c == '\t';
}

static bool ReadAloudHighlightGrow(ReadAloudHighlightMap* map) {
    if (map->len + 1 < map->cap) {
        return true;
    }
    int newCap = map->cap == 0 ? 256 : map->cap * 2;
    ReadAloudByteLoc* newLocs = (ReadAloudByteLoc*)realloc(map->locs, sizeof(ReadAloudByteLoc) * (size_t)newCap);
    if (!newLocs) {
        return false;
    }
    map->locs = newLocs;
    map->cap = newCap;
    return true;
}

static bool ReadAloudHighlightAppend(ReadAloudHighlightMap* map, const ReadAloudByteLoc& loc) {
    if (!ReadAloudHighlightGrow(map)) {
        return false;
    }
    map->locs[map->len] = loc;
    map->len++;
    return true;
}

static bool ReadAloudHighlightAppendRaw(Vec<ReadAloudRawByte>& raw, char c, const ReadAloudByteLoc& loc) {
    ReadAloudRawByte rb;
    rb.c = c;
    rb.loc = loc;
    raw.Append(rb);
    return true;
}

static void ReadAloudByteLocSetFromRect(ReadAloudByteLoc& loc, int pageNo, const Rect& r) {
    loc.pageNo = pageNo;
    loc.x = r.x;
    loc.y = r.y;
    loc.dx = r.dx;
    loc.dy = r.dy;
}

static bool ReadAloudByteLocHasRect(const ReadAloudByteLoc& loc) {
    return loc.pageNo > 0 && (loc.x || loc.dx);
}

static Rect ReadAloudByteLocToRect(const ReadAloudByteLoc& loc) {
    return Rect(loc.x, loc.y, loc.dx, loc.dy);
}

static bool IsLineBreakGlyph(const WCHAR* text, const Rect* coords, int idx, int textLen) {
    return idx >= 0 && idx < textLen && text[idx] == '\n' && !coords[idx].x && !coords[idx].dx;
}

static bool CleanRawBytes(Vec<ReadAloudRawByte>& raw, ReadAloudHighlightMap* map, StrBuilder& cleanedOut) {
    if (!map) {
        logf("ReadAloud: CleanRawBytes: null map\n");
        return false;
    }

    cleanedOut.Reset();
    map->len = 0;

    bool lastWasSpace = false;
    for (size_t i = 0; i < raw.size();) {
        char c = raw[i].c;
        ReadAloudByteLoc loc = raw[i].loc;

        if (c == '-' && i + 1 < raw.size() && IsReadAloudLineBreak(raw[i + 1].c)) {
            size_t after = i + 1;
            while (after < raw.size() && IsReadAloudLineBreak(raw[after].c)) {
                after++;
            }
            while (after < raw.size() && IsReadAloudHorizontalSpace(raw[after].c)) {
                after++;
            }

            bool prevIsLower = i > 0 && IsReadAloudLowerAscii(raw[i - 1].c);
            bool nextIsLower = after < raw.size() && IsReadAloudLowerAscii(raw[after].c);
            if (prevIsLower && nextIsLower) {
                i = after;
                lastWasSpace = false;
                continue;
            }
        }

        if (IsReadAloudLineBreak(c)) {
            int lineBreaks = 0;
            while (i < raw.size() && IsReadAloudLineBreak(raw[i].c)) {
                if (raw[i].c == '\n') {
                    lineBreaks++;
                }
                i++;
            }
            while (i < raw.size() && IsReadAloudHorizontalSpace(raw[i].c)) {
                i++;
            }

            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("ReadAloud: CleanRawBytes: failed appending line-break space\n");
                    return false;
                }
                lastWasSpace = true;
            }
            if (lineBreaks >= 2) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("ReadAloud: CleanRawBytes: failed appending paragraph space\n");
                    return false;
                }
            }
            continue;
        }

        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("ReadAloud: CleanRawBytes: failed appending horizontal space\n");
                    return false;
                }
                lastWasSpace = true;
            }
            i++;
            continue;
        }

        if (!ReadAloudHighlightAppend(map, loc) || !cleanedOut.AppendChar(c)) {
            logf("ReadAloud: CleanRawBytes: failed appending char 0x%02x\n", (unsigned char)c);
            return false;
        }
        lastWasSpace = false;
        i++;
    }

    return true;
}

void ReadAloudHighlightFree(ReadAloudHighlightMap* map) {
    if (!map) {
        return;
    }
    free(map->locs);
    map->locs = nullptr;
    map->len = 0;
    map->cap = 0;
}

bool ReadAloudHighlightBuildFromPage(EngineBase* engine, int pageNo, ReadAloudHighlightMap* map,
                                     StrBuilder& cleanedOut) {
    if (!engine || !map) {
        return false;
    }

    PageTextUtf8 pageText = engine->ExtractPageTextUtf8(pageNo);
    if (!pageText.text || pageText.len <= 0) {
        FreePageTextUtf8(&pageText);
        return false;
    }

    Vec<ReadAloudRawByte> raw;
    for (int i = 0; i < pageText.len; i++) {
        ReadAloudByteLoc loc;
        Rect r = pageText.coords[i];
        if (r.x || r.dx) {
            ReadAloudByteLocSetFromRect(loc, pageNo, r);
        }
        ReadAloudHighlightAppendRaw(raw, pageText.text.s[i], loc);
    }
    FreePageTextUtf8(&pageText);

    return CleanRawBytes(raw, map, cleanedOut);
}

static void ReadAloudAppendPageGlyphs(Vec<ReadAloudRawByte>& raw, EngineBase* engine, int pageNo, int startGlyph,
                                      int endGlyph) {
    int textLen = 0;
    Rect* coords = nullptr;
    const WCHAR* text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (!text || textLen <= 0) {
        logf("ReadAloud: AppendPageGlyphs: page %d has no text (textLen=%d)\n", pageNo, textLen);
        return;
    }

    if (startGlyph < 0) {
        startGlyph = 0;
    }
    if (endGlyph < 0 || endGlyph > textLen) {
        endGlyph = textLen;
    }

    ReadAloudByteLoc noLoc;
    for (int g = startGlyph; g < endGlyph; g++) {
        if (IsLineBreakGlyph(text, coords, g, textLen)) {
            ReadAloudHighlightAppendRaw(raw, '\r', noLoc);
            ReadAloudHighlightAppendRaw(raw, '\n', noLoc);
            continue;
        }

        ReadAloudByteLoc loc;
        Rect r = coords[g];
        if (r.x || r.dx) {
            ReadAloudByteLocSetFromRect(loc, pageNo, r);
        }

        WCHAR wc[2] = {text[g], 0};
        TempStr utf8 = ToUtf8Temp(wc);
        if (str::IsEmpty(utf8)) {
            continue;
        }
        for (const char* p = utf8; *p; p++) {
            ReadAloudHighlightAppendRaw(raw, *p, loc);
        }
    }
}

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, StrBuilder& cleanedOut) {
    if (!ts || !ts->engine || !map) {
        return false;
    }

    int fromPage = 0, fromGlyph = 0, toPage = 0, toGlyph = 0;
    ts->GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    Vec<ReadAloudRawByte> raw;
    for (int page = fromPage; page <= toPage; page++) {
        int glyph = page == fromPage ? fromGlyph : 0;
        int endGlyph = page == toPage ? toGlyph : -1;
        ReadAloudAppendPageGlyphs(raw, ts->engine, page, glyph, endGlyph);
    }

    return CleanRawBytes(raw, map, cleanedOut);
}

bool ReadAloudGetViewportStart(DisplayModel* dm, int* startPageOut, int* startGlyphOut) {
    if (!dm || !startPageOut || !startGlyphOut) {
        logf("ReadAloud: GetViewportStart: null args (dm=%p)\n", dm);
        return false;
    }

    *startPageOut = 0;
    *startGlyphOut = 0;

    int pageCount = dm->PageCount();
    Rect viewArea = dm->GetViewPort();
    viewArea.x = 0;
    viewArea.y = 0;
    logf("ReadAloud: GetViewportStart: viewArea=(%d,%d %dx%d)\n", viewArea.x, viewArea.y, viewArea.dx, viewArea.dy);

    int firstVisiblePage = 0;
    EngineBase* engine = dm->GetEngine();
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        PageInfo* pageInfo = dm->GetPageInfo(pageNo);
        if (!pageInfo || pageInfo->visibleRatio <= 0.0) {
            continue;
        }
        if (firstVisiblePage == 0) {
            firstVisiblePage = pageNo;
        }

        int textLen = 0;
        Rect* coords = nullptr;
        const WCHAR* text = engine->GetTextForPage(pageNo, &textLen, &coords);
        if (!text || textLen <= 0) {
            continue;
        }

        int g = 0;
        while (g < textLen) {
            while (g < textLen && IsLineBreakGlyph(text, coords, g, textLen)) {
                g++;
            }
            if (g >= textLen) {
                break;
            }

            int lineStart = g;
            while (g < textLen && !IsLineBreakGlyph(text, coords, g, textLen)) {
                g++;
            }

            Rect lineBbox;
            for (int i = lineStart; i < g; i++) {
                Rect r = coords[i];
                if (r.x || r.dx) {
                    lineBbox = lineBbox.IsEmpty() ? r : lineBbox.Union(r);
                }
            }
            if (lineBbox.IsEmpty()) {
                continue;
            }

            Rect screenLine = dm->CvtToScreen(pageNo, ToRectF(lineBbox));
            if (!screenLine.Intersect(viewArea).IsEmpty()) {
                logf("ReadAloud: GetViewportStart: found visible line at page %d glyph %d (screenLine=%d,%d %dx%d)\n",
                     pageNo, lineStart, screenLine.x, screenLine.y, screenLine.dx, screenLine.dy);
                *startPageOut = pageNo;
                *startGlyphOut = lineStart;
                return true;
            }
        }
    }

    if (firstVisiblePage == 0) {
        logf("ReadAloud: GetViewportStart: no visible pages (pageCount=%d)\n", pageCount);
        return false;
    }

    logf("ReadAloud: GetViewportStart: no visible line in viewport, falling back to page %d glyph 0\n",
         firstVisiblePage);
    *startPageOut = firstVisiblePage;
    *startGlyphOut = 0;
    return true;
}

static bool ReadAloudGetGlyphAtCursor(DisplayModel* dm, Point screenPt, int* pageOut, int* glyphOut) {
    if (!dm || !pageOut || !glyphOut || !dm->textSelection) {
        return false;
    }
    if (!dm->IsOverText(screenPt)) {
        return false;
    }

    int pageNo = dm->GetPageNoByPoint(screenPt);
    if (!dm->ValidPageNo(pageNo)) {
        return false;
    }

    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        return false;
    }

    PointF pt = dm->CvtFromScreen(screenPt, pageNo);

    int textLen = 0;
    Rect* coords = nullptr;
    engine->GetTextForPage(pageNo, &textLen, &coords);
    if (textLen <= 0) {
        return false;
    }

    // find the glyph under the cursor without mutating the live selection:
    // StartAt() would overwrite startGlyph and corrupt an existing selection
    // when this is called from the context menu (issue #5718)
    // Same adjustment as TextSelection::IsOverGlyph: FindClosestGlyph can return
    // the index after the glyph under the cursor when clicking its right half.
    int glyph = dm->textSelection->FindClosestGlyphAt(pageNo, pt.x, pt.y);
    Point pti = ToPoint(pt);
    if (glyph == textLen || (glyph >= 0 && glyph < textLen && !coords[glyph].Contains(pti))) {
        glyph--;
    }
    if (glyph < 0 || glyph >= textLen) {
        return false;
    }

    *pageOut = pageNo;
    *glyphOut = glyph;
    return true;
}

bool ReadAloudCanReadFromCursor(DisplayModel* dm, Point screenPt) {
    int pageNo = 0;
    int glyph = 0;
    return ReadAloudGetGlyphAtCursor(dm, screenPt, &pageNo, &glyph);
}

bool ReadAloudGetCursorStart(DisplayModel* dm, Point screenPt, int* startPageOut, int* startGlyphOut) {
    if (!startPageOut || !startGlyphOut) {
        logf("ReadAloud: GetCursorStart: null args\n");
        return false;
    }

    *startPageOut = 0;
    *startGlyphOut = 0;

    int pageNo = 0;
    int glyph = 0;
    if (!ReadAloudGetGlyphAtCursor(dm, screenPt, &pageNo, &glyph)) {
        logf("ReadAloud: GetCursorStart: no text at cursor (%d,%d)\n", screenPt.x, screenPt.y);
        return false;
    }

    logf("ReadAloud: GetCursorStart: page %d glyph %d\n", pageNo, glyph);
    *startPageOut = pageNo;
    *startGlyphOut = glyph;
    return true;
}

bool ReadAloudHighlightBuildFromDocument(DisplayModel* dm, int startPage, int startGlyph, ReadAloudHighlightMap* map,
                                         StrBuilder& cleanedOut) {
    if (!dm || !map || !dm->ValidPageNo(startPage)) {
        logf("ReadAloud: BuildFromDocument: invalid args (dm=%p map=%p startPage=%d)\n", dm, map, startPage);
        return false;
    }

    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        logf("ReadAloud: BuildFromDocument: no engine\n");
        return false;
    }

    Vec<ReadAloudRawByte> raw;
    int pageCount = dm->PageCount();
    logf("ReadAloud: BuildFromDocument: startPage=%d startGlyph=%d pageCount=%d\n", startPage, startGlyph, pageCount);
    for (int page = startPage; page <= pageCount; page++) {
        int glyph = page == startPage ? startGlyph : 0;
        ReadAloudAppendPageGlyphs(raw, engine, page, glyph, -1);
    }

    if (raw.size() == 0) {
        logf("ReadAloud: BuildFromDocument: no raw bytes extracted\n");
        return false;
    }

    if (!CleanRawBytes(raw, map, cleanedOut)) {
        logf("ReadAloud: BuildFromDocument: CleanRawBytes failed (raw.size=%zu)\n", raw.size());
        return false;
    }

    logf("ReadAloud: BuildFromDocument: ok raw=%zu cleanedLen=%d mapLen=%d\n", raw.size(), (int)cleanedOut.len,
         map->len);
    return true;
}

void ReadAloudHighlightTimerStart(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    SetTimer(win->hwndCanvas, READ_ALOUD_HIGHLIGHT_TIMER_ID, READ_ALOUD_HIGHLIGHT_DELAY_IN_MS, nullptr);
}

void ReadAloudHighlightTimerStop(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    KillTimer(win->hwndCanvas, READ_ALOUD_HIGHLIGHT_TIMER_ID);
}

static int gReadAloudPaintLogState = 0;

static void ReadAloudPaintLogOnce(int code, const char* fmt, ...) {
    if (gReadAloudPaintLogState == code) {
        return;
    }
    gReadAloudPaintLogState = code;
    logf(fmt);
}

static int ReadAloudWordEndUtf8(const char* text, int pos) {
    if (!text || pos < 0) {
        return pos;
    }
    int len = str::Leni(text);
    if (pos >= len) {
        return len;
    }
    while (pos < len && (IsReadAloudHorizontalSpace(text[pos]) || IsReadAloudLineBreak(text[pos]))) {
        pos++;
    }
    int end = pos;
    while (end < len && !IsReadAloudHorizontalSpace(text[end]) && !IsReadAloudLineBreak(text[end])) {
        end++;
    }
    return end;
}

bool ReadAloudGetProgressPage(WindowTab* tab, int* pageOut, int* pageCountOut) {
    if (!tab || !pageOut || !pageCountOut) {
        return false;
    }

    *pageOut = 0;
    *pageCountOut = 0;

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }
    *pageCountOut = dm->PageCount();

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        return false;
    }

    int absPos = -1;
    WindowTab* sourceTab = GetReadAloudSourceTab();
    if (sourceTab == tab && TtsIsSpeaking()) {
        int spokenPos = TtsGetSpokenPosUtf8();
        if (spokenPos >= 0) {
            absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
        }
    } else if (tab->readAloudResumePos > 0) {
        absPos = tab->readAloudResumePos;
    } else if (tab->readAloudChunkEnd > 0) {
        absPos = tab->readAloudHighlightBase + tab->readAloudChunkStart;
    }

    if (absPos < 0 || absPos >= map->len) {
        return false;
    }

    int pageNo = map->locs[absPos].pageNo;
    if (pageNo <= 0) {
        return false;
    }

    *pageOut = pageNo;
    return true;
}

static bool ReadAloudGetCurrentWordAbsRange(WindowTab* tab, int* startAbsOut, int* endAbsOut) {
    if (!tab || !startAbsOut || !endAbsOut) {
        return false;
    }

    *startAbsOut = 0;
    *endAbsOut = 0;

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0 || str::IsEmpty(tab->readAloudText)) {
        return false;
    }

    int spokenPos = TtsGetSpokenPosUtf8();
    if (spokenPos < 0) {
        return false;
    }

    const char* chunkText = tab->readAloudText + tab->readAloudChunkStart;
    int wordStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
    int wordEndAbs =
        tab->readAloudHighlightBase + tab->readAloudChunkStart + ReadAloudWordEndUtf8(chunkText, spokenPos);
    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        return false;
    }
    if (wordEndAbs > map->len) {
        wordEndAbs = map->len;
    }
    if (wordEndAbs <= wordStartAbs) {
        return false;
    }

    *startAbsOut = wordStartAbs;
    *endAbsOut = wordEndAbs;
    return true;
}

static bool ReadAloudGetCurrentWordScreenRect(MainWindow* win, Rect* rectOut) {
    if (!rectOut || !win) {
        return false;
    }

    *rectOut = Rect();

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        return false;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return false;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        return false;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    Rect unionRect;
    bool hasRect = false;
    for (int i = wordStartAbs; i < wordEndAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        Rect sr = dm->CvtToScreen(loc.pageNo, ToRectF(ReadAloudByteLocToRect(loc)));
        if (!hasRect) {
            unionRect = sr;
            hasRect = true;
        } else {
            unionRect = unionRect.Union(sr);
        }
    }

    if (!hasRect) {
        return false;
    }

    *rectOut = unionRect;
    return true;
}

static bool ReadAloudIsWordRectVisibleInViewport(MainWindow* win, const Rect& wordRect) {
    if (!win) {
        return false;
    }
    return !wordRect.Intersect(win->canvasRc).IsEmpty();
}

static bool ReadAloudIsWordRectFullyVisibleInViewport(MainWindow* win, const Rect& wordRect, int margin) {
    if (!win) {
        return false;
    }
    Rect canvas = win->canvasRc;
    if (wordRect.x < margin || wordRect.y < margin) {
        return false;
    }
    if (wordRect.x + wordRect.dx > canvas.dx - margin) {
        return false;
    }
    if (wordRect.y + wordRect.dy > canvas.dy - margin) {
        return false;
    }
    return true;
}

void ReadAloudOnUserViewChanged(MainWindow* win) {
    if (!win || win->readAloudScrollFromCode || !TtsIsSpeaking()) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win || !tab->readAloudAutoScroll) {
        return;
    }

    Rect wordRect;
    if (!ReadAloudGetCurrentWordScreenRect(win, &wordRect) || !ReadAloudIsWordRectVisibleInViewport(win, wordRect)) {
        tab->readAloudAutoScroll = false;
        logf("ReadAloud: auto-scroll disabled (user scrolled away from highlight)\n");
    }
}

void ReadAloudUpdateAutoScroll(MainWindow* win) {
    if (!win || !TtsIsSpeaking()) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win || !tab->readAloudAutoScroll) {
        return;
    }

    Rect wordRect;
    if (!ReadAloudGetCurrentWordScreenRect(win, &wordRect)) {
        return;
    }

    int margin = DpiScale(win->hwndCanvas, 48);
    if (ReadAloudIsWordRectFullyVisibleInViewport(win, wordRect, margin)) {
        return;
    }

    Rect canvas = win->canvasRc;

    int dx = 0;
    int dy = 0;
    if (wordRect.y < margin) {
        dy = wordRect.y - margin;
    } else if (wordRect.y + wordRect.dy > canvas.dy - margin) {
        dy = wordRect.y + wordRect.dy - (canvas.dy - margin);
    }
    if (wordRect.x < margin) {
        dx = wordRect.x - margin;
    } else if (wordRect.x + wordRect.dx > canvas.dx - margin) {
        dx = wordRect.x + wordRect.dx - (canvas.dx - margin);
    }

    if (dx == 0 && dy == 0) {
        return;
    }

    int maxStep = std::max(canvas.dy / 4, DpiScale(win->hwndCanvas, 120));
    if (dx > maxStep) {
        dx = maxStep;
    } else if (dx < -maxStep) {
        dx = -maxStep;
    }
    if (dy > maxStep) {
        dy = maxStep;
    } else if (dy < -maxStep) {
        dy = -maxStep;
    }

    win->readAloudScrollFromCode = true;
    win->MoveDocBy(dx, dy);
    win->readAloudScrollFromCode = false;
}

void PaintReadAloudHighlight(MainWindow* win, HDC hdc) {
    if (!TtsIsSpeaking()) {
        gReadAloudPaintLogState = 0;
        return;
    }
    if (!win) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        ReadAloudPaintLogOnce(1, "ReadAloud: PaintHighlight: no matching source tab");
        return;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        ReadAloudPaintLogOnce(2, "ReadAloud: PaintHighlight: no highlight map");
        return;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        ReadAloudPaintLogOnce(3, "ReadAloud: PaintHighlight: tab is not a fixed-layout document");
        return;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        if (gReadAloudPaintLogState != 4) {
            gReadAloudPaintLogState = 4;
            logf("ReadAloud: PaintHighlight: no spoken position (textLen=%d)\n", str::Leni(tab->readAloudText));
        }
        return;
    }

    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        ReadAloudPaintLogOnce(5, "ReadAloud: PaintHighlight: wordStartAbs out of range");
        return;
    }
    if (wordEndAbs > map->len) {
        wordEndAbs = map->len;
    }
    if (wordEndAbs <= wordStartAbs) {
        ReadAloudPaintLogOnce(6, "ReadAloud: PaintHighlight: empty word range");
        return;
    }

    int pageCount = dm->GetEngine()->PageCount();
    Vec<RectF> pageUnions;
    pageUnions.SetSize(pageCount + 1);

    for (int i = wordStartAbs; i < wordEndAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        PageInfo* pi = dm->GetPageInfo(loc.pageNo);
        if (!pi || pi->visibleRatio <= 0.0) {
            continue;
        }
        RectF& u = pageUnions[loc.pageNo];
        RectF rf = ToRectF(ReadAloudByteLocToRect(loc));
        u = u.IsEmpty() ? rf : u.Union(rf);
    }

    Vec<Rect> screenRects;
    for (int pageNo = 1; pageNo <= pageCount; pageNo++) {
        RectF u = pageUnions[pageNo];
        if (u.IsEmpty()) {
            continue;
        }
        Rect sr = dm->CvtToScreen(pageNo, u);
        sr = sr.Intersect(win->canvasRc);
        if (!sr.IsEmpty()) {
            screenRects.Append(sr);
        }
    }

    if (screenRects.size() == 0) {
        ReadAloudPaintLogOnce(7, "ReadAloud: PaintHighlight: no screen rects for current word");
        return;
    }

    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.selectionColor);
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }
    PaintTransparentRectangles(hdc, win->canvasRc, screenRects, parsedCol->col, alpha);
}