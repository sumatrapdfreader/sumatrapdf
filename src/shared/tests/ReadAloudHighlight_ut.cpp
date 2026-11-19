/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "ChapterTable.h"
#include "ReadAloud.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

// glyphs n, each dx by dy, stepping by (stepX, stepY) from (x, y)
static void AddGlyphs(ReadAloudByteLoc* locs, int& n, int count, int x, int y, int stepX, int stepY) {
    constexpr int kGlyphDx = 10;
    constexpr int kGlyphDy = 10;
    for (int i = 0; i < count; i++) {
        locs[n++] = ReadAloudByteLoc{LocFromPageNo(1), x + (i * stepX), y + (i * stepY), kGlyphDx, kGlyphDy};
    }
}

// A line of horizontal text is one run; a column of vertical text (issue #6530)
// is one vertical run, and the next column starts another.
static void ReadAloudRunsTests() {
    ReadAloudByteLoc locs[32];
    ReadAloudHighlightMap map;
    map.locs = locs;

    int n = 0;
    AddGlyphs(locs, n, 6, 100, 100, 11, 0);
    map.len = n;
    Vec<ReadAloudRun> runs;
    ReadAloudCollectRuns(&map, 0, n, runs);
    utassert(len(runs) == 1);
    utassert(!runs[0].vertical);

    // two columns, read top to bottom, right column first
    n = 0;
    AddGlyphs(locs, n, 5, 200, 100, 0, 12);
    AddGlyphs(locs, n, 4, 180, 100, 0, 12);
    map.len = n;
    VecReset(runs);
    ReadAloudCollectRuns(&map, 0, n, runs);
    utassert(len(runs) == 2);
    utassert(runs[0].vertical && runs[1].vertical);
    utassert(runs[0].bbox.x == 200.f && runs[0].bbox.dy == 58.f);
    utassert(runs[1].bbox.x == 180.f && runs[1].bbox.dy == 46.f);

    map.locs = nullptr;
}

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

    ReadAloudRunsTests();
}
