/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "ReadAloudHighlight.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

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
