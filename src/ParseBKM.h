/* Copyright 2018 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

void SerializeBookmarksRec(DocTocItem* node, int level, str::Str& s);
DocTocTree* ParseBookmarksFile(std::string_view path);
