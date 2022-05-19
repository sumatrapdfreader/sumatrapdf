/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

bool DirTraverse(const char* dir, bool recurse, const std::function<bool(const char* path)>& cb);

bool CollectPathsFromDirectory(const char* pattern, StrVec& paths, bool dirsInsteadOfFiles = false);

bool CollectFilesFromDirectory(const char* dir, StrVec& files,
                               const std::function<bool(const char* path)>& fileMatches);
