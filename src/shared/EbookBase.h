/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

// this is a collection of structs and classes that are
// useful for more than one ebook format

struct EbookTocVisitor {
  public:
    virtual void Visit(Str name, Str url, int level) = 0;
    virtual ~EbookTocVisitor() = default;
};
