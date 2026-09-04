/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"
#include "gui/Dpi.h"
#include "gui/Gfx.h"

#include "gui/UIModels.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"

#include "TextToSpeech.h"
#include "WindowTab.h"
#include "MainWindow.h"
#include "SumatraPDF.h"
#include "ReadAloudHighlight.h"
#include "SumatraLog.h"

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
    VecAppend(raw, rb);
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
    return {loc.x, loc.y, loc.dx, loc.dy};
}

static bool IsLineBreakGlyph(const Rect* coords, int idx, int c) {
    return c == '\n' && !coords[idx].x && !coords[idx].dx;
}

static bool CleanRawBytes(Vec<ReadAloudRawByte>& raw, ReadAloudHighlightMap* map, str::Builder& cleanedOut) {
    if (!map) {
        logf("tts: CleanRawBytes: null map\n");
        return false;
    }

    cleanedOut.Reset();
    map->len = 0;

    bool lastWasSpace = false;
    for (int i = 0; i < len(raw);) {
        char c = raw[i].c;
        ReadAloudByteLoc loc = raw[i].loc;

        if (c == '-' && i + 1 < len(raw) && IsReadAloudLineBreak(raw[i + 1].c)) {
            int after = i + 1;
            while (after < len(raw) && IsReadAloudLineBreak(raw[after].c)) {
                after++;
            }
            while (after < len(raw) && IsReadAloudHorizontalSpace(raw[after].c)) {
                after++;
            }

            bool prevIsLower = i > 0 && IsReadAloudLowerAscii(raw[i - 1].c);
            bool nextIsLower = after < len(raw) && IsReadAloudLowerAscii(raw[after].c);
            if (prevIsLower && nextIsLower) {
                i = after;
                lastWasSpace = false;
                continue;
            }
        }

        if (IsReadAloudLineBreak(c)) {
            int lineBreaks = 0;
            while (i < len(raw) && IsReadAloudLineBreak(raw[i].c)) {
                if (raw[i].c == '\n') {
                    lineBreaks++;
                }
                i++;
            }
            while (i < len(raw) && IsReadAloudHorizontalSpace(raw[i].c)) {
                i++;
            }

            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("tts: CleanRawBytes: failed appending line-break space\n");
                    return false;
                }
                lastWasSpace = true;
            }
            if (lineBreaks >= 2) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("tts: CleanRawBytes: failed appending paragraph space\n");
                    return false;
                }
            }
            continue;
        }

        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    logf("tts: CleanRawBytes: failed appending horizontal space\n");
                    return false;
                }
                lastWasSpace = true;
            }
            i++;
            continue;
        }

        if (!ReadAloudHighlightAppend(map, loc) || !cleanedOut.AppendChar(c)) {
            logf("tts: CleanRawBytes: failed appending char 0x%02x\n", (unsigned char)c);
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
                                     str::Builder& cleanedOut) {
    if (!engine || !map) {
        return false;
    }

    PageText pageText = engine->ExtractPageText(pageNo);
    if (len(pageText.text) == 0 || pageText.nCodepoints <= 0) {
        FreePageText(&pageText);
        return false;
    }

    Vec<ReadAloudRawByte> raw;
    int byteIdx = 0;
    for (int i = 0; i < pageText.nCodepoints; i++) {
        ReadAloudByteLoc loc;
        Rect r = pageText.coords[i];
        if (r.x || r.dx) {
            ReadAloudByteLocSetFromRect(loc, pageNo, r);
        }
        int n = 0;
        Utf8CodepointAtByte(pageText.text, byteIdx, &n);
        for (int j = 0; j < n; j++) {
            ReadAloudHighlightAppendRaw(raw, pageText.text.s[byteIdx + j], loc);
        }
        byteIdx += n;
    }
    FreePageText(&pageText);

    return CleanRawBytes(raw, map, cleanedOut);
}

static void ReadAloudAppendPageGlyphs(Vec<ReadAloudRawByte>& raw, EngineBase* engine, int pageNo, int startGlyph,
                                      int endGlyph) {
    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (len(text) == 0) {
        dbgtts("AppendPageGlyphs: page %d has no text (textLen=%d)\n", pageNo, textLen);
        return;
    }

    startGlyph = std::max(startGlyph, 0);
    if (endGlyph < 0 || endGlyph > textLen) {
        endGlyph = textLen;
    }

    ReadAloudByteLoc noLoc;
    int byteIdx = Utf8CodepointToByteIndex(text, startGlyph);
    for (int g = startGlyph; g < endGlyph; g++) {
        int charStart = byteIdx;
        int c = Utf8CodepointNext(text, byteIdx);
        if (IsLineBreakGlyph(coords, g, c)) {
            ReadAloudHighlightAppendRaw(raw, '\r', noLoc);
            ReadAloudHighlightAppendRaw(raw, '\n', noLoc);
            continue;
        }

        ReadAloudByteLoc loc;
        Rect r = coords[g];
        if (r.x || r.dx) {
            ReadAloudByteLocSetFromRect(loc, pageNo, r);
        }

        Str utf8(text.s + charStart, byteIdx - charStart);
        if (len(utf8) == 0) {
            continue;
        }
        for (int i = 0; i < utf8.len; i++) {
            ReadAloudHighlightAppendRaw(raw, utf8.s[i], loc);
        }
    }
}

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, str::Builder& cleanedOut) {
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
        logf("tts: GetViewportStart: null args (dm=%p)\n", dm);
        return false;
    }

    *startPageOut = 0;
    *startGlyphOut = 0;

    int pageCount = dm->PageCount();
    Rect viewArea = dm->GetViewPort();
    viewArea.x = 0;
    viewArea.y = 0;
    dbgtts("GetViewportStart: viewArea=(%d,%d %dx%d)\n", viewArea.x, viewArea.y, viewArea.dx, viewArea.dy);

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

        Rect* coords = nullptr;
        int textLen = 0;
        Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
        if (len(text) == 0) {
            continue;
        }

        int g = 0;
        int byteIdx = 0;
        while (g < textLen) {
            while (g < textLen) {
                int nextByte = byteIdx;
                int c = Utf8CodepointNext(text, nextByte);
                if (!IsLineBreakGlyph(coords, g, c)) {
                    break;
                }
                byteIdx = nextByte;
                g++;
            }
            if (g >= textLen) {
                break;
            }

            int lineStart = g;
            while (g < textLen) {
                int nextByte = byteIdx;
                int c = Utf8CodepointNext(text, nextByte);
                if (IsLineBreakGlyph(coords, g, c)) {
                    break;
                }
                byteIdx = nextByte;
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
                dbgtts("GetViewportStart: found visible line at page %d glyph %d (screenLine=%d,%d %dx%d)\n", pageNo,
                       lineStart, screenLine.x, screenLine.y, screenLine.dx, screenLine.dy);
                *startPageOut = pageNo;
                *startGlyphOut = lineStart;
                return true;
            }
        }
    }

    if (firstVisiblePage == 0) {
        logf("tts: GetViewportStart: no visible pages (pageCount=%d)\n", pageCount);
        return false;
    }

    dbgtts("GetViewportStart: no visible line in viewport, falling back to page %d glyph 0\n", firstVisiblePage);
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

    Rect* coords = nullptr;
    int textLen = 0;
    Str text = engine->GetTextForPage(pageNo, &textLen, &coords);
    if (len(text) == 0) {
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
        logf("tts: GetCursorStart: null args\n");
        return false;
    }

    *startPageOut = 0;
    *startGlyphOut = 0;

    int pageNo = 0;
    int glyph = 0;
    if (!ReadAloudGetGlyphAtCursor(dm, screenPt, &pageNo, &glyph)) {
        logf("tts: GetCursorStart: no text at cursor (%d,%d)\n", screenPt.x, screenPt.y);
        return false;
    }

    dbgtts("GetCursorStart: page %d glyph %d\n", pageNo, glyph);
    *startPageOut = pageNo;
    *startGlyphOut = glyph;
    return true;
}

bool ReadAloudHighlightBuildFromDocument(DisplayModel* dm, int startPage, int startGlyph, ReadAloudHighlightMap* map,
                                         str::Builder& cleanedOut) {
    if (!dm || !map || !dm->ValidPageNo(startPage)) {
        logf("tts: BuildFromDocument: invalid args (dm=%p map=%p startPage=%d)\n", dm, map, startPage);
        return false;
    }

    EngineBase* engine = dm->GetEngine();
    if (!engine) {
        logf("tts: BuildFromDocument: no engine\n");
        return false;
    }

    Vec<ReadAloudRawByte> raw;
    int pageCount = dm->PageCount();
    dbgtts("BuildFromDocument: startPage=%d startGlyph=%d pageCount=%d\n", startPage, startGlyph, pageCount);
    for (int page = startPage; page <= pageCount; page++) {
        int glyph = page == startPage ? startGlyph : 0;
        ReadAloudAppendPageGlyphs(raw, engine, page, glyph, -1);
    }

    if (len(raw) == 0) {
        logf("tts: BuildFromDocument: no raw bytes extracted\n");
        return false;
    }

    if (!CleanRawBytes(raw, map, cleanedOut)) {
        logf("tts: BuildFromDocument: CleanRawBytes failed (raw.size=%zu)\n", len(raw));
        return false;
    }

    dbgtts("BuildFromDocument: ok raw=%zu cleanedLen=%d mapLen=%d\n", len(raw), (int)cleanedOut.len, map->len);
    return true;
}

void ReadAloudHighlightTimerStart(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    SetTimer(win->hwndCanvas, kReadAloudHighlightTimerID, kReadAloudHighlightDelayInMs, nullptr);
}

void ReadAloudHighlightTimerStop(MainWindow* win) {
    if (!win || !win->hwndCanvas) {
        return;
    }
    KillTimer(win->hwndCanvas, kReadAloudHighlightTimerID);
}

static int gReadAloudPaintLogState = 0;

static void ReadAloudPaintLogOnce(int code, Str fmt) {
    if (gReadAloudPaintLogState == code) {
        return;
    }
    gReadAloudPaintLogState = code;
    dbgtts("%s\n", fmt);
}

static int ReadAloudWordEndUtf8(Str text, int pos) {
    if (len(text) == 0 || pos < 0) {
        return pos;
    }
    if (pos >= text.len) {
        return text.len;
    }
    while (pos < text.len && (IsReadAloudHorizontalSpace(text.s[pos]) || IsReadAloudLineBreak(text.s[pos]))) {
        pos++;
    }
    int end = pos;
    while (end < text.len && !IsReadAloudHorizontalSpace(text.s[end]) && !IsReadAloudLineBreak(text.s[end])) {
        end++;
    }
    return end;
}

static bool IsReadAloudSentPunct(int c) {
    return c == '.' || c == '!' || c == '?' || c == 0x3002 || c == 0xFF01 || c == 0xFF1F || c == 0x2026;
}

static bool IsReadAloudCloser(int c) {
    return c == '"' || c == '\'' || c == ')' || c == ']' || c == '}' || c == 0x2019 || c == 0x201D || c == 0x00BB;
}

// Byte after punctuation (and closers / spaces). 0 if this looks like "e.g. the".
static int ReadAloudAfterSentEnd(Str text, int afterPunct) {
    int after = afterPunct;
    while (after < text.len) {
        int t = after;
        int d = Utf8CodepointNext(text, t);
        if (!IsReadAloudCloser(d)) {
            break;
        }
        after = t;
    }
    while (after < text.len) {
        int t = after;
        int d = Utf8CodepointNext(text, t);
        if (d != ' ' && d != '\t') {
            break;
        }
        after = t;
    }
    if (after < text.len) {
        int t = after;
        int d = Utf8CodepointNext(text, t);
        if (d >= 'a' && d <= 'z') {
            return 0;
        }
    }
    return after;
}

static bool ReadAloudSentenceRange(Str text, int pos, int* startOut, int* endOut) {
    if (!startOut || !endOut || len(text) == 0) {
        return false;
    }
    if (pos < 0) {
        return false;
    }
    if (pos > text.len) {
        pos = text.len;
    }

    int start = 0;
    int i = 0;
    while (i < pos) {
        int next = i;
        int c = Utf8CodepointNext(text, next);
        if (next <= i) {
            break;
        }
        if (IsReadAloudSentPunct(c)) {
            int after = ReadAloudAfterSentEnd(text, next);
            if (after > 0 && after <= pos) {
                start = after;
            }
            i = next;
            continue;
        }
        if (c == ' ' && next < text.len && text.s[next] == ' ') {
            int after = next;
            while (after < text.len && text.s[after] == ' ') {
                after++;
            }
            if (after <= pos) {
                start = after;
            }
            i = after;
            continue;
        }
        i = next;
    }

    int end = text.len;
    i = pos;
    while (i < text.len) {
        int next = i;
        int c = Utf8CodepointNext(text, next);
        if (next <= i) {
            break;
        }
        if (IsReadAloudSentPunct(c)) {
            int after = ReadAloudAfterSentEnd(text, next);
            if (after > 0) {
                end = after;
                break;
            }
        }
        if (c == ' ' && next < text.len && text.s[next] == ' ') {
            end = next;
            break;
        }
        i = next;
    }

    if (end <= start) {
        return false;
    }
    *startOut = start;
    *endOut = end;
    return true;
}

struct ReadAloudLineRun {
    int pageNo = 0;
    RectF bbox;
};

static bool ReadAloudLineContinues(const ReadAloudLineRun& run, int pageNo, const RectF& g) {
    if (run.pageNo != pageNo) {
        return false;
    }
    float aBot = run.bbox.y + run.bbox.dy;
    float bBot = g.y + g.dy;
    float h = std::max(run.bbox.dy, g.dy);
    float tol = std::max(h * 0.4f, 2.0f);
    if (std::abs(aBot - bBot) > tol) {
        return false;
    }
    float gap = g.x - (run.bbox.x + run.bbox.dx);
    if (gap < 0) {
        gap = run.bbox.x - (g.x + g.dx);
    }
    float maxGap = std::max(h * 2.5f, 8.0f);
    return gap <= maxGap;
}

static void ReadAloudFlushLine(DisplayModel* dm, Rect canvasRc, const ReadAloudLineRun& run, int minThick, int thickDiv,
                               Vec<Rect>& out) {
    if (run.pageNo <= 0 || run.bbox.IsEmpty()) {
        return;
    }
    PageInfo* pi = dm->GetPageInfo(run.pageNo);
    if (!pi || pi->visibleRatio <= 0.0) {
        return;
    }
    Rect sr = dm->CvtToScreen(run.pageNo, run.bbox);
    sr = sr.Intersect(canvasRc);
    if (sr.IsEmpty() || sr.dx <= 0) {
        return;
    }
    int thick = thickDiv > 0 ? sr.dy / thickDiv : minThick;
    if (thick < minThick) {
        thick = minThick;
    }
    if (thick > sr.dy) {
        thick = sr.dy;
    }
    if (thick < 1) {
        thick = 1;
    }
    Rect u = {sr.x, sr.y + sr.dy - thick, sr.dx, thick};
    if (!u.IsEmpty()) {
        VecAppend(out, u);
    }
}

static void ReadAloudAppendUnderlines(DisplayModel* dm, Rect canvasRc, ReadAloudHighlightMap* map, int startAbs,
                                      int endAbs, int minThick, int thickDiv, Vec<Rect>& out) {
    if (!dm || !map || !map->locs || startAbs < 0 || endAbs > map->len || endAbs <= startAbs) {
        return;
    }
    ReadAloudLineRun run;
    for (int i = startAbs; i < endAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        RectF g = ToRectF(ReadAloudByteLocToRect(loc));
        if (g.IsEmpty()) {
            continue;
        }
        if (run.pageNo == 0) {
            run.pageNo = loc.pageNo;
            run.bbox = g;
            continue;
        }
        if (ReadAloudLineContinues(run, loc.pageNo, g)) {
            run.bbox = run.bbox.Union(g);
            continue;
        }
        ReadAloudFlushLine(dm, canvasRc, run, minThick, thickDiv, out);
        run.pageNo = loc.pageNo;
        run.bbox = g;
    }
    ReadAloudFlushLine(dm, canvasRc, run, minThick, thickDiv, out);
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
    } else if (tab->readAloudResumePos >= 0) {
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
    if (!map || !map->locs || map->len <= 0 || len(tab->readAloudText) == 0) {
        return false;
    }

    int spokenPos = TtsGetSpokenPosUtf8();
    if (spokenPos < 0) {
        return false;
    }

    int chunkLen = tab->readAloudChunkEnd > tab->readAloudChunkStart
                       ? tab->readAloudChunkEnd - tab->readAloudChunkStart
                       : tab->readAloudText.len - tab->readAloudChunkStart;
    Str chunkText = Str(tab->readAloudText.s + tab->readAloudChunkStart, chunkLen);
    int wordStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart + spokenPos;
    int wordEndAbs =
        tab->readAloudHighlightBase + tab->readAloudChunkStart + ReadAloudWordEndUtf8(chunkText, spokenPos);
    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        return false;
    }
    wordEndAbs = std::min(wordEndAbs, map->len);
    if (wordEndAbs <= wordStartAbs) {
        return false;
    }

    *startAbsOut = wordStartAbs;
    *endAbsOut = wordEndAbs;
    return true;
}

static int ReadAloudGlyphDy(ReadAloudHighlightMap* map, int startAbs, int endAbs) {
    if (!map || !map->locs) {
        return 0;
    }
    for (int i = startAbs; i < endAbs && i < map->len; i++) {
        if (ReadAloudByteLocHasRect(map->locs[i]) && map->locs[i].dy > 0) {
            return map->locs[i].dy;
        }
    }
    return 0;
}

// Shrink a punct-based sentence to the visual paragraph around the word so
// heading-heavy PDF text (man pages, etc.) does not underline the whole page.
static void ReadAloudClampVisual(ReadAloudHighlightMap* map, int wordStartAbs, int wordEndAbs, int* startAbs,
                                 int* endAbs) {
    if (!map || !startAbs || !endAbs) {
        return;
    }
    int lineDy = ReadAloudGlyphDy(map, wordStartAbs, wordEndAbs);
    if (lineDy <= 0) {
        lineDy = ReadAloudGlyphDy(map, *startAbs, *endAbs);
    }
    if (lineDy <= 0) {
        return;
    }
    int maxGap = lineDy * 7 / 4;

    int lastY = 0;
    int lastPage = 0;
    bool have = false;
    int s = wordStartAbs;
    for (int i = wordStartAbs; i >= *startAbs; i--) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        if (!have) {
            lastY = loc.y;
            lastPage = loc.pageNo;
            have = true;
            s = i;
            continue;
        }
        if (loc.pageNo != lastPage) {
            break;
        }
        int yGap = lastY - loc.y;
        if (yGap > maxGap) {
            break;
        }
        s = i;
        lastY = loc.y;
        lastPage = loc.pageNo;
    }

    have = false;
    int e = wordEndAbs;
    for (int i = wordStartAbs; i < *endAbs; i++) {
        ReadAloudByteLoc& loc = map->locs[i];
        if (!ReadAloudByteLocHasRect(loc)) {
            continue;
        }
        if (!have) {
            lastY = loc.y;
            lastPage = loc.pageNo;
            have = true;
            e = i + 1;
            continue;
        }
        if (loc.pageNo != lastPage) {
            break;
        }
        int yGap = loc.y - lastY;
        if (yGap > maxGap) {
            break;
        }
        e = i + 1;
        lastY = loc.y;
        lastPage = loc.pageNo;
    }

    if (s >= *startAbs && s < *endAbs) {
        *startAbs = s;
    }
    if (e > *startAbs && e <= *endAbs) {
        *endAbs = e;
    }
}

static bool ReadAloudGetSentenceAbsRange(WindowTab* tab, int wordStartAbs, int wordEndAbs, int* startAbsOut,
                                         int* endAbsOut) {
    if (!tab || !startAbsOut || !endAbsOut) {
        return false;
    }

    *startAbsOut = 0;
    *endAbsOut = 0;

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0 || len(tab->readAloudText) == 0) {
        return false;
    }

    int rel = wordStartAbs - tab->readAloudHighlightBase;
    int sentRelStart = 0;
    int sentRelEnd = 0;
    if (!ReadAloudSentenceRange(tab->readAloudText, rel, &sentRelStart, &sentRelEnd)) {
        return false;
    }

    int chunkStartAbs = tab->readAloudHighlightBase + tab->readAloudChunkStart;
    int chunkLen = tab->readAloudChunkEnd > tab->readAloudChunkStart
                       ? tab->readAloudChunkEnd - tab->readAloudChunkStart
                       : tab->readAloudText.len - tab->readAloudChunkStart;
    int chunkEndAbs = chunkStartAbs + chunkLen;

    int startAbs = tab->readAloudHighlightBase + sentRelStart;
    int endAbs = tab->readAloudHighlightBase + sentRelEnd;
    startAbs = std::max(startAbs, chunkStartAbs);
    endAbs = std::min(endAbs, chunkEndAbs);
    startAbs = std::max(startAbs, 0);
    endAbs = std::min(endAbs, map->len);
    if (endAbs <= startAbs) {
        return false;
    }

    ReadAloudClampVisual(map, wordStartAbs, wordEndAbs, &startAbs, &endAbs);
    if (endAbs <= startAbs) {
        return false;
    }

    *startAbsOut = startAbs;
    *endAbsOut = endAbs;
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
        dbgtts("auto-scroll disabled (user scrolled away from highlight)\n");
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

    int margin = DpiScale(48);
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

    int maxStep = std::max(canvas.dy / 4, DpiScale(120));
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

void PaintReadAloudHighlight(MainWindow* win, Gfx* gfx) {
    if (!TtsIsSpeaking()) {
        gReadAloudPaintLogState = 0;
        return;
    }
    if (!win) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        ReadAloudPaintLogOnce(1, StrL("PaintHighlight: no matching source tab"));
        return;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        ReadAloudPaintLogOnce(2, StrL("PaintHighlight: no highlight map"));
        return;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        ReadAloudPaintLogOnce(3, StrL("PaintHighlight: tab is not a fixed-layout document"));
        return;
    }

    int wordStartAbs = 0;
    int wordEndAbs = 0;
    if (!ReadAloudGetCurrentWordAbsRange(tab, &wordStartAbs, &wordEndAbs)) {
        if (gReadAloudPaintLogState != 4) {
            gReadAloudPaintLogState = 4;
            dbgtts("PaintHighlight: no spoken position (textLen=%d)\n", len(tab->readAloudText));
        }
        return;
    }

    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        ReadAloudPaintLogOnce(5, StrL("PaintHighlight: wordStartAbs out of range"));
        return;
    }
    wordEndAbs = std::min(wordEndAbs, map->len);
    if (wordEndAbs <= wordStartAbs) {
        ReadAloudPaintLogOnce(6, StrL("PaintHighlight: empty word range"));
        return;
    }

    constexpr Color kSentenceCol = MkRgb(0x3b, 0x82, 0xf6);
    constexpr Color kWordCol = MkRgb(0xf5, 0x9e, 0x0b);
    int minThick = DpiScale(2);

    Vec<Rect> sentenceRects;
    int sentStartAbs = 0;
    int sentEndAbs = 0;
    if (ReadAloudGetSentenceAbsRange(tab, wordStartAbs, wordEndAbs, &sentStartAbs, &sentEndAbs)) {
        ReadAloudAppendUnderlines(dm, win->canvasRc, map, sentStartAbs, sentEndAbs, minThick, 10, sentenceRects);
    }
    if (len(sentenceRects) > 0) {
        gfx->FillRects(sentenceRects.els, len(sentenceRects), kSentenceCol);
    }

    Vec<Rect> wordRects;
    ReadAloudAppendUnderlines(dm, win->canvasRc, map, wordStartAbs, wordEndAbs, DpiScale(3), 7, wordRects);
    if (len(wordRects) == 0) {
        ReadAloudPaintLogOnce(7, StrL("PaintHighlight: no screen rects for current word"));
        return;
    }
    gfx->FillRects(wordRects.els, len(wordRects), kWordCol);
}

#if IS_DEBUG
#include "base/UtAssert.h"

void ReadAloudHighlight_UnitTests() {
    int s = 0;
    int e = 0;

    Str two = StrL("Hello world. Next one!");
    int nextAt = str::IndexOf(two, StrL("Next"));
    utassert(nextAt >= 0);
    utassert(ReadAloudSentenceRange(two, 0, &s, &e));
    utassert(s == 0);
    utassert(e == nextAt);
    utassert(ReadAloudSentenceRange(two, 6, &s, &e));
    utassert(s == 0);
    utassert(e == nextAt);
    utassert(ReadAloudSentenceRange(two, nextAt, &s, &e));
    utassert(s == nextAt);
    utassert(e == two.len);

    Str abbr = StrL("See e.g. the cat. Done.");
    int theAt = str::IndexOf(abbr, StrL("the"));
    int doneAt = str::IndexOf(abbr, StrL("Done"));
    utassert(theAt >= 0 && doneAt >= 0);
    utassert(ReadAloudSentenceRange(abbr, theAt, &s, &e));
    utassert(s == 0);
    utassert(e == doneAt);
    utassert(ReadAloudSentenceRange(abbr, doneAt, &s, &e));
    utassert(s == doneAt);
    utassert(e == abbr.len);

    Str quoted = StrL("He said \"Go!\" Then left.");
    int thenAt = str::IndexOf(quoted, StrL("Then"));
    utassert(thenAt >= 0);
    utassert(ReadAloudSentenceRange(quoted, 0, &s, &e));
    utassert(s == 0);
    utassert(e == thenAt);
    utassert(ReadAloudSentenceRange(quoted, thenAt, &s, &e));
    utassert(s == thenAt);
    utassert(e == quoted.len);

    Str para = StrL("First  Second");
    int secondAt = str::IndexOf(para, StrL("Second"));
    utassert(secondAt >= 0);
    utassert(ReadAloudSentenceRange(para, 0, &s, &e));
    utassert(s == 0);
    utassert(e == secondAt - 1);
    utassert(ReadAloudSentenceRange(para, secondAt, &s, &e));
    utassert(s == secondAt);
    utassert(e == para.len);

    Str dotted = StrL("First.  Second.");
    int dottedSecond = str::IndexOf(dotted, StrL("Second"));
    utassert(dottedSecond >= 0);
    utassert(ReadAloudSentenceRange(dotted, 0, &s, &e));
    utassert(s == 0);
    utassert(e == dottedSecond);
    utassert(ReadAloudSentenceRange(dotted, dottedSecond, &s, &e));
    utassert(s == dottedSecond);
    utassert(e == dotted.len);

    // U+3002 ideographic full stop
    Str cjk = StrL("你好。世界");
    utassert(ReadAloudSentenceRange(cjk, 0, &s, &e));
    utassert(s == 0);
    utassert(e > 0);
    utassert(e < cjk.len);
    utassert(ReadAloudSentenceRange(cjk, e, &s, &e));
    utassert(e == cjk.len);
}
#endif
