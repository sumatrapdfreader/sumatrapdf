/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct MarkdownHeadingItem {
    Str title;
    Str anchor;
    int level = 0;
};

struct MarkdownFileToc {
    Str filePath;
    Str relPath;
    Vec<MarkdownHeadingItem> headings;
};

void CollectMarkdownFiles(Str baseDir, Str openedFile, StrVec& filesOut);

void ParseMarkdownTocsParallel(StrVec& files, Vec<MarkdownFileToc>& tocsOut);

Str MarkdownToHtmlPage(Str markdown);

Str MarkdownHeadingSlug(Arena* a, Str title);