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

// Collect .md / .markdown files under baseDir, at most two subdirectory levels deep.
void CollectMarkdownFiles(Str baseDir, Str openedFile, StrVec& filesOut);

// Parse headings from all files in parallel (up to CpuCoreCount() - 2 threads).
void ParseMarkdownTocsParallel(StrVec& files, Vec<MarkdownFileToc>& tocsOut);

// Convert markdown source to a full HTML page (body only is rendered with cmark-gfm).
Str MarkdownToHtmlPage(Str markdown);

// Slug for a heading title, matching cmark-gfm autoheaderid. Pass arena to allocate there.
Str MarkdownHeadingSlug(Arena* a, Str title);