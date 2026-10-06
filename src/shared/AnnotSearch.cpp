/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "base/Base.h"

#include "Annotation.h"
#include "AnnotSearch.h"
#include "FilterUtil.h"

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

constexpr const char* kFilterWhitespace = " \t";

static Str ScanValue(Str& rest) {
    str::TrimAny(rest, kFilterWhitespace);
    int n = 0;
    while (n < len(rest) && rest.s[n] != ' ' && rest.s[n] != '\t') {
        n++;
    }
    Str value(rest.s, n);
    rest = Str(rest.s + n, len(rest) - n);
    return value;
}

// false if the text is not valid filter syntax; opts is then meaningless and
// the caller should fall back to matching the whole string as contents
bool ParseAnnotSearch(Str filter, AnnotMatchOpts& optsOut) {
    Str rest = filter;
    while (len(rest) > 0) {
        str::TrimAny(rest, kFilterWhitespace);
        if (len(rest) == 0) {
            break;
        }
        if (!str::TrimPrefix(rest, StrL(":"))) {
            AddCond(optsOut, AnnotMatchCond::Type::ContentMatches, ScanValue(rest), AnnotationType::Unknown);
            continue;
        }
        int nameLen = 0;
        while (nameLen < len(rest) && str::IsAlNum(rest.s[nameLen])) {
            nameLen++;
        }
        Str name(rest.s, nameLen);
        rest = Str(rest.s + nameLen, len(rest) - nameLen);
        if (str::EqI(name, StrL("c"))) {
            if (len(rest) == 0 || (rest.s[0] != '+' && rest.s[0] != '-')) {
                return false;
            }
            auto tp = rest.s[0] == '+' ? AnnotMatchCond::Type::HasContent : AnnotMatchCond::Type::NoContent;
            rest = Str(rest.s + 1, len(rest) - 1);
            AddCond(optsOut, tp, {}, AnnotationType::Unknown);
            continue;
        }
        bool isAuthor = str::EqI(name, StrL("a"));
        bool isType = str::EqI(name, StrL("t"));
        if (!isAuthor && !isType) {
            return false;
        }
        str::TrimAny(rest, kFilterWhitespace);
        bool isNot = str::TrimPrefix(rest, StrL("!=")) > 0;
        if (!isNot) {
            if (!str::TrimPrefix(rest, StrL("="))) {
                return false;
            }
            str::TrimPrefix(rest, StrL("="));
        }
        Str val = ScanValue(rest);
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
                if (FilterIndexOf(contents, c->s, nullptr) < 0) {
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
