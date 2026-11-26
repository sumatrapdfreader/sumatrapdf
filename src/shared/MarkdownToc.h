/* Copyright 2026 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#define SUMATRA_MARKDOWN_HOST "https://sumatrapdf.markdown/"
#define SUMATRA_MARKDOWN_PELE "pele.min.js"
#define SUMATRA_MARKDOWN_MERMAID "mermaid-run.js"
#define SUMATRA_MARKDOWN_HIGHLIGHT "microlighter.min.js"
#define SUMATRA_MARKDOWN_CODE "code-run.js"

struct MarkdownHeadingItem {
    Str title;
    Str anchor;
    int level = 0;
};

struct MarkdownFileToc {
    Str filePath;
    Vec<MarkdownHeadingItem>* headings; // owned; a pointer so Vec<MarkdownFileToc> stays memcpy-safe
};

// htmlMode collects/parses sibling .html/.htm files instead of .md/.markdown
void CollectMarkdownFiles(Str baseDir, Str openedFile, bool htmlMode, StrVec& filesOut);

void ParseMarkdownTocsParallel(StrVec& files, bool htmlMode, Vec<MarkdownFileToc>& tocsOut);

Str MarkdownToHtmlPage(Str markdown);
struct WebDocStyle;
TempStr BrowserTypographyCssTemp(const WebDocStyle&);
Str MermaidRunJs();
Str CodeRunJs();
