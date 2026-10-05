/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct Annotation;
struct StrVec;

// One condition from the annotation filter box. Intrusive list: `next` is the
// first member so the List* helpers in base/Base.h can walk it.
struct AnnotMatchCond {
    AnnotMatchCond* next = nullptr;

    enum class Type {
        AuthorEqual,
        AuthorNotEqual,
        AnnotTypeEqual,
        AnnotTypeNotEqual,
        ContentMatches,
        HasContent,
        NoContent,
    };

    Type tp = Type::ContentMatches;
    AnnotationType annotType = AnnotationType::Unknown; // AnnotType* conditions
    Str s;                                              // the others

    ~AnnotMatchCond();
};

// A parsed filter. Owns its condition list.
struct AnnotMatchOpts {
    AnnotMatchCond* conds = nullptr;

    AnnotMatchOpts() = default;
    ~AnnotMatchOpts();
    void Reset();
    AnnotMatchOpts(const AnnotMatchOpts&) = delete;
    AnnotMatchOpts& operator=(const AnnotMatchOpts&) = delete;
};

bool ParseAnnotSearch(Str filter, AnnotMatchOpts& optsOut);
bool AnnotMatchesFields(Str author, Str contents, AnnotationType, const AnnotMatchOpts&);
bool AnnotMatches(Annotation*, const AnnotMatchOpts&);
void AnnotSearchContentWords(const AnnotMatchOpts&, StrVec& wordsOut);
void AnnotSearchAddContentWord(AnnotMatchOpts&, Str word);
