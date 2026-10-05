/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Annotation.h"
#include "AnnotSearch.h"

// must be last due to assert() over-write
#include "base/tests/UtAssert.h"

static int CondCount(const AnnotMatchOpts& opts) {
    return ListLen(opts.conds);
}

static AnnotMatchCond* CondAt(const AnnotMatchOpts& opts, int idx) {
    AnnotMatchCond* c = opts.conds;
    while (c && idx > 0) {
        c = c->next;
        idx--;
    }
    return c;
}

static void TestParseFails(Str filter) {
    AnnotMatchOpts opts;
    utassert(!ParseAnnotSearch(filter, opts));
}

bool AnnotSearch_UnitTests() {
    using Type = AnnotMatchCond::Type;
    {
        // plain words are content conditions
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL("todo later"), opts));
        utassert(CondCount(opts) == 2);
        utassert(CondAt(opts, 0)->tp == Type::ContentMatches);
        utassert(str::Eq(CondAt(opts, 0)->s, StrL("todo")));
        utassert(str::Eq(CondAt(opts, 1)->s, StrL("later")));
    }
    {
        // an empty filter matches everything
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(""), opts));
        utassert(CondCount(opts) == 0);
        utassert(AnnotMatchesFields(StrL("kjk"), StrL(""), AnnotationType::Text, opts));
    }
    {
        // the three author spellings parse the same
        for (Str f : {StrL(":a=kjk"), StrL(":a = kjk"), StrL(":a== kjk")}) {
            AnnotMatchOpts opts;
            utassert(ParseAnnotSearch(f, opts));
            utassert(CondCount(opts) == 1);
            utassert(CondAt(opts, 0)->tp == Type::AuthorEqual);
            utassert(str::Eq(CondAt(opts, 0)->s, StrL("kjk")));
        }
    }
    {
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":a!= kjk"), opts));
        utassert(CondAt(opts, 0)->tp == Type::AuthorNotEqual);
        utassert(AnnotMatchesFields(StrL("bob"), StrL(""), AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields(StrL("kjk"), StrL(""), AnnotationType::Text, opts));
        // author match is case-insensitive
        utassert(!AnnotMatchesFields(StrL("KJK"), StrL(""), AnnotationType::Text, opts));
    }
    {
        // ":t" takes an untranslated type name, spaces and case ignored
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":t== freetext"), opts));
        utassert(CondAt(opts, 0)->tp == Type::AnnotTypeEqual);
        utassert(CondAt(opts, 0)->annotType == AnnotationType::FreeText);
        utassert(AnnotMatchesFields({}, {}, AnnotationType::FreeText, opts));
        utassert(!AnnotMatchesFields({}, {}, AnnotationType::Text, opts));
    }
    {
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":t !=line"), opts));
        utassert(CondAt(opts, 0)->tp == Type::AnnotTypeNotEqual);
        utassert(!AnnotMatchesFields({}, {}, AnnotationType::Line, opts));
        utassert(AnnotMatchesFields({}, {}, AnnotationType::Ink, opts));
    }
    {
        // several "==" on one field are alternatives
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":t=text :t=freetext"), opts));
        utassert(CondCount(opts) == 2);
        utassert(AnnotMatchesFields({}, {}, AnnotationType::Text, opts));
        utassert(AnnotMatchesFields({}, {}, AnnotationType::FreeText, opts));
        utassert(!AnnotMatchesFields({}, {}, AnnotationType::Ink, opts));
    }
    {
        // several "!=" all have to hold
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":a!=kjk :a!=bob"), opts));
        utassert(!AnnotMatchesFields(StrL("kjk"), {}, AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields(StrL("bob"), {}, AnnotationType::Text, opts));
        utassert(AnnotMatchesFields(StrL("ann"), {}, AnnotationType::Text, opts));
    }
    {
        // ":c+" keeps only annotations that have contents
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":c+"), opts));
        utassert(CondCount(opts) == 1);
        utassert(CondAt(opts, 0)->tp == Type::HasContent);
        utassert(AnnotMatchesFields({}, StrL("a note"), AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields({}, StrL(""), AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields({}, {}, AnnotationType::Text, opts));
    }
    {
        // ":c-" keeps only the ones with none
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":c-"), opts));
        utassert(CondAt(opts, 0)->tp == Type::NoContent);
        utassert(!AnnotMatchesFields({}, StrL("a note"), AnnotationType::Text, opts));
        utassert(AnnotMatchesFields({}, StrL(""), AnnotationType::Text, opts));
        utassert(AnnotMatchesFields({}, {}, AnnotationType::Text, opts));
    }
    {
        // asking for both at once matches nothing, and must not be a parse error
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":c+ :c-"), opts));
        utassert(!AnnotMatchesFields({}, StrL("x"), AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields({}, {}, AnnotationType::Text, opts));
    }
    {
        // conditions and words combine; contents match on substring, any case
        AnnotMatchOpts opts;
        utassert(ParseAnnotSearch(StrL(":a=kjk :t!=line :c+ TODO"), opts));
        utassert(CondCount(opts) == 4);
        utassert(AnnotMatchesFields(StrL("kjk"), StrL("a todo item"), AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields(StrL("bob"), StrL("a todo item"), AnnotationType::Text, opts));
        utassert(!AnnotMatchesFields(StrL("kjk"), StrL("a todo item"), AnnotationType::Line, opts));
        utassert(!AnnotMatchesFields(StrL("kjk"), StrL("something else"), AnnotationType::Text, opts));
        StrVec words;
        AnnotSearchContentWords(opts, words);
        utassert(len(words) == 1);
        utassert(str::Eq(words[0], StrL("TODO")));
    }
    // malformed input is rejected rather than silently matching nothing
    TestParseFails(StrL(":a"));
    TestParseFails(StrL(":a="));
    TestParseFails(StrL(":a kjk"));
    TestParseFails(StrL(":t=nosuchtype"));
    TestParseFails(StrL(":zz=1"));
    TestParseFails(StrL(":a!kjk"));
    TestParseFails(StrL(":c"));
    TestParseFails(StrL(":c="));
    TestParseFails(StrL(":nc"));
    return true;
}
