/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"

#include "Settings.h"
#include "GlobalPrefs.h"
#include "DocController.h"
#include "EngineBase.h"
#include "DisplayModel.h"
#include "TextSelection.h"
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
                    return false;
                }
                lastWasSpace = true;
            }
            if (lineBreaks >= 2) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    return false;
                }
            }
            continue;
        }

        if (IsReadAloudHorizontalSpace(c)) {
            if (!lastWasSpace && map->len > 0) {
                ReadAloudByteLoc spaceLoc;
                if (!ReadAloudHighlightAppend(map, spaceLoc) || !cleanedOut.AppendChar(' ')) {
                    return false;
                }
                lastWasSpace = true;
            }
            i++;
            continue;
        }

        if (!ReadAloudHighlightAppend(map, loc) || !cleanedOut.AppendChar(c)) {
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
        ReadAloudHighlightAppendRaw(raw, pageText.text[i], loc);
    }
    FreePageTextUtf8(&pageText);

    return CleanRawBytes(raw, map, cleanedOut);
}

bool ReadAloudHighlightBuildFromTextSelection(TextSelection* ts, ReadAloudHighlightMap* map, StrBuilder& cleanedOut) {
    if (!ts || !ts->engine || !map) {
        return false;
    }

    int fromPage = 0, fromGlyph = 0, toPage = 0, toGlyph = 0;
    ts->GetGlyphRange(&fromPage, &fromGlyph, &toPage, &toGlyph);

    Vec<ReadAloudRawByte> raw;
    ReadAloudByteLoc noLoc;

    for (int page = fromPage; page <= toPage; page++) {
        int textLen = 0;
        Rect* coords = nullptr;
        const WCHAR* text = ts->engine->GetTextForPage(page, &textLen, &coords);
        int glyph = page == fromPage ? fromGlyph : 0;
        int endGlyph = page == toPage ? toGlyph : textLen;

        for (int g = glyph; g < endGlyph; g++) {
            if (IsLineBreakGlyph(text, coords, g, textLen)) {
                ReadAloudHighlightAppendRaw(raw, '\r', noLoc);
                ReadAloudHighlightAppendRaw(raw, '\n', noLoc);
                continue;
            }

            ReadAloudByteLoc loc;
            Rect r = coords[g];
            if (r.x || r.dx) {
                ReadAloudByteLocSetFromRect(loc, page, r);
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

    return CleanRawBytes(raw, map, cleanedOut);
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

void PaintReadAloudHighlight(MainWindow* win, HDC hdc) {
    if (!TtsIsSpeaking() || !win) {
        return;
    }

    WindowTab* tab = GetReadAloudSourceTab();
    if (!tab || tab->win != win) {
        return;
    }

    ReadAloudHighlightMap* map = tab->readAloudHighlight;
    if (!map || !map->locs || map->len <= 0) {
        return;
    }

    DisplayModel* dm = tab->AsFixed();
    if (!dm) {
        return;
    }

    int spokenPos = TtsGetSpokenPosUtf8();
    if (spokenPos < 0 || str::IsEmpty(tab->readAloudText)) {
        return;
    }

    int wordStartAbs = tab->readAloudHighlightBase + spokenPos;
    int wordEndAbs = tab->readAloudHighlightBase + ReadAloudWordEndUtf8(tab->readAloudText, spokenPos);
    if (wordStartAbs < 0 || wordStartAbs >= map->len) {
        return;
    }
    if (wordEndAbs > map->len) {
        wordEndAbs = map->len;
    }
    if (wordEndAbs <= wordStartAbs) {
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
        return;
    }

    ParsedColor* parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.selectionColor);
    u8 alpha = GetAlpha(parsedCol->col);
    if (alpha == 0) {
        alpha = kSelectionDefaultAlpha;
    }
    PaintTransparentRectangles(hdc, win->canvasRc, screenRects, parsedCol->col, alpha);
}