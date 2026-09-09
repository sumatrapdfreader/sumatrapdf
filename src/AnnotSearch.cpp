/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Annotation.h"
#include "AnnotSearch.h"

/*
The annotation filter box takes plain words plus `:` conditions:

    :a = kjk        author is kjk        (`=` and `==` mean the same)
    :a != kjk       author is not kjk
    :t = text       annotation is a Text
    :t != line      annotation is not a Line
    :c+             only annotations that have contents
    :c-             only annotations with no contents
    todo            contents contain "todo"

Spaces around the operator are optional; a value runs to the next space, so
authors with spaces in the name cannot be matched exactly (yet).

Repeating a condition on the same field ORs the `==`s and ANDs the `!=`s:
`:t=text :t=freetext` keeps both types, `:a!=kjk :a!=bob` drops both authors.
Everything else ANDs.
*/

AnnotMatchCond::~AnnotMatchCond() {
    str::Free(s);
}

AnnotMatchOpts::~AnnotMatchOpts() {
    ListDelete(conds);
}

void AnnotMatchOpts::Reset() {
    ListDelete(conds);
    conds = nullptr;
}

static void AddCond(AnnotMatchOpts& opts, AnnotMatchCond::Type tp, Str s, AnnotationType annotType) {
    auto* c = new AnnotMatchCond;
    c->tp = tp;
    c->annotType = annotType;
    c->s = str::Dup(s);
    // conditions are ANDed, so order does not matter; append anyway so the
    // list reads the way the user typed it
    ListInsertEnd(&opts.conds, c);
}

static bool IsFilterWs(char c) {
    return c == ' ' || c == '\t';
}

// the condition name right after ':' is letters only, so ":a=x" splits even
// without a space
static int ScanName(Str s, int i) {
    int start = i;
    while (i < s.len && str::IsAlNum(s.s[i])) {
        i++;
    }
    return i - start;
}

static void SkipWs(Str s, int& i) {
    while (i < s.len && IsFilterWs(s.s[i])) {
        i++;
    }
}

// "=", "==" or "!=" ; false if there is no operator here
static bool ScanOp(Str s, int& i, bool& isNotOut) {
    SkipWs(s, i);
    if (i < s.len && s.s[i] == '!') {
        if (i + 1 >= s.len || s.s[i + 1] != '=') {
            return false;
        }
        isNotOut = true;
        i += 2;
        return true;
    }
    if (i < s.len && s.s[i] == '=') {
        isNotOut = false;
        i++;
        if (i < s.len && s.s[i] == '=') {
            i++;
        }
        return true;
    }
    return false;
}

static Str ScanValue(Str s, int& i) {
    SkipWs(s, i);
    int start = i;
    while (i < s.len && !IsFilterWs(s.s[i])) {
        i++;
    }
    return Str(s.s + start, i - start);
}

// false if the text is not valid filter syntax; opts is then meaningless and
// the caller should fall back to matching the whole string as contents
bool ParseAnnotSearch(Str filter, AnnotMatchOpts& optsOut) {
    Str s = filter;
    int i = 0;
    while (i < s.len) {
        SkipWs(s, i);
        if (i >= s.len) {
            break;
        }
        if (s.s[i] != ':') {
            Str word = ScanValue(s, i);
            if (len(word) > 0) {
                AddCond(optsOut, AnnotMatchCond::Type::ContentMatches, word, AnnotationType::Unknown);
            }
            continue;
        }
        i++; // ':'
        int nameLen = ScanName(s, i);
        Str name = Str(s.s + i, nameLen);
        i += nameLen;
        if (str::EqI(name, StrL("c"))) {
            // ":c+" / ":c-", no operator and no value
            if (i >= s.len || (s.s[i] != '+' && s.s[i] != '-')) {
                return false;
            }
            auto tp = (s.s[i] == '+') ? AnnotMatchCond::Type::HasContent : AnnotMatchCond::Type::NoContent;
            i++;
            AddCond(optsOut, tp, {}, AnnotationType::Unknown);
            continue;
        }
        bool isAuthor = str::EqI(name, StrL("a"));
        bool isType = str::EqI(name, StrL("t"));
        if (!isAuthor && !isType) {
            return false;
        }
        bool isNot = false;
        if (!ScanOp(s, i, isNot)) {
            return false;
        }
        Str val = ScanValue(s, i);
        if (len(val) == 0) {
            return false;
        }
        if (isAuthor) {
            auto tp = isNot ? AnnotMatchCond::Type::AuthorNotEqual : AnnotMatchCond::Type::AuthorEqual;
            AddCond(optsOut, tp, val, AnnotationType::Unknown);
            continue;
        }
        AnnotationType annotType = AnnotationTypeFromName(val);
        if (annotType == AnnotationType::Unknown) {
            return false;
        }
        auto tp = isNot ? AnnotMatchCond::Type::AnnotTypeNotEqual : AnnotMatchCond::Type::AnnotTypeEqual;
        AddCond(optsOut, tp, {}, annotType);
    }
    return true;
}

bool AnnotMatchesFields(Str author, Str contents, AnnotationType annotType, const AnnotMatchOpts& opts) {
    // the positive conditions on one field are alternatives; "no such condition"
    // and "one of them matched" both pass
    bool wantAuthor = false, sawAuthor = false;
    bool wantType = false, sawType = false;
    for (AnnotMatchCond* c = opts.conds; c; c = c->next) {
        switch (c->tp) {
            case AnnotMatchCond::Type::AuthorEqual:
                wantAuthor = true;
                sawAuthor = sawAuthor || str::EqI(author, c->s);
                break;
            case AnnotMatchCond::Type::AuthorNotEqual:
                if (str::EqI(author, c->s)) {
                    return false;
                }
                break;
            case AnnotMatchCond::Type::AnnotTypeEqual:
                wantType = true;
                sawType = sawType || (annotType == c->annotType);
                break;
            case AnnotMatchCond::Type::AnnotTypeNotEqual:
                if (annotType == c->annotType) {
                    return false;
                }
                break;
            case AnnotMatchCond::Type::ContentMatches:
                if (!str::ContainsI(contents, c->s)) {
                    return false;
                }
                break;
            case AnnotMatchCond::Type::HasContent:
                if (len(contents) == 0) {
                    return false;
                }
                break;
            case AnnotMatchCond::Type::NoContent:
                if (len(contents) > 0) {
                    return false;
                }
                break;
        }
    }
    if (wantAuthor && !sawAuthor) {
        return false;
    }
    if (wantType && !sawType) {
        return false;
    }
    return true;
}

bool AnnotMatches(Annotation* annot, const AnnotMatchOpts& opts) {
    if (!annot) {
        return false;
    }
    return AnnotMatchesFields(Author(annot), Contents(annot), Type(annot), opts);
}

void AnnotSearchAddContentWord(AnnotMatchOpts& opts, Str word) {
    if (len(word) > 0) {
        AddCond(opts, AnnotMatchCond::Type::ContentMatches, word, AnnotationType::Unknown);
    }
}

// the words to highlight in the list: the `:` conditions are syntax, not text
// the user is looking for
void AnnotSearchContentWords(const AnnotMatchOpts& opts, StrVec& wordsOut) {
    for (AnnotMatchCond* c = opts.conds; c; c = c->next) {
        if (c->tp == AnnotMatchCond::Type::ContentMatches) {
            AppendIfNotExists(&wordsOut, c->s);
        }
    }
}
